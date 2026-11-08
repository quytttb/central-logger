#pragma once

#include "LoggerListModel.h"
#include "core/events/RecentEventsModel.h"
#include "core/sensors/LoggerOverviewTableModel.h"
#include "core/sensors/SensorMonitoringTableModel.h"
#include "core/charts/PollHistoryStore.h"
#include "core/sensors/SensorSnapshotCache.h"
#include "utils/AppConstants.h"

#include <QFuture>
#include <QHash>
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
struct PollSnapshot;
} // namespace CentralLogger::Network

namespace CentralLogger::Core {

class AppState;
class SettingsController;

/// QML facade for the Dashboard: live logger list, sensor monitoring table,
/// recent events, the readings chart and the Modbus polling life-cycle.
///
/// Logger Add/Edit/Remove + REST config probing live in LoggerFormController;
/// that controller calls back into afterMutation()/logEvent() here to refresh
/// the dashboard's models after a CRUD operation.
///
/// Registered as a QML singleton (`CentralLogger.Core.DashboardController`).
class DashboardController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CentralLogger::Core::LoggerListModel *loggers READ loggers CONSTANT)
    Q_PROPERTY(CentralLogger::Core::SensorMonitoringTableModel *sensorTable
                   READ sensorTable CONSTANT)
    Q_PROPERTY(CentralLogger::Core::LoggerOverviewTableModel *overviewTable
                   READ overviewTable CONSTANT)
    Q_PROPERTY(CentralLogger::Core::RecentEventsModel *recentEvents
                   READ recentEvents CONSTANT)
    Q_PROPERTY(qint64 primaryLoggerId READ primaryLoggerId NOTIFY primaryLoggerChanged)
    Q_PROPERTY(QString primaryLoggerName READ primaryLoggerName NOTIFY primaryLoggerChanged)
    Q_PROPERTY(QVariantList primaryChartSeries READ primaryChartSeries
                   NOTIFY primaryChartChanged)
    Q_PROPERTY(QVariantMap primaryChartAxis READ primaryChartAxis
                   NOTIFY primaryChartChanged)
    Q_PROPERTY(bool primaryChartHasData READ primaryChartHasData
                   NOTIFY primaryChartChanged)
    Q_PROPERTY(QVariantList readingsChartPlotPoints READ readingsChartPlotPoints
                   NOTIFY readingsChartChanged)
    Q_PROPERTY(QVariantMap readingsChartAxis READ readingsChartAxis
                   NOTIFY readingsChartChanged)
    Q_PROPERTY(bool readingsChartHasData READ readingsChartHasData
                   NOTIFY readingsChartChanged)

public:
    /// Parent required so QML cannot default-construct a second singleton
    /// (Qt 6 prefers Constructor over create() when T is default-constructible).
    explicit DashboardController(QObject *parent);
    ~DashboardController() override;

    LoggerListModel *loggers() { return &m_loggers; }
    SensorMonitoringTableModel *sensorTable() { return &m_sensorTable; }
    LoggerOverviewTableModel   *overviewTable() { return &m_overviewTable; }
    SensorSnapshotCache       *sensorCache() { return &m_sensorCache; }
    PollHistoryStore          *pollHistory() { return &m_pollHistory; }
    RecentEventsModel         *recentEvents() { return &m_recentEvents; }
    qint64        primaryLoggerId() const { return m_primaryLoggerId; }
    QString       primaryLoggerName() const { return m_primaryLoggerName; }
    QVariantList  primaryChartSeries() const { return m_primaryChartSeries; }
    QVariantMap   primaryChartAxis() const { return m_primaryChartAxis; }
    bool          primaryChartHasData() const { return m_primaryChartHasData; }
    QVariantList readingsChartPlotPoints() const { return m_readingsChartPlotPoints; }
    QVariantMap  readingsChartAxis() const { return m_readingsChartAxis; }
    bool         readingsChartHasData() const { return m_readingsChartHasData; }
    void setDatabase(Data::Database *db);
    void setAppState(AppState *state) { m_appState = state; }

    /// Modbus integration wiring. main.cpp constructs both bridge and
    /// service then hands them in.
    void setModbusBridge(Network::ModbusBridge *bridge);
    void setModbusService(Network::ModbusService *service);
    void setSettingsController(SettingsController *settings);

    static DashboardController *instance();
    static void setInstance(DashboardController *controller);
    static DashboardController *create(QQmlEngine *, QJSEngine *);

    /// Refresh all dashboard models after a CRUD mutation. Called by
    /// LoggerFormController once a logger is added / updated / removed.
    void afterMutation();

    /// Drop in-RAM dashboard state for a removed logger (snapshot cache,
    /// trending history, last-status map, alarm state, detail table).
    /// Called by LoggerFormController inside removeLogger.
    void cleanupRemovedLogger(qint64 id);

public slots:
    void reloadLoggers();
    void refreshReadingsChart();
    void reloadRecentEvents();

    /// Resolve the configured primary logger (fallback: first online logger),
    /// rebuild its real-time chart and the secondary-logger overview table.
    /// Called by DashboardView when switching to the summary mode.
    void refreshSummary();

    /// Tooltip snap for the primary-logger summary chart. Empty map = miss.
    Q_INVOKABLE QVariantMap snapPrimaryChart(double mouseX,
                                             double mouseY,
                                             double plotX,
                                             double plotY,
                                             double plotW,
                                             double plotH) const;

    /// Purge sensor_reading rows older than `data_retention_days`.
    /// Emits retentionPurgeCompleted(deleted) and refreshes the
    /// readings chart when rows were removed.  FE-016.
    void purgeOldData();

    /// Tooltip snap for readings chart (plot area coords + mouse). Empty map = miss.
    Q_INVOKABLE QVariantMap snapReadingsChart(double mouseX,
                                              double mouseY,
                                              double plotX,
                                              double plotY,
                                              double plotW,
                                              double plotH) const;

    /// Pushes loggers with `logger_info.enabled` to the Modbus worker and starts polling.
    /// Idempotent; safe to call again after CRUD.
    void startModbusPolling();
    void stopModbusPolling();

    /// Bridge → DashboardController on the main thread. Wired from main.cpp
    /// so it has to be public; not intended for direct QML use.
    /// Audit H-A: @p catalogRows is the catalog fetched on the bridge thread;
    /// the UI path no longer issues its own catalog SELECT.
    void onSnapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                           int sensorCount,
                           const QVector<Data::LoggerSensor> &catalogRows);

    /// Insert a row into `system_event` and refresh the recent-events list.
    /// Called from LoggerDetailViewModel and LoggerFormController.
    /// @p loggerId may be 0 for app-level events.
    void logEvent(qint64 loggerId, const QString &eventType, const QString &message);

signals:
    void readingsChartChanged();
    void primaryChartChanged();
    void primaryLoggerChanged();
    void retentionPurgeCompleted(int deletedCount);
    /// Emitted on the main thread after the snapshot cache has been
    /// updated for @p loggerId. Task 8 wires this to LoggerDetailViewModel.
    void loggerSnapshotUpdated(qint64 loggerId, bool pollSuccess, QString modbusError);

private:
    void syncModbusRegistry();

    /// Audit H-A: clear the bridge's cached catalog entries for @p loggerId
    /// (or all when <=0) after catalog mutations. Safe to call from any thread.
    void invalidateBridgeCatalogCache(qint64 loggerId);

    /// Edge-trigger helper for Task 19 — records `Online`/`Offline` events
    /// only on actual transitions, skipping the very first snapshot for an
    /// unknown logger so we don't spam the log when the app starts.
    void maybeLogStatusTransition(qint64 loggerId,
                                  const QString &prevStatus,
                                  const QString &newStatus);

    /// Configured primary logger if it still exists; otherwise the first
    /// online logger (or first logger when none is online); 0 when empty.
    qint64 resolvePrimaryLoggerId() const;
    void   refreshPrimaryChart();
    void   refreshOverviewTable();

    Data::Database        *m_db       = nullptr;
    AppState              *m_appState = nullptr;
    SettingsController    *m_settings = nullptr;
    Network::ModbusBridge *m_bridge   = nullptr;
    Network::ModbusService *m_modbus  = nullptr;
    LoggerListModel        m_loggers;
    SensorMonitoringTableModel m_sensorTable;
    LoggerOverviewTableModel   m_overviewTable;
    SensorSnapshotCache    m_sensorCache;
    PollHistoryStore       m_pollHistory;
    RecentEventsModel      m_recentEvents;
    QHash<qint64, QString> m_lastStatus;
    qint64                 m_primaryLoggerId = Defaults::kNoPrimaryLoggerId;
    QString                m_primaryLoggerName;
    QVariantList           m_primaryChartSeries;
    QVariantMap            m_primaryChartAxis;
    bool                   m_primaryChartHasData = false;
    QVariantList           m_readingsChartPlotPoints;
    QVariantMap            m_readingsChartAxis;
    bool                   m_readingsChartHasData = false;
    QTimer                 m_purgeTimer;
    bool                   m_purgeRunning = false;
    bool                   m_chartQueryRunning = false; // H-E coalesce guard
};

} // namespace CentralLogger::Core
