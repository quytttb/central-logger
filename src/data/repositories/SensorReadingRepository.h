#pragma once

#include "data/models/HistoryRow.h"
#include "data/models/SensorReading.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace CentralLogger::Data {

class SensorReadingRepository
{
public:
    explicit SensorReadingRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    /// Inserts all readings. When @p manageTransaction is true (default), wraps
    /// inserts in a local transaction. Set false when the caller already began
    /// a transaction on @p m_db (e.g. ModbusBridge::applyBatch).
    bool insertBatch(const QVector<SensorReading> &readings,
                     QString *errorOut = nullptr,
                     bool manageTransaction = true);

    /// Returns the number of rows deleted (or -1 on error).
    int purgeOlderThan(const QDateTime &cutoffUtc, QString *errorOut = nullptr);

    /// Convenience helper for tests / status panels.
    int countForSensor(qint64 sensorId, QString *errorOut = nullptr) const;

    /// Queries sensor_reading joined with logger_sensor and logger_info.
    /// @p loggerId == 0 means all loggers; @p sensorId == 0 means all sensors.
    /// Results are sorted by recorded_at DESC and capped at @p limit.
    QVector<HistoryRow> searchHistory(qint64 loggerId,
                                      const QDateTime &fromUtc,
                                      const QDateTime &toUtc,
                                      qint64 sensorId = 0,
                                      int limit = 5000,
                                      QString *errorOut = nullptr) const;

    /// Same filters as searchHistory — total row count without LIMIT.
    int countHistory(qint64 loggerId,
                     const QDateTime &fromUtc,
                     const QDateTime &toUtc,
                     qint64 sensorId = 0,
                     QString *errorOut = nullptr) const;

    /// Returns unique (sensor id, display label) pairs with at least one reading.
    /// @p loggerId == 0 lists sensors across all loggers (label: "logger — sensor").
    QVector<QPair<qint64, QString>> sensorsWithReadings(qint64 loggerId,
                                                        QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
