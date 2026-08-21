#include "SettingsRepository.h"

#include "utils/AppConstants.h"
#include "utils/DbConstants.h"

#include <QSqlError>
#include <QSqlQuery>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Defaults::kHistoryFlushIntervalS;

void setErr(QString *out, const QSqlQuery &q)
{
    if (out) {
        *out = q.lastError().text();
    }
}

} // namespace

AppSettings SettingsRepository::get(QString *errorOut) const
{
    AppSettings result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT theme, system_timezone, data_retention_days, history_flush_interval_s "
            "FROM %1 WHERE id = 1")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableAppSettings)))) {
        setErr(errorOut, q);
        return result;
    }
    if (!q.next()) {
        return result;
    }
    result.theme             = q.value(0).toString();
    result.systemTimezone    = q.value(1).toString();
    result.dataRetentionDays = q.value(2).toInt();
    result.historyFlushIntervalS = q.value(3).toInt();
    if (result.historyFlushIntervalS <= 0) {
        result.historyFlushIntervalS = kHistoryFlushIntervalS;
    }
    return result;
}

bool SettingsRepository::update(const AppSettings &settings, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE %1 SET "
        "  theme = :theme,"
        "  system_timezone = :tz,"
        "  data_retention_days = :retention,"
        "  history_flush_interval_s = :history_flush "
        "WHERE id = 1")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableAppSettings)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTheme),       settings.theme);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTz),          settings.systemTimezone);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindRetention),   settings.dataRetentionDays);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindHistoryFlush), settings.historyFlushIntervalS);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace CentralLogger::Data
