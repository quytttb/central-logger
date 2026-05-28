#pragma once

#include "data/models/LoggerSensor.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace CentralLogger::Data {

class SensorCatalogRepository
{
public:
    explicit SensorCatalogRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    /// Modbus auto-create path: inserts a sensor row typed @p sensorType when
    /// the (loggerId, sensorType, edgeSensorId) triple is unseen. Returns the
    /// row id (existing or newly inserted). Returns 0 on error.
    qint64 ensureExists(qint64 loggerId, int edgeSensorId, const QString &sensorType,
                        QString *errorOut = nullptr);

    /// REST catalog path: insert-or-update the metadata. Uses
    /// UNIQUE(logger_id, sensor_type, edge_sensor_id) as the conflict target.
    /// On success @p sensor.id is set to the row id.
    bool upsert(LoggerSensor &sensor, QString *errorOut = nullptr);

    std::optional<LoggerSensor> findByLoggerAndEdgeId(qint64 loggerId,
                                                      int edgeSensorId,
                                                      const QString &sensorType,
                                                      QString *errorOut = nullptr) const;

    QVector<LoggerSensor> listByLoggerId(qint64 loggerId, QString *errorOut = nullptr) const;

    /// Deactivates catalog rows no longer exposed on the wire. Call after every
    /// successful Modbus poll (modbus-map-v1).
    ///
    /// ANALOG: @p liveAnalogEdgeIds lists `sensor_id` values from FC03 blocks.
    /// Deactivates ANALOG rows whose edge_sensor_id is **not** in that set.
    /// Pass an empty list to skip ANALOG pruning (e.g. when Na = 0).
    ///
    /// DI / DO: bit address equals edge_sensor_id. Deactivates rows with
    /// edge_sensor_id >= @p maxDi or @p maxDo. Pass -1 to skip a digital type;
    /// when max is 0 (Ndi/Ndo = 0, no FC02/FC01) that type is also skipped.
    ///
    /// Sets active=0 only — preserves sensor_reading history.
    /// Returns the total number of deactivated rows, or -1 on error.
    int pruneOrphanSensors(qint64 loggerId,
                           const QVector<int> &liveAnalogEdgeIds,
                           int maxDi,
                           int maxDo,
                           QString *errorOut = nullptr);

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
