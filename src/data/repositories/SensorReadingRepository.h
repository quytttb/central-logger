#pragma once

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
    /// a transaction on @p m_db (e.g. ModbusBridge::applySnapshot).
    bool insertBatch(const QVector<SensorReading> &readings,
                     QString *errorOut = nullptr,
                     bool manageTransaction = true);

    /// Returns the number of rows deleted (or -1 on error).
    int purgeOlderThan(const QDateTime &cutoffUtc, QString *errorOut = nullptr);

    /// Convenience helper for tests / status panels.
    int countForSensor(qint64 sensorId, QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
