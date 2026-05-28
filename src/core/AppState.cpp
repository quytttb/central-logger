#include "AppState.h"

#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"

#include <QJSEngine>
#include <QQmlEngine>

namespace CentralLogger::Core {

namespace {
AppState *g_instance = nullptr;
} // namespace

AppState::AppState(QObject *parent)
    : QObject(parent)
{
}

AppState *AppState::instance() { return g_instance; }

void AppState::setInstance(AppState *state) { g_instance = state; }

AppState *AppState::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(g_instance);
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

void AppState::refreshFromDatabase()
{
    if (!m_db || !m_db->isOpen()) {
        const QString text = QStringLiteral("Database not open");
        if (m_statusText != text) {
            m_statusText = text;
            emit statusTextChanged();
        }
        if (m_totalLoggers != 0) {
            m_totalLoggers = 0;
            emit totalLoggersChanged();
        }
        if (m_onlineLoggers != 0) {
            m_onlineLoggers = 0;
            emit onlineLoggersChanged();
        }
        if (m_alarmCount != 0) {
            m_alarmCount = 0;
            emit alarmCountChanged();
        }
        return;
    }

    Data::LoggerRepository loggers(m_db->connection());

    const auto rows = loggers.findAll();
    const int total = rows.size();
    int online = 0;
    for (const auto &l : rows) {
        if (l.status == QStringLiteral("online")) {
            ++online;
        }
    }

    int alarms = 0;
    for (bool alarm : std::as_const(m_alarmByLogger)) {
        if (alarm) {
            ++alarms;
        }
    }

    const QString text = total == 0 ? QStringLiteral("No loggers configured")
                                    : QStringLiteral("Ready");

    if (m_totalLoggers != total) {
        m_totalLoggers = total;
        emit totalLoggersChanged();
    }
    if (m_onlineLoggers != online) {
        m_onlineLoggers = online;
        emit onlineLoggersChanged();
    }
    if (m_alarmCount != alarms) {
        m_alarmCount = alarms;
        emit alarmCountChanged();
    }
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}

void AppState::updateAlarmState(qint64 loggerId, bool anyAlarm)
{
    if (m_alarmByLogger.value(loggerId, false) == anyAlarm) return;
    m_alarmByLogger.insert(loggerId, anyAlarm);
    refreshFromDatabase();
}

void AppState::removeLogger(qint64 loggerId)
{
    if (!m_alarmByLogger.remove(loggerId)) return;
    refreshFromDatabase();
}

} // namespace CentralLogger::Core
