#pragma once

#include <QDateTime>

namespace CentralLogger::Data {

struct SensorReading
{
    qint64    id = 0;
    qint64    sensorId = 0;
    double    value = 0.0;
    bool      valid = true;
    bool      alarm = false;
    bool      stale = false;
    qint64    loggerTimestamp = 0;   // Unix seconds, Modbus HR2-3
    QDateTime recordedAt;            // UTC
};

} // namespace CentralLogger::Data
