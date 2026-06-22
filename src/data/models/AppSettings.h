#pragma once

#include <QString>

namespace CentralLogger::Data {

struct AppSettings
{
    QString theme = QStringLiteral("dark");
    QString systemTimezone = QStringLiteral("Asia/Ho_Chi_Minh");
    int     dataRetentionDays = 30;
    int     historyFlushIntervalS = 5;
};

} // namespace CentralLogger::Data
