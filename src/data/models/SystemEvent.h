#pragma once

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
    QString               level = QStringLiteral("info"); // critical|warning|error|info
    QDateTime             createdAt;
};

} // namespace CentralLogger::Data
