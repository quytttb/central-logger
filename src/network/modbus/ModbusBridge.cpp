#include "ModbusBridge.h"

#include "data/db/Database.h"
#include "data/models/SensorReading.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QtDebug>

namespace CentralLogger::Network {

ModbusBridge::ModbusBridge(QObject *parent)
    : QObject(parent)
{
}

void ModbusBridge::setConnection(QSqlDatabase db)
{
    m_standaloneConn = std::move(db);
    m_db             = nullptr;
}

QSqlDatabase ModbusBridge::sqlConnection() const
{
    if (m_db && m_db->isOpen()) {
        return m_db->connection();
    }
    return m_standaloneConn;
}

void ModbusBridge::applyLiveSnapshot(const PollSnapshot &snapshot)
{
    QSqlDatabase db = sqlConnection();
    if (!db.isValid() || !db.isOpen()) {
        return;
    }

    Data::LoggerRepository        loggers(db);
    Data::SensorCatalogRepository catalog(db);

    const QDateTime now = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt
        : QDateTime::currentDateTimeUtc();

    if (!snapshot.success) {
        loggers.updateStatus(snapshot.loggerId, QStringLiteral("offline"));
        const int sensorCount = catalog.listByLoggerId(snapshot.loggerId).size();
        emit snapshotApplied(snapshot, sensorCount);
        return;
    }

    const bool inTransaction = db.transaction();
    if (!inTransaction) {
        qWarning() << "ModbusBridge: cannot begin applyLiveSnapshot transaction:"
                   << db.lastError().text();
    }

    loggers.updateStatusAndLastSeen(snapshot.loggerId,
                                    QStringLiteral("online"),
                                    now);

    for (const auto &sample : snapshot.analogs) {
        catalog.ensureExists(snapshot.loggerId, sample.edgeSensorId,
                             QStringLiteral("ANALOG"));
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
        }
    }

    if (inTransaction) {
        if (!db.commit()) {
            qWarning() << "ModbusBridge: applyLiveSnapshot commit failed:"
                       << db.lastError().text();
            db.rollback();
        }
    }

    const int sensorCount = catalog.listByLoggerId(snapshot.loggerId).size();
    emit snapshotApplied(snapshot, sensorCount);
}

QVector<Data::SensorReading> ModbusBridge::buildReadings(const PollSnapshot &snapshot) const
{
    QVector<Data::SensorReading> batch;
    if (!snapshot.success) {
        return batch;
    }

    const QSqlDatabase db = sqlConnection();
    if (!db.isValid() || !db.isOpen()) {
        return batch;
    }

    Data::SensorCatalogRepository catalog(db);

    const QDateTime now = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt
        : QDateTime::currentDateTimeUtc();

    batch.reserve(snapshot.analogs.size()
                  + snapshot.diBits.size()
                  + snapshot.doBits.size());

    auto appendReading = [&](qint64 sensorId, double value,
                             bool valid, bool alarm, bool stale)
    {
        Data::SensorReading r;
        r.sensorId        = sensorId;
        r.value           = value;
        r.valid           = valid;
        r.alarm           = alarm;
        r.stale           = stale;
        r.loggerTimestamp = snapshot.header.unixTimestamp;
        r.recordedAt      = now;
        batch.append(r);
    };

    for (const auto &sample : snapshot.analogs) {
        const qint64 sensorId = catalog.ensureExists(snapshot.loggerId, sample.edgeSensorId,
                                                     QStringLiteral("ANALOG"));
        if (sensorId <= 0) {
            continue;
        }
        appendReading(sensorId, static_cast<double>(sample.value),
                      sample.isValid(), sample.isAlarm(), sample.isStale());
    }

    const auto catalogRows = catalog.listByLoggerId(snapshot.loggerId);
    for (const auto &sensor : catalogRows) {
        if (!sensor.active || sensor.id <= 0) {
            continue;
        }
        if (sensor.sensorType == QStringLiteral("DI")) {
            const int bit = sensor.edgeSensorId;
            if (bit < 0 || bit >= snapshot.diBits.size()) {
                continue;
            }
            appendReading(sensor.id, snapshot.diBits.at(bit) ? 1.0 : 0.0,
                          true, false, false);
        } else if (sensor.sensorType == QStringLiteral("DO")) {
            const int bit = sensor.edgeSensorId;
            if (bit < 0 || bit >= snapshot.doBits.size()) {
                continue;
            }
            appendReading(sensor.id, snapshot.doBits.at(bit) ? 1.0 : 0.0,
                          true, false, false);
        }
    }

    return batch;
}

void ModbusBridge::applyBatch(const QList<PollSnapshot> &batch)
{
    if (batch.isEmpty()) {
        return;
    }

    const QSqlDatabase db = sqlConnection();
    if (!db.isValid() || !db.isOpen()) {
        return;
    }

    QVector<Data::SensorReading> readings;
    for (const PollSnapshot &snapshot : batch) {
        if (!snapshot.success) {
            continue;
        }
        readings += buildReadings(snapshot);
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
