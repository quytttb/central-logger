#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

/// Read-only aggregate of dashboard stats. Wired in Task 2 from the
/// loggers/events repositories; ModbusBridge will refresh it in Task 4.
class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int     totalLoggers   READ totalLoggers   NOTIFY totalLoggersChanged)
    Q_PROPERTY(int     onlineLoggers  READ onlineLoggers  NOTIFY onlineLoggersChanged)
    Q_PROPERTY(int     alarmCount     READ alarmCount     NOTIFY alarmCountChanged)
    Q_PROPERTY(QString statusText     READ statusText     NOTIFY statusTextChanged)

public:
    /// Parent required — Qt 6 uses the default ctor for QML_SINGLETON when
    /// parent is optional, which would bypass create() and the main.cpp instance.
    explicit AppState(QObject *parent);

    int     totalLoggers()  const { return m_totalLoggers; }
    int     onlineLoggers() const { return m_onlineLoggers; }
    int     alarmCount()    const { return m_alarmCount; }
    QString statusText()    const { return m_statusText; }

    /// Process-wide singleton handed to the QML engine. Set once at startup
    /// from main.cpp before the QML engine loads.
    static AppState *instance();
    static void setInstance(AppState *state);

    /// QML_SINGLETON factory — returns the process-wide instance set above.
    static AppState *create(QQmlEngine *, QJSEngine *);

    void setDatabase(Data::Database *db) { m_db = db; }

public slots:
    void refreshFromDatabase();

    /// Called by DashboardController on every poll snapshot to keep
    /// alarmCount in sync with live Modbus state instead of event history.
    void updateAlarmState(qint64 loggerId, bool anyAlarm);

    /// Remove a logger's alarm tracking entry (called on logger removal).
    void removeLogger(qint64 loggerId);

signals:
    void totalLoggersChanged();
    void onlineLoggersChanged();
    void alarmCountChanged();
    void statusTextChanged();

private:
    Data::Database        *m_db = nullptr;
    QHash<qint64, bool>   m_alarmByLogger;  // live alarm state per logger
    int     m_totalLoggers  = 0;
    int     m_onlineLoggers = 0;
    int     m_alarmCount    = 0;
    QString m_statusText    = QStringLiteral("Ready");
};

} // namespace CentralLogger::Core
