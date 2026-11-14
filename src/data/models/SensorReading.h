#pragma once

#include <QDateTime>

namespace CentralLogger::Data {

struct SensorReading
{
    qint64    id = 0;
    qint64    sensorId = 0;
    double    value = 0.0;
    QString   statusCode; // TT10: "00"|"01"|"02"|"03", empty = no data/stale
    qint64    loggerTimestamp = 0;   // Unix seconds, Modbus HR2-3
    QDateTime recordedAt;            // UTC
};

} // namespace CentralLogger::Data
