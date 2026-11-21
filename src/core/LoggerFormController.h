#pragma once

#include "network/rest/RestConfigParser.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {
class RestConfigService;
} // namespace CentralLogger::Network

namespace CentralLogger::Core {

class DashboardController;

/// QML facade for logger Add/Edit/Remove + REST config probing. Split out of
/// DashboardController so the dashboard stays focused on live state (Modbus
/// polling, charts, events) while this owns the form/CRUD lifecycle.
///
/// Registered as a QML singleton (`CentralLogger.Core.LoggerFormController`).
/// Mutations delegate dashboard refresh/event logging back to
/// DashboardController (wired via setDashboardController in main.cpp).
class LoggerFormController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(qint64 reloadingLoggerId READ reloadingLoggerId NOTIFY reloadingLoggerChanged)

public:
    /// Parent required so QML cannot default-construct a second singleton.
    explicit LoggerFormController(QObject *parent);

    qint64 reloadingLoggerId() const { return m_reloadingLoggerId; }

    void setDatabase(Data::Database *db) { m_db = db; }
    void setRestConfigService(Network::RestConfigService *rest);
    void setDashboardController(DashboardController *dashboard) { m_dashboard = dashboard; }

    static void setInstance(LoggerFormController *controller);
    static LoggerFormController *create(QQmlEngine *, QJSEngine *);

public slots:
    bool removeLogger(qint64 id);

    /// Toggle `logger_info.enabled` and sync Modbus polling immediately.
    Q_INVOKABLE bool setLoggerEnabled(qint64 id, bool enabled);

    /// Snapshot of a logger as a flat QVariantMap suitable for prefilling
    /// the Settings form. Returns an empty map when the id is unknown.
    QVariantMap getLoggerFormData(qint64 id) const;

    /// Probe GET /config from form fields (RAM only, no DB).
    Q_INVOKABLE void probeConfig(const QString &host, int apiPort, const QString &token);

    /// Cancels the Add probe and discards its snapshot.
    Q_INVOKABLE void clearProbedConfig();

    /// Modbus unit ID from the last successful probe (`modbus_tcp_unit_id`), or -1.
    Q_INVOKABLE int probedModbusUnitId() const { return m_probedConfig.modbusTcpUnitId; }

    /// `station_name` from last GET/probe snapshot (may be empty).
    Q_INVOKABLE QString probedStationName() const { return m_probedConfig.stationName; }

    /// `station_code` from last GET/probe snapshot (may be empty).
    Q_INVOKABLE QString probedStationCode() const { return m_probedConfig.stationCode; }

    /// True when Add has a usable GET /config snapshot.
    bool hasProbedConfig() const { return m_probedConfig.valid && m_probedConfig.revision >= 0; }

    /// Read the saved logger's configuration and mirror its catalog/revision.
    /// No POST, and no change to its Central connection settings.
    Q_INVOKABLE void reloadLoggerConfig(qint64 loggerId);

    /// Local Save from Add/Edit. Add persists the probed catalog atomically;
    /// Edit does not require the logger to be reachable. No REST requests.
    Q_INVOKABLE void saveLoggerFromForm(bool isAdd,
                                        qint64 loggerId,
                                        const QString &stationCode,
                                        const QString &name,
                                        const QString &host,
                                        int modbusPort,
                                        int apiPort,
                                        const QString &apiToken,
                                        int modbusUnitId,
                                        int pollIntervalS,
                                        int timeoutS,
                                        bool enabled,
                                        const QString &note);

    /// IPv4 or hostname (docs/thiet_ke_db.md). Used by LoggersSettingsPane.
    Q_INVOKABLE bool isValidHost(const QString &host) const;

signals:
    void loggerRemoved(qint64 id);

    /// Probe result forwarded from RestConfigService.
    void probeConfigResult(bool ok, QString errorMessage);

    void reloadingLoggerChanged();
    void configReloadFinished(qint64 loggerId, bool ok, const QString &errorMessage);

    /// Add/Edit Save finished (@p loggerId valid when @p ok).
    void formSaveFinished(bool ok, qint64 loggerId, QString errorMessage);

private:
    bool ensureDatabase(QString *errorOut = nullptr) const;
    static bool validateCommon(const QString &stationCode,
                               const QString &name,
                               const QString &host,
                               int modbusPort,
                               int apiPort,
                               QString *errorOut = nullptr);

    void onProbeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void onConfigReloadFetched(qint64 loggerId, bool ok, int httpStatus,
                               const QString &rawJson, QString errorMessage);

    Data::Database             *m_db        = nullptr;
    Network::RestConfigService *m_restConfig = nullptr;
    DashboardController        *m_dashboard = nullptr;

    // Bind the catalog to the connection that returned it. Add must not
    // attach a previous device's catalog after a host/port/token change.
    struct ProbeEndpoint {
        QString host;
        int apiPort = 0;
        QString token;
        bool operator==(const ProbeEndpoint &) const = default;
    };
    ProbeEndpoint m_probeEndpoint;
    Network::RestConfigParser::ConfigPayload m_probedConfig;
    bool   m_probeRequested = false;
    qint64 m_reloadingLoggerId = 0;
};

} // namespace CentralLogger::Core
