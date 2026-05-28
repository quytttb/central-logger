#include "SettingsRepository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace CentralLogger::Data {

namespace {

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
            "SELECT theme, system_timezone, data_retention_days, maintenance_mode "
            "FROM app_settings WHERE id = 1"))) {
        setErr(errorOut, q);
        return result;
    }
    if (!q.next()) {
        return result;
    }
    result.theme             = q.value(0).toString();
    result.systemTimezone    = q.value(1).toString();
    result.dataRetentionDays = q.value(2).toInt();
    result.maintenanceMode   = q.value(3).toInt() != 0;
    return result;
}

bool SettingsRepository::update(const AppSettings &settings, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE app_settings SET "
        "  theme = :theme,"
        "  system_timezone = :tz,"
        "  data_retention_days = :retention,"
        "  maintenance_mode = :maintenance "
        "WHERE id = 1"));
    q.bindValue(QStringLiteral(":theme"),       settings.theme);
    q.bindValue(QStringLiteral(":tz"),          settings.systemTimezone);
    q.bindValue(QStringLiteral(":retention"),   settings.dataRetentionDays);
    q.bindValue(QStringLiteral(":maintenance"), settings.maintenanceMode ? 1 : 0);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace CentralLogger::Data
