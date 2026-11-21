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
    /// Default graph logger; 0 = not configured.
    qint64  primaryLoggerId = Defaults::kNoPrimaryLoggerId;
    /// Persisted Loggers > Data presentation ('cards' | 'list').
    QString loggersViewMode = QLatin1String(Ui::kLoggersViewCards);
};

} // namespace CentralLogger::Data
