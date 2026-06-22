#pragma once

#include <QDateTime>
#include <QString>

namespace CentralLogger::Data {

/// One row returned by SensorReadingRepository::searchHistory().
/// Combines sensor_reading data with logger_sensor metadata.
struct HistoryRow
{
    qint64    id = 0;
    QDateTime recordedAt;          ///< UTC
    QString   loggerName;          ///< from logger_info.name
    QString   sensorName;
    QString   unit;
    double    value = 0.0;
    int       decimals = 4;        ///< display precision from logger_sensor.decimals
    bool      valid = true;
    bool      alarm = false;
    bool      stale = false;
    qint64    sensorId = 0;
};

} // namespace CentralLogger::Data
