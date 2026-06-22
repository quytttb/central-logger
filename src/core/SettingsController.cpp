#include "SettingsController.h"

#include "data/db/Database.h"
#include "data/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QJSEngine>
#include <QQmlEngine>
#include <QTimeZone>
#include <QtGlobal>

namespace CentralLogger::Core {

namespace {
SettingsController *g_instance = nullptr;
} // namespace

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{
}

SettingsController *SettingsController::instance() { return g_instance; }

void SettingsController::setInstance(SettingsController *controller)
{
    g_instance = controller;
}

SettingsController *SettingsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(g_instance);
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

void SettingsController::setDatabase(Data::Database *db)
{
    m_db = db;
}

void SettingsController::setTheme(const QString &value)
{
    if (m_settings.theme == value) return;
    m_settings.theme = value;
    emit themeChanged();
}

void SettingsController::setSystemTimezone(const QString &value)
{
    if (m_settings.systemTimezone == value) return;
    m_settings.systemTimezone = value;
    emit systemTimezoneChanged();
}

void SettingsController::setDataRetentionDays(int value)
{
    if (m_settings.dataRetentionDays == value) return;
    m_settings.dataRetentionDays = value;
    emit dataRetentionDaysChanged();
}

void SettingsController::setHistoryFlushIntervalS(int value)
{
    value = qBound(1, value, 3600);
    if (m_settings.historyFlushIntervalS == value) return;
    m_settings.historyFlushIntervalS = value;
    emit historyFlushIntervalSChanged();
}

void SettingsController::load()
{
    // L-13: clear stale error from a previous load/save before retrying.
    setError(QString{});

    if (!m_db || !m_db->isOpen()) {
        setError(QStringLiteral("Database not open"));
        return;
    }
    Data::SettingsRepository repo(m_db->connection());
    QString err;
    const auto loaded = repo.get(&err);
    if (!err.isEmpty()) {
        setError(err);
        return;
    }
    const auto previous = m_settings;
    m_settings = loaded;
    setError(QString{});
    if (previous.theme              != loaded.theme)              emit themeChanged();
    if (previous.systemTimezone     != loaded.systemTimezone)     emit systemTimezoneChanged();
    if (previous.dataRetentionDays      != loaded.dataRetentionDays)      emit dataRetentionDaysChanged();
    if (previous.historyFlushIntervalS  != loaded.historyFlushIntervalS)  emit historyFlushIntervalSChanged();
}

bool SettingsController::save()
{
    if (!m_db || !m_db->isOpen()) {
        setError(QStringLiteral("Database not open"));
        return false;
    }
    Data::SettingsRepository repo(m_db->connection());
    QString err;
    if (!repo.update(m_settings, &err)) {
        setError(err.isEmpty() ? QStringLiteral("No row updated") : err);
        return false;
    }
    setError(QString{});
    emit saved();
    return true;
}

bool SettingsController::saveTheme(const QString &value)
{
    setTheme(value);
    return save();
}

QString SettingsController::formatTimestamp(const QDateTime &dt) const
{
    if (!dt.isValid()) return QString();
    QTimeZone tz(m_settings.systemTimezone.toUtf8());
    const QDateTime local = tz.isValid() ? dt.toTimeZone(tz) : dt.toLocalTime();
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void SettingsController::setError(const QString &message)
{
    if (m_lastError == message) return;
    m_lastError = message;
    emit lastErrorChanged();
    if (!message.isEmpty()) {
        emit error(message);
    }
}

} // namespace CentralLogger::Core
