#pragma once

#include "LoggerListModel.h"
#include "core/events/RecentEventsModel.h"
#include "core/sensors/LoggerLiveCardsModel.h"
#include "core/sensors/LoggerLiveListModel.h"
#include "core/sensors/LoggerOverviewTableModel.h"
#include "core/charts/GraphChartsModel.h"
#include "core/charts/GraphHistoryQueryService.h"
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
#include <memory>

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
    Q_PROPERTY(CentralLogger::Core::LoggerLiveCardsModel *liveCards
                   READ liveCards CONSTANT)
    Q_PROPERTY(CentralLogger::Core::LoggerLiveListModel *liveList
                   READ liveList CONSTANT)
    Q_PROPERTY(CentralLogger::Core::LoggerOverviewTableModel *overviewTable
                   READ overviewTable CONSTANT)
    Q_PROPERTY(CentralLogger::Core::RecentEventsModel *recentEvents
                   READ recentEvents CONSTANT)
    /// Logger currently plotted on the summary view. Defaults to the
    /// configured primary logger until the user picks another one in the
    /// "Show graph" dropdown (explicit choice wins until that logger is
    /// removed).
    Q_PROPERTY(qint64 graphLoggerId READ graphLoggerId WRITE setGraphLoggerId
                   NOTIFY graphLoggerChanged)
    /// Dropdown model: [{ id, name, online, status }] for every configured
    /// logger. Rebuilt whenever the logger catalog changes.
    Q_PROPERTY(QVariantList graphLoggerOptions READ graphLoggerOptions
                   NOTIFY graphLoggerOptionsChanged)
    Q_PROPERTY(QVariantMap graphChartAxis READ graphChartAxis
                   NOTIFY graphChartChanged)
    Q_PROPERTY(CentralLogger::Core::GraphChartsModel *graphCharts READ graphCharts CONSTANT)
    Q_PROPERTY(int graphWindowMinutes READ graphWindowMinutes NOTIFY graphWindowChanged)
    Q_PROPERTY(QString graphWindowLabel READ graphWindowLabel NOTIFY graphWindowChanged)
    Q_PROPERTY(bool canZoomGraphIn READ canZoomGraphIn NOTIFY graphWindowChanged)
    Q_PROPERTY(bool canZoomGraphOut READ canZoomGraphOut NOTIFY graphWindowChanged)
    Q_PROPERTY(bool atDefaultGraphZoom READ atDefaultGraphZoom NOTIFY graphWindowChanged)
    Q_PROPERTY(bool graphHistoryLoading READ graphHistoryLoading NOTIFY graphHistoryStateChanged)
    Q_PROPERTY(QString graphHistoryError READ graphHistoryError NOTIFY graphHistoryStateChanged)
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

    LoggerListModel           *loggers() { return &m_loggers; }
    LoggerLiveCardsModel      *liveCards() { return &m_liveCards; }
    LoggerLiveListModel       *liveList() { return &m_liveList; }
    Q_INVOKABLE void           setLiveFilterText(const QString &text);
    LoggerOverviewTableModel   *overviewTable() { return &m_overviewTable; }
    RecentEventsModel         *recentEvents() { return &m_recentEvents; }
    qint64        graphLoggerId() const;
    QVariantList  graphLoggerOptions() const { return m_graphLoggerOptions; }
    void          setGraphLoggerId(qint64 id);
    QVariantMap   graphChartAxis() const { return m_graphChartAxis; }
    GraphChartsModel *graphCharts() { return &m_graphCharts; }
    QVariantList  graphSensorCharts() const { return m_graphCharts.charts(); }
    int graphWindowMinutes() const { return Defaults::kGraphZoomPresetsMin[m_graphZoomIndex]; }
    QString graphWindowLabel() const;
    bool canZoomGraphIn() const { return m_graphZoomIndex > 0; }
    bool canZoomGraphOut() const { return m_graphZoomIndex + 1 < Defaults::kGraphZoomPresetsMin.size(); }
    bool atDefaultGraphZoom() const { return m_graphZoomIndex == Defaults::kGraphDefaultZoomIndex; }
    bool graphHistoryLoading() const { return m_graphHistoryLoading; }
    QString graphHistoryError() const { return m_graphHistoryError; }
    Q_INVOKABLE void setGraphActive(bool active);
    Q_INVOKABLE void zoomGraphIn();
    Q_INVOKABLE void zoomGraphOut();
    Q_INVOKABLE void resetGraphZoom();
    Q_INVOKABLE void refreshGraphHistory();
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

    static void setInstance(DashboardController *controller);
    static DashboardController *create(QQmlEngine *, QJSEngine *);

    /// Refresh all dashboard models after a CRUD mutation. Called by
    /// LoggerFormController once a logger is added / updated / removed.
    void afterMutation();

    /// Drop in-RAM dashboard state for a removed logger (snapshot cache,
    /// trending history, last-status map, alarm state, detail table).
    /// Called by LoggerFormController inside removeLogger.
    void cleanupRemovedLogger(qint64 id);

    /// Clear cached live values when a logger is disabled so UI badges and
    /// cards immediately reflect non-polling/offline state.
    void markLoggerDisabled(qint64 id);

    /// True when @p loggerId exists, is enabled, and its current status is online.
    bool isLoggerOnline(qint64 loggerId) const;

public slots:
    void reloadLoggers();
    void refreshReadingsChart();
    void reloadRecentEvents();

    /// Resolve the configured primary logger (fallback: first online logger),
    /// rebuild its real-time chart and the secondary-logger overview table.
    /// Called by DashboardView when switching to the summary mode.
    void refreshSummary();

    /// Re-read the sensor catalog for @p loggerId into the summary cache and
    /// drop the bridge's cached catalog so the next poll uses the refreshed
    /// metadata. Called after Reload config.
    void refreshLoggerCatalog(qint64 loggerId);

    /// Purge sensor_reading rows older than `data_retention_days` and refresh
    /// the readings chart when rows were removed. FE-016.
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

    /// Reopen the connection through the existing worker, without changing settings.
    void reconnectLogger(qint64 loggerId);

    /// Bridge → DashboardController on the main thread. Wired from main.cpp
    /// so it has to be public; not intended for direct QML use.
    /// Audit H-A: @p catalogRows is the catalog fetched on the bridge thread;
    /// the UI path no longer issues its own catalog SELECT.
    void onSnapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                           int sensorCount,
                           const QVector<Data::LoggerSensor> &catalogRows);

    /// Insert a row into `system_event` and refresh the recent-events list.
    /// Called from LoggerFormController.
    /// @p loggerId may be 0 for app-level events.
    void logEvent(qint64 loggerId, const QString &eventType, const QString &message);

signals:
    void readingsChartChanged();
    void graphChartChanged();
    void graphLoggerChanged();
    void graphLoggerOptionsChanged();
    void graphWindowChanged();
    void graphHistoryStateChanged();
    /// Emitted on the main thread after the snapshot cache has been
    /// updated for @p loggerId.
    void loggerSnapshotUpdated(qint64 loggerId, bool pollSuccess);

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
    /// Rebuild the logger dropdown from the current logger list.
    void   refreshGraphLoggerOptions();
    void   refreshGraphChart(bool forceQuery = false);
    void   setGraphZoomIndex(std::size_t index);
    void   updateGraphChart(qint64 loggerId, qint64 fromMs, qint64 toMs, const GraphSeries *history);
    void   setGraphHistoryState(bool loading, const QString &error);
    struct GraphWindow {
        qint64 loggerId = 0;
        int minutes = 0;
        bool operator==(const GraphWindow &) const = default;
    };
    void   startGraphHistoryQuery(const QString &dbPath, GraphWindow window, qint64 fromMs, qint64 toMs);
    void   refreshOverviewTable();
    void   refreshLiveModels();

    Data::Database        *m_db       = nullptr;
    AppState              *m_appState = nullptr;
    SettingsController    *m_settings = nullptr;
    Network::ModbusBridge *m_bridge   = nullptr;
    Network::ModbusService *m_modbus  = nullptr;
    LoggerListModel        m_loggers;
    LoggerLiveCardsModel   m_liveCards;
    LoggerLiveListModel    m_liveList;
    LoggerOverviewTableModel   m_overviewTable;
    SensorSnapshotCache    m_sensorCache;
    PollHistoryStore       m_pollHistory;
    RecentEventsModel      m_recentEvents;
    QHash<qint64, QString> m_lastStatus;
    /// 0 = follow the configured primary logger; >0 = explicit dropdown pick.
    qint64                 m_graphLoggerId = Defaults::kNoPrimaryLoggerId;
    qint64                 m_lastGraphLoggerId = Defaults::kNoPrimaryLoggerId;
    QVariantList           m_graphLoggerOptions;
    QVariantMap            m_graphChartAxis;
    GraphChartsModel       m_graphCharts;
    std::size_t            m_graphZoomIndex = Defaults::kGraphDefaultZoomIndex;
    bool                   m_graphActive = true;
    bool                   m_graphHistoryLoading = false;
    QString                m_graphHistoryError;
    GraphWindow            m_displayedGraphWindow;
    GraphWindow            m_graphQueryWindow;
    GraphWindow            m_graphHistoryWindow;
    qint64                 m_graphHistoryQueriedToMs = 0;
    GraphHistoryResult     m_graphHistory;
    std::shared_ptr<std::atomic_bool> m_cancelGraphQuery;
    /// Latest catalog rows per logger (from the bridge snapshot or a manual
    /// threshold refresh). Used by the summary mini charts for thresholds.
    QHash<qint64, QVector<Data::LoggerSensor>> m_catalogCache;
    QVariantList           m_readingsChartPlotPoints;
    QVariantMap            m_readingsChartAxis;
    bool                   m_readingsChartHasData = false;
    QTimer                 m_purgeTimer;
    bool                   m_purgeRunning = false;
    bool                   m_chartQueryRunning = false; // H-E coalesce guard
};

} // namespace CentralLogger::Core
