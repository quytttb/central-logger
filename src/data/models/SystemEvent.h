#pragma once

#include "utils/SensorConstants.h"

#include <QDateTime>
#include <QString>
#include <optional>

namespace CentralLogger::Data {

struct SystemEvent
{
    qint64                id = 0;
    std::optional<qint64> loggerId;                 // empty for app-wide events
    QString               eventType;                // Alarm|Offline|Online|Warning|Info
    QString               message;
    QString               level = Sensor::kLevelInfo; // critical|warning|error|info
    QDateTime             createdAt;
};

} // namespace CentralLogger::Data
