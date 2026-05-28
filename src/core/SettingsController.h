#pragma once

#include "data/models/AppSettings.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

/// Bridges `app_settings` (singleton row, id=1) to QML. Owns the live values
/// while the app runs; persists them via SettingsRepository on save().
///
/// Registered as a QML singleton (`CentralLogger.Core.SettingsController`).
/// Single instance created in main.cpp and handed to the QML factory via
/// setInstance() — same pattern as AppState.
class SettingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString theme              READ theme              WRITE setTheme              NOTIFY themeChanged)
    Q_PROPERTY(QString systemTimezone     READ systemTimezone     WRITE setSystemTimezone     NOTIFY systemTimezoneChanged)
    Q_PROPERTY(int     dataRetentionDays  READ dataRetentionDays  WRITE setDataRetentionDays  NOTIFY dataRetentionDaysChanged)
    Q_PROPERTY(bool    maintenanceMode    READ maintenanceMode    WRITE setMaintenanceMode    NOTIFY maintenanceModeChanged)
    Q_PROPERTY(QString lastError          READ lastError                                       NOTIFY lastErrorChanged)

public:
    explicit SettingsController(QObject *parent = nullptr);

    QString theme()             const { return m_settings.theme; }
    QString systemTimezone()    const { return m_settings.systemTimezone; }
    int     dataRetentionDays() const { return m_settings.dataRetentionDays; }
    bool    maintenanceMode()   const { return m_settings.maintenanceMode; }
    QString lastError()         const { return m_lastError; }

    void setTheme(const QString &value);
    void setSystemTimezone(const QString &value);
    void setDataRetentionDays(int value);
    void setMaintenanceMode(bool value);

    /// QML singleton accessor + factory — see AppState.
    static SettingsController *instance();
    static void setInstance(SettingsController *controller);
    static SettingsController *create(QQmlEngine *, QJSEngine *);

    void setDatabase(Data::Database *db) { m_db = db; }

    /// Format @p dt in the configured systemTimezone using "yyyy-MM-dd HH:mm:ss".
    /// Falls back to local time when the timezone name is invalid.
    /// Callable from QML: SettingsController.formatTimestamp(model.createdAt)
    Q_INVOKABLE QString formatTimestamp(const QDateTime &dt) const;

public slots:
    /// Reads `app_settings` row 1 from the database; emits per-property
    /// signals so any QML binding refreshes.
    void load();

    /// Writes the in-memory snapshot back to the row. Emits saved() on
    /// success, error(message) on failure.
    bool save();

    /// Convenience for an instant theme toggle from the top bar.
    bool saveTheme(const QString &value);

signals:
    void themeChanged();
    void systemTimezoneChanged();
    void dataRetentionDaysChanged();
    void maintenanceModeChanged();
    void lastErrorChanged();
    void saved();
    void error(const QString &message);

private:
    void setError(const QString &message);

    Data::Database  *m_db = nullptr;
    Data::AppSettings m_settings;
    QString          m_lastError;
};

} // namespace CentralLogger::Core
