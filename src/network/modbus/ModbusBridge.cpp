#include "ModbusBridge.h"

#include "data/db/Database.h"
#include "data/models/LoggerSensor.h"
#include "data/models/SensorReading.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "utils/DbConstants.h"
#include "utils/SensorConstants.h"

#include <QDateTime>
#include <QSqlError>
#include <QThread>
#include <QtDebug>

namespace CentralLogger::Network {

ModbusBridge::ModbusBridge(QObject *parent)
    : QObject(parent)
{
}

void ModbusBridge::start()
{
    // Audit H-A: when the bridge lives on its own thread, open a dedicated
    // connection here (QSqlDatabase handles are thread-bound).
    if (!m_databasePath.isEmpty()) {
        const QString connName =
            QStringLiteral("modbus_live_%1")
                .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        m_dedicatedConn = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), connName);
        m_dedicatedConn.setDatabaseName(m_databasePath);
        if (!m_dedicatedConn.open()) {
            qWarning() << "ModbusBridge: cannot open dedicated connection:"
                       << m_dedicatedConn.lastError().text();
            m_dedicatedConn = QSqlDatabase();
            QSqlDatabase::removeDatabase(connName);
            return;
        }
        QString pragmaErr;
        if (!Data::Database::applyPerformancePragmas(m_dedicatedConn, &pragmaErr)) {
            qWarning() << "ModbusBridge: performance pragmas failed:" << pragmaErr;
        }
    }
}

void ModbusBridge::shutdown()
{
    m_catalogCache.clear();
    if (m_dedicatedConn.isValid()) {
        const QString connName = m_dedicatedConn.connectionName();
        if (m_dedicatedConn.isOpen()) {
            m_dedicatedConn.close();
        }
        m_dedicatedConn = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
    }
}

void ModbusBridge::invalidateCatalogCache(qint64 loggerId)
{
    if (loggerId <= 0) {
        m_catalogCache.clear();
    } else {
        m_catalogCache.remove(loggerId);
    }
}

ModbusBridge::CatalogCacheEntry &ModbusBridge::catalogCacheFor(qint64 loggerId,
                                                               QSqlDatabase &db)
{
    auto it = m_catalogCache.find(loggerId);
    if (it != m_catalogCache.end()) {
        return *it;
    }

    // Audit M-2: build the cache once per logger from one catalog SELECT
    // instead of doing UPSERT+SELECT per sample on every poll.
    CatalogCacheEntry entry;
    Data::SensorCatalogRepository catalog(db);
    const auto rows = catalog.listByLoggerId(loggerId);
    for (const auto &s : rows) {
        if (!s.active || s.id <= 0) {
            continue;
        }
        if (s.sensorType == CentralLogger::Sensor::kTypeAnalog) {
            entry.analogIds.insert(s.edgeSensorId, s.id);
        } else if (s.sensorType == CentralLogger::Sensor::kTypeDi) {
            entry.diIds.insert(s.edgeSensorId, s.id);
        } else if (s.sensorType == CentralLogger::Sensor::kTypeDo) {
            entry.doIds.insert(s.edgeSensorId, s.id);
        }
    }
    return *m_catalogCache.insert(loggerId, std::move(entry));
}

void ModbusBridge::applyLiveSnapshot(const PollSnapshot &snapshot)
{
    QSqlDatabase db = m_dedicatedConn;
    if (!db.isValid() || !db.isOpen()) {
        return;
    }

    Data::LoggerRepository        loggers(db);
    const auto loggerInfo = loggers.findById(snapshot.loggerId);
    if (!loggerInfo || !loggerInfo->enabled) {
        return;
    }

    Data::SensorCatalogRepository catalog(db);

    const QDateTime now = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt
        : QDateTime::currentDateTimeUtc();

    if (!snapshot.success) {
        loggers.updateStatus(snapshot.loggerId, CentralLogger::Sensor::kLoggerOffline);
        const int sensorCount = catalog.listByLoggerId(snapshot.loggerId).size();
        emit snapshotApplied(snapshot, sensorCount,
                             QVector<Data::LoggerSensor>{});
        return;
    }

    const bool inTransaction = db.transaction();
    if (!inTransaction) {
        qWarning() << "ModbusBridge: cannot begin applyLiveSnapshot transaction:"
                   << db.lastError().text();
    }

    loggers.updateStatusAndLastSeen(snapshot.loggerId,
                                    CentralLogger::Sensor::kLoggerOnline,
                                    now);

    // Audit M-2: ensureExists is only needed for sensors the cache does not
    // know yet (newly wired sensors). Cache hits skip the UPSERT+SELECT.
    {
        auto &cached = catalogCacheFor(snapshot.loggerId, db);
        for (const auto &sample : snapshot.analogs) {
            if (cached.analogIds.contains(sample.edgeSensorId)) {
                continue;
            }
            const qint64 id = catalog.ensureExists(snapshot.loggerId,
                                                   sample.edgeSensorId,
                                                   CentralLogger::Sensor::kTypeAnalog);
            if (id > 0) {
                cached.analogIds.insert(sample.edgeSensorId, id);
            }
        }
    }

    {
        QVector<int> liveAnalogEdgeIds;
        if (snapshot.header.na > 0) {
            liveAnalogEdgeIds.reserve(snapshot.analogs.size());
            for (const auto &sample : snapshot.analogs) {
                liveAnalogEdgeIds.append(sample.edgeSensorId);
            }
        }

        QString pruneErr;
        const int pruned = catalog.pruneOrphanSensors(
            snapshot.loggerId,
            liveAnalogEdgeIds,
            static_cast<int>(snapshot.header.ndi),
            static_cast<int>(snapshot.header.ndo),
            &pruneErr);
        if (pruned < 0) {
            qWarning() << "ModbusBridge: pruneOrphanSensors failed:" << pruneErr;
        } else if (pruned > 0) {
            // Deactivated rows may be cached — drop the entry so the next
            // snapshot rebuilds it from the current catalog.
            m_catalogCache.remove(snapshot.loggerId);
        }
    }

    if (inTransaction) {
        if (!db.commit()) {
            qWarning() << "ModbusBridge: applyLiveSnapshot commit failed:"
                       << db.lastError().text();
            db.rollback();
        }
    }

    const auto rows = catalog.listByLoggerId(snapshot.loggerId);
    emit snapshotApplied(snapshot, rows.size(), rows);
}

QVector<Data::SensorReading> ModbusBridge::buildReadings(const PollSnapshot &snapshot,
                                                          QSqlDatabase db)
{
    QVector<Data::SensorReading> batch;
    if (!snapshot.success) {
        return batch;
    }

    if (!db.isValid() || !db.isOpen()) {
        return batch;
    }

    Data::LoggerRepository loggers(db);
    const auto loggerInfo = loggers.findById(snapshot.loggerId);
    if (!loggerInfo || !loggerInfo->enabled) {
        return batch;
    }

    Data::SensorCatalogRepository catalog(db);

    const QDateTime now = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt
        : QDateTime::currentDateTimeUtc();
    const qint64 nowMs = now.toMSecsSinceEpoch();

    batch.reserve(snapshot.analogs.size()
                   + snapshot.diBits.size()
                   + snapshot.doBits.size());

    // H-C (store-on-change): write a reading only when the value or the
    // TT10 status code changed, or once every kHeartbeatMs even when
    // unchanged (charts keep showing continuity). Cuts write volume by 1–2
    // orders of magnitude at steady state — see docs/adr/0002-store-on-change.md.
    auto appendIfChanged = [&](qint64 sensorId, double value,
                               const QString &statusCode)
    {
        const auto valIt = m_lastValue.constFind(sensorId);
        const bool firstSeen = valIt == m_lastValue.constEnd();
        const bool valueChanged = firstSeen || (*valIt != value);

        const auto flgIt = m_lastStatusCode.constFind(sensorId);
        const bool statusChanged = flgIt == m_lastStatusCode.constEnd()
                                   || *flgIt != statusCode;

        const auto tsIt = m_lastWrittenMs.constFind(sensorId);
        const bool heartbeatDue = tsIt == m_lastWrittenMs.constEnd()
                                  || nowMs - *tsIt >= kHeartbeatMs;

        if (!valueChanged && !statusChanged && !heartbeatDue) {
            return;
        }

        Data::SensorReading r;
        r.sensorId        = sensorId;
        r.value           = value;
        r.statusCode      = statusCode;
        r.loggerTimestamp = snapshot.header.unixTimestamp;
        r.recordedAt      = now;
        batch.append(r);

        m_lastValue.insert(sensorId, value);
        m_lastStatusCode.insert(sensorId, statusCode);
        m_lastWrittenMs.insert(sensorId, nowMs);
    };

    // Reuse the per-instance catalog cache populated by applyLiveSnapshot on
    // the bridge thread (and by this thread's own first miss). Without the
    // cache we would UPSERT+SELECT for every analog sample on every poll —
    // 2 SQL statements per Na per snapshot — which is exactly what the M-2
    // audit fixed for the live pipeline. The writer-side bridge keeps its
    // own cache, so no cross-thread sharing.
    auto &cache = catalogCacheFor(snapshot.loggerId, db);
    for (const auto &sample : snapshot.analogs) {
        qint64 sensorId = cache.analogIds.value(sample.edgeSensorId, 0);
        if (sensorId <= 0) {
            sensorId = catalog.ensureExists(snapshot.loggerId, sample.edgeSensorId,
                                            CentralLogger::Sensor::kTypeAnalog);
            if (sensorId > 0) {
                cache.analogIds.insert(sample.edgeSensorId, sensorId);
            }
        }
        if (sensorId <= 0) {
            continue;
        }
        // The v7 history schema has no separate quality column. Do not store
        // the transport's zero placeholder as a real value for an invalid v2
        // sample, even if the source supplied an error status.
        if (sample.hasQuality && (!sample.isValid() || sample.isStale())) {
            m_lastValue.remove(sensorId);
            continue;
        }
        appendIfChanged(sensorId, static_cast<double>(sample.value),
                        sample.statusCode);
    }

    for (auto it = cache.diIds.constBegin(); it != cache.diIds.constEnd(); ++it) {
        const int bit = it.key();
        if (bit < 0 || bit >= snapshot.diBits.size()) {
            continue;
        }
        appendIfChanged(it.value(), snapshot.diBits.at(bit) ? 1.0 : 0.0,
                        QLatin1String(CentralLogger::Sensor::kStatusOk));
    }
    for (auto it = cache.doIds.constBegin(); it != cache.doIds.constEnd(); ++it) {
        const int bit = it.key();
        if (bit < 0 || bit >= snapshot.doBits.size()) {
            continue;
        }
        appendIfChanged(it.value(), snapshot.doBits.at(bit) ? 1.0 : 0.0,
                        QLatin1String(CentralLogger::Sensor::kStatusOk));
    }

    return batch;
}

void ModbusBridge::applyBatch(const QList<PollSnapshot> &batch, QSqlDatabase db)
{
    if (batch.isEmpty()) {
        return;
    }

    if (!db.isValid() || !db.isOpen()) {
        qWarning() << "ModbusBridge::applyBatch: invalid or closed database connection";
        return;
    }

    QVector<Data::SensorReading> readings;
    for (const PollSnapshot &snapshot : batch) {
        if (!snapshot.success) {
            continue;
        }
        readings += buildReadings(snapshot, db);
    }

    if (readings.isEmpty()) {
        return;
    }

    Data::SensorReadingRepository repo(db);
    QString err;
    if (!repo.insertBatch(readings, &err, /*manageTransaction*/ true)) {
        qWarning() << "ModbusBridge: applyBatch insertBatch failed:" << err;
    }
}

} // namespace CentralLogger::Network
