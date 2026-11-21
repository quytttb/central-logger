#pragma once

#include "utils/AppConstants.h"

#include <QDateTime>
#include <QString>

namespace CentralLogger::Data {

/// One row returned by SensorReadingRepository::searchHistory().
/// Combines sensor_reading data with logger_sensor metadata.
struct HistoryRow
{
    QDateTime recordedAt;          ///< UTC
    QString   loggerName;          ///< from logger_info.name
    QString   sensorName;
    QString   unit;
    double    value = 0.0;
    QString   statusCode; // TT10 status of the reading
    int       decimals = Defaults::kDecimalsDefault; ///< display precision from logger_sensor.decimals
    qint64    readingId = 0;       ///< stable identity, even when timestamps match

    bool operator==(const HistoryRow &) const = default;
};

} // namespace CentralLogger::Data
