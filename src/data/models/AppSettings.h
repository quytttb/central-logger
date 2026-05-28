#pragma once

#include <QString>

namespace CentralLogger::Data {

struct AppSettings
{
    QString theme = QStringLiteral("dark");
    QString systemTimezone = QStringLiteral("Asia/Ho_Chi_Minh");
    int     dataRetentionDays = 30;
    bool    maintenanceMode = false;
};

} // namespace CentralLogger::Data
