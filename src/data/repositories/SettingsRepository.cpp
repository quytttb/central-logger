#include "SettingsRepository.h"

#include "SqlHelper.h"
#include "utils/AppConstants.h"
#include "utils/DbConstants.h"
#include "utils/UiConstants.h"

#include <QSqlQuery>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Defaults::kHistoryFlushIntervalS;

} // namespace

AppSettings SettingsRepository::get(QString *errorOut) const
{
    AppSettings result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT theme, system_timezone, data_retention_days, history_flush_interval_s, "
            "       primary_logger_id, loggers_view_mode "
            "FROM %1 WHERE id = 1")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableAppSettings)))) {
        setSqlError(errorOut, q);
        return result;
    }
    if (!q.next()) {
        return result;
    }
    result.theme             = q.value(0).toString();
    result.systemTimezone    = q.value(1).toString();
    result.dataRetentionDays = q.value(2).toInt();
    result.historyFlushIntervalS = q.value(3).toInt();
    result.primaryLoggerId   = q.value(4).toLongLong();
    const QString viewMode   = q.value(5).toString().trimmed().toLower();
    result.loggersViewMode   = (viewMode == QLatin1String(CentralLogger::Ui::kLoggersViewList))
        ? QLatin1String(CentralLogger::Ui::kLoggersViewList)
        : QLatin1String(CentralLogger::Ui::kLoggersViewCards);
    if (result.historyFlushIntervalS <= 0) {
        result.historyFlushIntervalS = kHistoryFlushIntervalS;
    }
    return result;
}

bool SettingsRepository::update(const AppSettings &settings, QString *errorOut)
{
    const QString normalizedViewMode =
        (settings.loggersViewMode.trimmed().toLower() == QLatin1String(CentralLogger::Ui::kLoggersViewList))
            ? QLatin1String(CentralLogger::Ui::kLoggersViewList)
            : QLatin1String(CentralLogger::Ui::kLoggersViewCards);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE %1 SET "
        "  theme = :theme,"
        "  system_timezone = :tz,"
        "  data_retention_days = :retention,"
        "  history_flush_interval_s = :history_flush,"
        "  primary_logger_id = :primary_logger,"
        "  loggers_view_mode = :loggers_view_mode "
        "WHERE id = 1")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableAppSettings)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTheme),           settings.theme);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTz),              settings.systemTimezone);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindRetention),       settings.dataRetentionDays);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindHistoryFlush),    settings.historyFlushIntervalS);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindPrimaryLogger),   settings.primaryLoggerId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggersViewMode), normalizedViewMode);
    if (!q.exec()) {
        setSqlError(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace CentralLogger::Data
