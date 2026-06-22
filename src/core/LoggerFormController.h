#pragma once

#include "network/rest/RestConfigParser.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>
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

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    /// Parent required so QML cannot default-construct a second singleton.
    explicit LoggerFormController(QObject *parent);

    QString lastError() const { return m_lastError; }

    void setDatabase(Data::Database *db) { m_db = db; }
    void setRestConfigService(Network::RestConfigService *rest);
    void setDashboardController(DashboardController *dashboard) { m_dashboard = dashboard; }

    static LoggerFormController *instance();
    static void setInstance(LoggerFormController *controller);
    static LoggerFormController *create(QQmlEngine *, QJSEngine *);

public slots:
    /// Returns the new row id on success, or -1 on validation / SQL error.
    /// On failure `lastError` is populated; the SQL UNIQUE error is mapped
    /// to a friendlier message.
    qint64 addLogger(const QString &stationCode,
                     const QString &name,
                     const QString &host,
                     int modbusPort,
                     int apiPort,
                     const QString &apiToken,
                     const QString &note = QString(),
                     int modbusUnitId = 1,
                     int pollIntervalS = 2,
                     int timeoutS = 2);

    bool updateLogger(qint64 id,
                      const QString &stationCode,
                      const QString &name,
                      const QString &host,
                      int modbusPort,
                      int apiPort,
                      const QString &apiToken,
                      const QString &note = QString(),
                      int modbusUnitId = 1,
                      int pollIntervalS = 2,
                      int timeoutS = 2);

    bool removeLogger(qint64 id);

    /// Snapshot of a logger as a flat QVariantMap suitable for prefilling
    /// the form dialog. Returns an empty map when the id is unknown.
    QVariantMap getLoggerFormData(qint64 id) const;

    /// Probe GET /config from form fields (RAM only, no DB).
    Q_INVOKABLE void probeConfig(const QString &host, int apiPort, const QString &token);

    /// Clears the cached RAM probe config (including config version).
    Q_INVOKABLE void clearProbedConfig();

    /// Modbus unit ID from the last successful probe (`modbus_tcp_unit_id`), or -1.
    Q_INVOKABLE int probedModbusUnitId() const { return m_probedModbusUnitId; }

    /// `poll_interval` from last GET/probe snapshot, or -1 if absent.
    Q_INVOKABLE int probedPollInterval() const;

    /// `station_name` from last GET/probe snapshot (may be empty).
    Q_INVOKABLE QString probedStationName() const;

    /// True when a config snapshot is loaded in RAM (Connect or Edit auto-fetch).
    Q_INVOKABLE bool hasProbedConfig() const { return m_probedRevision >= 0; }

    /// Edit: GET /config for @p loggerId using DB connection params.
    Q_INVOKABLE void loadConfigForForm(qint64 loggerId);

    /// Atomic Save from Add/Edit form (async — listen for formSaveFinished).
    Q_INVOKABLE void saveLoggerFromForm(bool isAdd,
                                        qint64 loggerId,
                                        const QString &name,
                                        const QString &host,
                                        int modbusPort,
                                        int apiPort,
                                        const QString &apiToken,
                                        int modbusUnitId,
                                        int pollIntervalS,
                                        int timeoutS);

    /// Resets lastError to empty. Call from QML before opening the form so
    /// errors from a previous dialog session don't persist.
    Q_INVOKABLE void clearLastError();

    /// IPv4 or hostname (docs/thiet_ke_db.md). Used by LoggerFormDialog.
    Q_INVOKABLE bool isValidHost(const QString &host) const;

    /// Build a JSON patch from the edit form diff.
    /// Returns a QVariantMap with keys that changed (for POST /config body).
    Q_INVOKABLE static QVariantMap buildEditPatch(const QVariantMap &original,
                                                  const QVariantMap &edited);

signals:
    void lastErrorChanged();
    void loggerAdded(qint64 id);
    void loggerUpdated(qint64 id);
    void loggerRemoved(qint64 id);

    /// Probe result forwarded from RestConfigService.
    void probeConfigResult(bool ok, QString errorMessage);

    /// Edit auto-fetch (GET /config by logger id) finished.
    void configLoadForFormFinished(bool ok, QString errorMessage);

    /// Add/Edit Save finished (@p loggerId valid when @p ok).
    void formSaveFinished(bool ok, qint64 loggerId, QString errorMessage);

    /// Emitted when the REST config push failed (applyConfig returned !ok).
    /// Consumers (LoggersView) show an error banner.
    void configApplyFailed(qint64 loggerId, QString errorMessage);

private:
    bool ensureDatabase();
    bool validateCommon(const QString &stationCode,
                        const QString &name,
                        const QString &host,
                        int modbusPort,
                        int apiPort);
    void setError(const QString &message);

    void onProbeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void onConfigFetchedForForm(qint64 loggerId, bool ok, int httpStatus,
                                QString rawJson, QString errorMessage);

    void storeProbedFromParsed(const Network::RestConfigParser::ConfigPayload &parsed,
                               qint64 loggerIdForSensors);
    bool upsertProbedCatalog(qint64 loggerId, QString *errorOut);
    bool waitForConfigApply(qint64 loggerId, int expectedRevision,
                            const QJsonObject &patch, int *appliedRevisionOut,
                            QString *errorOut);
    QString probedStationCode() const;

    Data::Database             *m_db        = nullptr;
    Network::RestConfigService *m_restConfig = nullptr;
    DashboardController        *m_dashboard = nullptr;
    QString                     m_lastError;

    int         m_probedRevision = -1;
    QJsonObject m_probedConfigObject;
    int         m_probedModbusUnitId = -1;
    QVector<CentralLogger::Data::LoggerSensor> m_probedSensors;

    qint64 m_formLoadLoggerId   = -1;
    bool   m_formSaveInProgress = false;
};

} // namespace CentralLogger::Core
