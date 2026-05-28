#pragma once

#include "data/models/LoggerInfo.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

namespace CentralLogger::Data {

/// `LoggerInfo` plus a denormalized count of catalog rows owned by the
/// logger. Returned by `findAllWithSensorCounts()` so list models can render
/// the sensor count without an N+1 query per row.
struct LoggerListRow
{
    LoggerInfo info;
    int        sensorCount = 0;
};

class LoggerRepository
{
public:
    explicit LoggerRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    /// Inserts a row; on success sets @p info.id and @p info.createdAt.
    /// Returns false on UNIQUE(station_code) violation or any SQL error.
    bool insert(LoggerInfo &info, QString *errorOut = nullptr);

    std::optional<LoggerInfo> findById(qint64 id, QString *errorOut = nullptr) const;
    std::optional<LoggerInfo> findByStationCode(const QString &stationCode,
                                                QString *errorOut = nullptr) const;

    QVector<LoggerInfo> findAll(QString *errorOut = nullptr) const;

    /// Joins `logger_sensor` so each row carries its catalog count.
    QVector<LoggerListRow> findAllWithSensorCounts(QString *errorOut = nullptr) const;

    bool update(const LoggerInfo &info, QString *errorOut = nullptr);

    /// Updates only the live-state columns. Used by ModbusBridge on success.
    bool updateStatusAndLastSeen(qint64 id,
                                 const QString &status,
                                 const QDateTime &lastSeenUtc,
                                 QString *errorOut = nullptr);

    /// Updates only the status column, leaving last_seen unchanged.
    /// Used by ModbusBridge on poll failure so last_seen is not cleared.
    bool updateStatus(qint64 id, const QString &status, QString *errorOut = nullptr);

    bool remove(qint64 id, QString *errorOut = nullptr);

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
