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

void ModbusBridge::applySnapshot(const PollSnapshot &snapshot)
{
    if (!m_db || !m_db->isOpen()) {
        return;
    }

    Data::LoggerRepository         loggers(m_db->connection());
    Data::SensorCatalogRepository  catalog(m_db->connection());
    Data::SensorReadingRepository  readings(m_db->connection());

    const QDateTime now = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt
        : QDateTime::currentDateTimeUtc();

    if (!snapshot.success) {
        // Only update status; preserve last_seen so the UI still shows when
        // the logger was last contactable.
        loggers.updateStatus(snapshot.loggerId, QStringLiteral("offline"));
        const int sensorCount = catalog.listByLoggerId(snapshot.loggerId).size();
        emit snapshotApplied(snapshot, sensorCount);
        return;
    }

    // M-4 fix: wrap the multi-step write (status, ensureExists, insertBatch,
    // prune) in a single transaction so a partial failure doesn't leave the
    // database in an inconsistent state.
    QSqlDatabase db = m_db->connection();
    const bool inTransaction = db.transaction();
    if (!inTransaction) {
        qWarning() << "ModbusBridge: cannot begin applySnapshot transaction:"
                   << db.lastError().text();
        // Proceed without transaction — better than dropping the snapshot.
    }

    loggers.updateStatusAndLastSeen(snapshot.loggerId,
                                    QStringLiteral("online"),
                                    now);

    QVector<Data::SensorReading> batch;
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
        if (sensorId <= 0) continue;
        appendReading(sensorId, static_cast<double>(sample.value),
                      sample.isValid(), sample.isAlarm(), sample.isStale());
    }

    // Persist DI/DO only for catalog rows (from GET /config). Do not auto-create
    // logger_sensor rows for every FC bit 0..Ndi-1 — that produced phantom DI#0..7.
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

    if (!batch.isEmpty()) {
        QString err;
        if (!readings.insertBatch(batch, &err, /*manageTransaction*/ !inTransaction)) {
            qWarning() << "ModbusBridge: insertBatch failed:" << err;
        }
    }

    // Deactivate catalog rows no longer on the wire (modbus-map-v1).
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
            qWarning() << "ModbusBridge: applySnapshot commit failed:"
                       << db.lastError().text();
            db.rollback();
        }
    }

    const int sensorCount = catalog.listByLoggerId(snapshot.loggerId).size();
    emit snapshotApplied(snapshot, sensorCount);
}

} // namespace CentralLogger::Network
