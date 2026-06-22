#pragma once

#include "LoggerListModel.h"
#include "core/events/RecentEventsModel.h"
#include "core/sensors/SensorMonitoringTableModel.h"
#include "core/charts/PollHistoryStore.h"
#include "core/sensors/SensorSnapshotCache.h"

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
    Q_PROPERTY(CentralLogger::Core::RecentEventsModel *recentEvents
                   READ recentEvents CONSTANT)
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

    LoggerListModel *loggers() { return &m_loggers; }
    SensorMonitoringTableModel *sensorTable() { return &m_sensorTable; }
    SensorSnapshotCache       *sensorCache() { return &m_sensorCache; }
    PollHistoryStore          *pollHistory() { return &m_pollHistory; }
    RecentEventsModel         *recentEvents() { return &m_recentEvents; }
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
    void onSnapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                           int sensorCount);

    /// Insert a row into `system_event` and refresh the recent-events list.
    /// Called from LoggerDetailViewModel and LoggerFormController.
    /// @p loggerId may be 0 for app-level events.
    void logEvent(qint64 loggerId, const QString &eventType, const QString &message);

signals:
    void readingsChartChanged();
    void retentionPurgeCompleted(int deletedCount);
    /// Emitted on the main thread after the snapshot cache has been
    /// updated for @p loggerId. Task 8 wires this to LoggerDetailViewModel.
    void loggerSnapshotUpdated(qint64 loggerId, bool pollSuccess, QString modbusError);

private:
    void syncModbusRegistry();

    /// Edge-trigger helper for Task 19 — records `Online`/`Offline` events
    /// only on actual transitions, skipping the very first snapshot for an
    /// unknown logger so we don't spam the log when the app starts.
    void maybeLogStatusTransition(qint64 loggerId,
                                  const QString &prevStatus,
                                  const QString &newStatus);

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
};

} // namespace CentralLogger::Core
