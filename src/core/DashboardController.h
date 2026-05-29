#pragma once

#include "LoggerListModel.h"
#include "core/events/RecentEventsModel.h"
#include "core/sensors/SensorMonitoringTableModel.h"
#include "core/charts/PollHistoryStore.h"
#include "core/sensors/SensorSnapshotCache.h"

#include "network/rest/RestConfigParser.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {
class ModbusBridge;
class ModbusService;
class RestConfigService;
struct PollSnapshot;
} // namespace CentralLogger::Network

namespace CentralLogger::Core {

class AppState;
class SettingsController;

/// QML facade for logger CRUD. Task 3 implements add/update/remove + form
/// data; Task 4 will extend the same class with Modbus snapshot ingestion.
///
/// Registered as a QML singleton (`CentralLogger.Core.DashboardController`).
/// Same setInstance/create pattern as AppState / SettingsController.
class DashboardController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CentralLogger::Core::LoggerListModel *loggers READ loggers CONSTANT)
    Q_PROPERTY(CentralLogger::Core::SensorMonitoringTableModel *sensorTable
                   READ sensorTable CONSTANT)
    Q_PROPERTY(CentralLogger::Core::RecentEventsModel *recentEvents
                   READ recentEvents CONSTANT)
    Q_PROPERTY(QVariantList readingsChartPlotPoints READ readingsChartPlotPoints
                   NOTIFY readingsChartChanged)
    Q_PROPERTY(QVariantMap readingsChartAxis READ readingsChartAxis
                   NOTIFY readingsChartChanged)
    Q_PROPERTY(bool readingsChartHasData READ readingsChartHasData
                   NOTIFY readingsChartChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    /// Parent required so QML cannot default-construct a second singleton
    /// (Qt 6 prefers Constructor over create() when T is default-constructible).
    explicit DashboardController(QObject *parent);

    LoggerListModel *loggers() { return &m_loggers; }
    SensorMonitoringTableModel *sensorTable() { return &m_sensorTable; }
    SensorSnapshotCache       *sensorCache() { return &m_sensorCache; }
    PollHistoryStore          *pollHistory() { return &m_pollHistory; }
    RecentEventsModel         *recentEvents() { return &m_recentEvents; }
    QVariantList readingsChartPlotPoints() const { return m_readingsChartPlotPoints; }
    QVariantMap  readingsChartAxis() const { return m_readingsChartAxis; }
    bool         readingsChartHasData() const { return m_readingsChartHasData; }
    QString lastError() const { return m_lastError; }
    void setDatabase(Data::Database *db);
    void setAppState(AppState *state) { m_appState = state; }

    /// Modbus integration wiring. main.cpp constructs both bridge and
    /// service then hands them in. Settings is queried for maintenanceMode.
    void setModbusBridge(Network::ModbusBridge *bridge);
    void setModbusService(Network::ModbusService *service);
    void setSettingsController(SettingsController *settings);
    void setRestConfigService(Network::RestConfigService *rest);

    static DashboardController *instance();
    static void setInstance(DashboardController *controller);
    static DashboardController *create(QQmlEngine *, QJSEngine *);

public slots:
    void reloadLoggers();
    void refreshReadingsChart();
    void reloadRecentEvents();

    /// Purge sensor_reading rows older than `data_retention_days`.
    /// Emits retentionPurgeCompleted(deleted) and refreshes the
    /// readings chart when rows were removed.  FE-016.
    void purgeOldData();

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

    /// Task 22: Probe GET /config from form fields (RAM only, no DB).
    /// Calls RestConfigService::probeConfig and re-emits result as
    /// probeConfigResult for QML.
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

    /// M-15: IPv4 or hostname (docs/thiet_ke_db.md). Used by LoggerFormDialog.
    Q_INVOKABLE bool isValidHost(const QString &host) const;

    /// Tooltip snap for readings chart (plot area coords + mouse). Empty map = miss.
    Q_INVOKABLE QVariantMap snapReadingsChart(double mouseX,
                                              double mouseY,
                                              double plotX,
                                              double plotY,
                                              double plotW,
                                              double plotH) const;

    /// Task 23: Build a JSON patch from the edit form diff.
    /// Returns a QVariantMap with keys that changed (for POST /config body).
    Q_INVOKABLE static QVariantMap buildEditPatch(const QVariantMap &original,
                                                   const QVariantMap &edited);

    /// Pushes loggers with `logger_info.enabled` to the Modbus worker and starts polling.
    /// Idempotent; safe to call again after CRUD.
    void startModbusPolling();
    void stopModbusPolling();

    /// Bridge → DashboardController on the main thread. Wired from main.cpp
    /// so it has to be public; not intended for direct QML use.
    void onSnapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                           int sensorCount);

    /// Insert a row into `system_event` and refresh the recent-events list.
    /// Called from LoggerDetailViewModel after a report download completes.
    /// @p loggerId may be 0 for app-level events.
    void logEvent(qint64 loggerId, const QString &eventType, const QString &message);

signals:
    void lastErrorChanged();
    void readingsChartChanged();
    void retentionPurgeCompleted(int deletedCount);
    void loggerAdded(qint64 id);
    void loggerUpdated(qint64 id);
    void loggerRemoved(qint64 id);
    /// Emitted on the main thread after the snapshot cache has been
    /// updated for @p loggerId. Task 8 wires this to LoggerDetailViewModel.
    void loggerSnapshotUpdated(qint64 loggerId, bool pollSuccess, QString modbusError);

    /// Task 22: probe result forwarded from RestConfigService.
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
    QString humanizeSqlError(const QString &raw, const QString &stationCode) const;
    void afterMutation();

    void syncModbusRegistry();

    /// Edge-trigger helper for Task 19 — records `Online`/`Offline` events
    /// only on actual transitions, skipping the very first snapshot for an
    /// unknown logger so we don't spam the log when the app starts.
    void maybeLogStatusTransition(qint64 loggerId,
                                  const QString &prevStatus,
                                  const QString &newStatus);

    void onProbeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void onConfigFetchedForForm(qint64 loggerId, bool ok, int httpStatus,
                                QString rawJson, QString errorMessage);

    void storeProbedFromParsed(const Network::RestConfigParser::ConfigPayload &parsed,
                               qint64 loggerIdForSensors);
    bool upsertProbedCatalog(qint64 loggerId, QString *errorOut);
    bool waitForConfigApply(qint64 loggerId, int expectedRevision,
                            const QJsonObject &patch, int *appliedRevisionOut,
                            QString *errorOut);

    Data::Database        *m_db       = nullptr;
    AppState              *m_appState = nullptr;
    SettingsController    *m_settings = nullptr;
    Network::ModbusBridge *m_bridge   = nullptr;
    Network::ModbusService *m_modbus  = nullptr;
    LoggerListModel        m_loggers;
    SensorMonitoringTableModel m_sensorTable;
    SensorSnapshotCache    m_sensorCache;
    PollHistoryStore       m_pollHistory;
    RecentEventsModel      m_recentEvents;
    QHash<qint64, QString> m_lastStatus;
    QVariantList           m_readingsChartPlotPoints;
    QVariantMap            m_readingsChartAxis;
    bool                   m_readingsChartHasData = false;
    QTimer                 m_purgeTimer;
    QTimer                 m_readingsChartTimer;
    QString                m_lastError;
    Network::RestConfigService *m_restConfig = nullptr;

    int m_probedRevision = -1;
    QJsonObject m_probedConfigObject;
    int         m_probedModbusUnitId = -1;
    QVector<CentralLogger::Data::LoggerSensor> m_probedSensors;

    qint64 m_formLoadLoggerId   = -1;
    bool   m_formSaveInProgress = false;

    QString probedStationCode() const;
};

} // namespace CentralLogger::Core
