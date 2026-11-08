#include "SettingsController.h"

#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SettingsRepository.h"
#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/UiConstants.h"

#include <QDateTime>
#include <QJSEngine>
#include <QQmlEngine>
#include <QTimeZone>
#include <QVariantMap>
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
    value = qBound(Defaults::kMinIntervalSec, value, Defaults::kMaxIntervalSec);
    if (m_settings.historyFlushIntervalS == value) return;
    m_settings.historyFlushIntervalS = value;
    emit historyFlushIntervalSChanged();
}

void SettingsController::setPrimaryLoggerId(qint64 value)
{
    if (value < 0) value = Defaults::kNoPrimaryLoggerId;
    if (m_settings.primaryLoggerId == value) return;
    m_settings.primaryLoggerId = value;
    emit primaryLoggerIdChanged();
}

void SettingsController::reloadLoggerChoices()
{
    m_loggerChoices.clear();

    if (m_db && m_db->isOpen()) {
        Data::LoggerRepository repo(m_db->connection());
        for (const auto &lg : repo.findAll()) {
            QVariantMap item;
            item.insert(QLatin1String(CentralLogger::Ui::kRoleId), lg.id);
            item.insert(QLatin1String(CentralLogger::Ui::kRoleName), lg.name);
            m_loggerChoices.append(item);
        }
    }
    emit loggerChoicesChanged();
}

void SettingsController::load()
{
    // L-13: clear stale error from a previous load/save before retrying.
    setError(QString{});

    if (!m_db || !m_db->isOpen()) {
        setError(QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen));
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
    reloadLoggerChoices();
    if (previous.theme              != loaded.theme)              emit themeChanged();
    if (previous.systemTimezone     != loaded.systemTimezone)     emit systemTimezoneChanged();
    if (previous.dataRetentionDays      != loaded.dataRetentionDays)      emit dataRetentionDaysChanged();
    if (previous.historyFlushIntervalS  != loaded.historyFlushIntervalS)  emit historyFlushIntervalSChanged();
    if (previous.primaryLoggerId        != loaded.primaryLoggerId)        emit primaryLoggerIdChanged();
}

bool SettingsController::save()
{
    if (!m_db || !m_db->isOpen()) {
        setError(QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen));
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
    return local.toString(QLatin1String(CentralLogger::Format::kDateYyyyMmDdHms));
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
