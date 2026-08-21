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
};

} // namespace CentralLogger::Data
