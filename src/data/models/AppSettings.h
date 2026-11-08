#pragma once

#include "utils/AppConstants.h"
#include "utils/UiConstants.h"

#include <QString>

namespace CentralLogger::Data {

struct AppSettings
{
    QString theme = Ui::kThemeDark;
    QString systemTimezone = Ui::kDefaultTimezone;
    int     dataRetentionDays = Defaults::kDefaultRetentionDays;
    int     historyFlushIntervalS = Defaults::kHistoryFlushIntervalS;
    /// Dashboard summary chart logger; 0 = not configured.
    qint64  primaryLoggerId = Defaults::kNoPrimaryLoggerId;
};

} // namespace CentralLogger::Data
