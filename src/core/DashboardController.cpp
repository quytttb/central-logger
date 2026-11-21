#include "DashboardController.h"

#include "utils/events/EventLevels.h"

#include "AppState.h"
#include "SettingsController.h"
#include "core/charts/ChartPresentation.h"
#include "core/charts/ChartQueryService.h"
#include "core/sensors/SensorMerger.h"
#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/models/SystemEvent.h"
#include "data/repositories/EventRepository.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "data/repositories/SettingsRepository.h"
#include "network/modbus/ModbusBridge.h"
#include "network/modbus/ModbusService.h"
#include "network/modbus/ModbusTypes.h"
#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

#include <QDateTime>
#include <QFuture>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QMap>
#include <QQmlEngine>
#include <QSqlQuery>
#include <QTimeZone>
#include <QTimer>
#include <QVector>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace CentralLogger::Core {

using CentralLogger::Utils::displayLevelForEvent;
using CentralLogger::Defaults::kPurgeIntervalMs;
using CentralLogger::Defaults::kVacuumChunkPages;
using CentralLogger::Defaults::kMaxVacuumIterations;

namespace {

DashboardController *g_instance = nullptr;

// Retention purge cadence — hourly (Task 16 / FE-016). kPurgeIntervalMs in Defaults.
// Number of pages freed per PRAGMA incremental_vacuum step. kVacuumChunkPages in Defaults.
// Upper bound on incremental_vacuum iterations per purge cycle. kMaxVacuumIterations in Defaults.

struct PurgeResult {
  int deletedReadings = 0;
  int deletedEvents   = 0;
  QString error;
};

/// Runs the retention purge (and incremental_vacuum) on a dedicated
/// connection so the chunked DELETEs + WAL write lock are off the UI thread
/// (audit H-B).
PurgeResult executeRetentionPurge(const QString &dbPath, const QDateTime &cutoff)
{
  PurgeResult result;
  QString openErr;
  if (!Data::Database::withScopedConnection(
          QStringLiteral("retention_purge"),
          dbPath,
          [&](QSqlDatabase &db) {
            {
              Data::SensorReadingRepository repo(db);
              result.deletedReadings = repo.purgeOlderThan(cutoff, &result.error);
            }
            if (result.error.isEmpty()) {
              Data::EventRepository events(db);
              result.deletedEvents = events.purgeOlderThan(cutoff, &result.error);
            }

            if (result.deletedReadings + result.deletedEvents > 0) {
              // Reclaim freed pages in chunks; incremental_vacuum only works when
              // auto_vacuum = INCREMENTAL (ensured by Database::open).
              QSqlQuery q(db);
              int freeList = -1;
              if (q.exec(QStringLiteral("PRAGMA freelist_count")) && q.next()) {
                freeList = q.value(0).toInt();
              }
              for (int iters = 0; iters < kMaxVacuumIterations && freeList > 0; ++iters) {
                if (!q.exec(QStringLiteral("PRAGMA incremental_vacuum(%1)")
                                .arg(kVacuumChunkPages))) {
                  break;
                }
                freeList = -1;
                if (q.exec(QStringLiteral("PRAGMA freelist_count")) && q.next()) {
                  freeList = q.value(0).toInt();
                }
              }
            }
          },
          &openErr)) {
    result.error = openErr;
  }
  return result;
}

/// H-E: run the 24h chart COUNT query on a dedicated throw-away connection
/// (thread pool) so the GROUP BY scan never blocks the UI thread.
QVector<ReadingBucketPoint> executeChartQuery(const QString &dbPath,
                                              const QTimeZone &tz,
                                              int bucketMinutes)
{
  QVector<ReadingBucketPoint> points;
  Data::Database::withScopedConnection(
      QStringLiteral("chart_query"),
      dbPath,
      [&](QSqlDatabase &db) {
        ChartQueryService svc(db);
        points = svc.readingCountsLast24h(bucketMinutes, tz);
      });
  return points;
}

} // namespace

DashboardController::DashboardController(QObject *parent) : QObject(parent) {
  // Hourly retention purge timer — Task 16 (FE-016).
  m_purgeTimer.setInterval(kPurgeIntervalMs);
  connect(&m_purgeTimer, &QTimer::timeout, this,
          &DashboardController::purgeOldData);
  m_purgeTimer.start();
}

DashboardController::~DashboardController() {
  if (m_cancelGraphQuery)
    m_cancelGraphQuery->store(true, std::memory_order_relaxed);
  // Do not leave in-flight SQLite work behind at shutdown.
  const QList<QFutureWatcherBase *> watchers =
      findChildren<QFutureWatcherBase *>();
  for (QFutureWatcherBase *w : watchers) {
    if (w->isRunning()) {
      w->waitForFinished();
    }
  }
}

void DashboardController::setDatabase(Data::Database *db) {
  m_db = db;
  m_loggers.setDatabase(db);
  m_recentEvents.setDatabase(db);
}

void DashboardController::setModbusBridge(Network::ModbusBridge *bridge) {
  m_bridge = bridge;
}

void DashboardController::invalidateBridgeCatalogCache(qint64 loggerId) {
  // Audit H-A: the bridge's catalog cache lives on the bridge thread — clear
  // it via a queued invocation after any CRUD that touched logger_sensor.
  if (!m_bridge)
    return;
  QMetaObject::invokeMethod(m_bridge, "invalidateCatalogCache",
                            Qt::QueuedConnection, Q_ARG(qint64, loggerId));
}

void DashboardController::setModbusService(Network::ModbusService *service) {
  m_modbus = service;
}

void DashboardController::setSettingsController(SettingsController *settings) {
  if (m_settings == settings)
    return;
  if (m_settings) {
    disconnect(m_settings, nullptr, this, nullptr);
  }
  m_settings = settings;
  if (m_settings) {
    // Purge when settings are saved (retention days may have changed).
    connect(m_settings, &SettingsController::saved, this,
            &DashboardController::purgeOldData);
    connect(m_settings, &SettingsController::saved, this,
            &DashboardController::refreshLiveModels);
    // Re-format chart timestamps when timezone changes.
    connect(m_settings, &SettingsController::systemTimezoneChanged, this,
            &DashboardController::refreshReadingsChart);
    connect(m_settings, &SettingsController::systemTimezoneChanged, this,
            &DashboardController::refreshLiveModels);
    connect(m_settings, &SettingsController::systemTimezoneChanged, this,
            &DashboardController::refreshSummary);
    // Summary view follows the configured primary logger immediately.
    connect(m_settings, &SettingsController::primaryLoggerIdChanged, this,
            &DashboardController::refreshSummary);
  }
}

void DashboardController::setInstance(DashboardController *controller) {
  g_instance = controller;
}

DashboardController *DashboardController::create(QQmlEngine *, QJSEngine *) {
  Q_ASSERT(g_instance);
  QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
  return g_instance;
}

void DashboardController::reloadLoggers() {
  m_loggers.reload();

  // L-22: seed m_lastStatus from the persisted DB status for loggers not
  // yet observed by a live Modbus poll. This ensures that the first poll
  // after startup can detect a genuine Online→Offline (or vice-versa)
  // transition and log the corresponding system_event, instead of silently
  // treating every initial snapshot as "first seen" (no prevStatus).
  const int n = m_loggers.rowCount();
  for (int i = 0; i < n; ++i) {
    const QModelIndex idx = m_loggers.index(i, 0);
    const qint64 id = m_loggers.data(idx, LoggerListModel::IdRole).toLongLong();
    if (!m_lastStatus.contains(id)) {
      const QString status =
          m_loggers.data(idx, LoggerListModel::StatusRole).toString();
      if (!status.isEmpty()) {
        m_lastStatus.insert(id, status);
      }
    }
    if (m_db && m_db->isOpen() && id > 0) {
      Data::SensorCatalogRepository catalogRepo(m_db->connection());
      m_catalogCache.insert(id, catalogRepo.listByLoggerId(id));
    }
  }

  syncModbusRegistry();
  if (m_appState) {
    m_appState->refreshFromDatabase();
  }
  refreshLiveModels();
  refreshSummary();
}

qint64 DashboardController::resolvePrimaryLoggerId() const {
  const qint64 configured = m_settings ? m_settings->primaryLoggerId()
                                       : Defaults::kNoPrimaryLoggerId;
  const int n = m_loggers.rowCount();
  qint64 firstId = Defaults::kNoPrimaryLoggerId;
  qint64 firstEnabledId = Defaults::kNoPrimaryLoggerId;
  qint64 firstOnlineId = Defaults::kNoPrimaryLoggerId;

  for (int i = 0; i < n; ++i) {
    const QModelIndex idx = m_loggers.index(i, 0);
    const qint64 id = m_loggers.data(idx, LoggerListModel::IdRole).toLongLong();
    if (id <= 0) continue;
    if (firstId == Defaults::kNoPrimaryLoggerId) firstId = id;
    const bool enabled = m_loggers.data(idx, LoggerListModel::EnabledRole).toBool();
    if (enabled && firstEnabledId == Defaults::kNoPrimaryLoggerId) {
      firstEnabledId = id;
    }
    const bool online = enabled && m_loggers.data(idx, LoggerListModel::OnlineRole).toBool();
    if (online && firstOnlineId == Defaults::kNoPrimaryLoggerId) {
      firstOnlineId = id;
    }
    if (configured > 0 && id == configured) {
      return configured;
    }
  }
  // Configured logger missing (deleted) or unset → deterministic fallback.
  if (firstOnlineId != Defaults::kNoPrimaryLoggerId) {
    return firstOnlineId;
  }
  return firstEnabledId != Defaults::kNoPrimaryLoggerId ? firstEnabledId : firstId;
}

qint64 DashboardController::graphLoggerId() const {
  // An explicit dropdown choice wins while that logger still exists;
  // otherwise the graph follows the configured primary logger.
  if (m_graphLoggerId > 0 && m_loggers.indexOfLogger(m_graphLoggerId) >= 0) {
    return m_graphLoggerId;
  }
  return resolvePrimaryLoggerId();
}

void DashboardController::setGraphLoggerId(qint64 id) {
  if (id > 0 && m_loggers.indexOfLogger(id) < 0) {
    return;
  }
  const qint64 explicitId = id > 0 ? id : Defaults::kNoPrimaryLoggerId;
  if (m_graphLoggerId == explicitId) {
    return;
  }
  m_graphLoggerId = explicitId;
  refreshSummary();
}

void DashboardController::refreshGraphLoggerOptions() {
  QVariantList options;
  const int n = m_loggers.rowCount();
  options.reserve(n);
  for (int i = 0; i < n; ++i) {
    const QModelIndex idx = m_loggers.index(i, 0);
    const qint64 id = m_loggers.data(idx, LoggerListModel::IdRole).toLongLong();
    if (id <= 0) {
      continue;
    }
    QVariantMap option;
    option.insert(QStringLiteral("id"), id);
    option.insert(QStringLiteral("name"),
                  m_loggers.data(idx, LoggerListModel::NameRole).toString());
    option.insert(QStringLiteral("online"),
                  m_loggers.data(idx, LoggerListModel::OnlineRole).toBool());
    option.insert(QStringLiteral("status"),
                  m_loggers.data(idx, LoggerListModel::StatusRole).toString());
    options.append(option);
  }
  if (options == m_graphLoggerOptions) {
    return;
  }
  m_graphLoggerOptions = std::move(options);
  emit graphLoggerOptionsChanged();
}

void DashboardController::refreshSummary() {
  // Drop an explicit graph choice whose logger was removed so the graph
  // falls back to the configured primary logger.
  if (m_graphLoggerId > 0 && m_loggers.indexOfLogger(m_graphLoggerId) < 0) {
    m_graphLoggerId = Defaults::kNoPrimaryLoggerId;
  }
  const qint64 graphId = graphLoggerId();
  if (m_lastGraphLoggerId != graphId) {
    m_lastGraphLoggerId = graphId;
    emit graphLoggerChanged();
  }

  refreshGraphLoggerOptions();
  refreshGraphChart();
  refreshOverviewTable();
}

QString DashboardController::graphWindowLabel() const {
  const int minutes = graphWindowMinutes();
  constexpr int minutesPerHour = Defaults::kSecondsPerHour / Defaults::kSecondsPerMinute;
  constexpr int minutesPerDay = Defaults::kHoursPerDay * minutesPerHour;
  if (minutes >= minutesPerDay)
    return tr("Last %1 d").arg(minutes / minutesPerDay);
  if (minutes >= minutesPerHour)
    return tr("Last %1 h").arg(minutes / minutesPerHour);
  return tr("Last %1 min").arg(minutes);
}

void DashboardController::setGraphActive(bool active) {
  if (m_graphActive == active)
    return;
  m_graphActive = active;
  if (active) {
    refreshSummary();
  } else {
    if (m_cancelGraphQuery)
      m_cancelGraphQuery->store(true, std::memory_order_relaxed);
    setGraphHistoryState(false, {});
  }
}

void DashboardController::zoomGraphIn() {
  if (canZoomGraphIn())
    setGraphZoomIndex(m_graphZoomIndex - 1);
}

void DashboardController::zoomGraphOut() {
  if (canZoomGraphOut())
    setGraphZoomIndex(m_graphZoomIndex + 1);
}

void DashboardController::resetGraphZoom() {
  setGraphZoomIndex(Defaults::kGraphDefaultZoomIndex);
}

void DashboardController::setGraphZoomIndex(std::size_t index) {
  if (m_graphZoomIndex != index) {
    m_graphZoomIndex = index;
    emit graphWindowChanged();
    refreshGraphChart();
  }
}

void DashboardController::refreshGraphHistory() {
  refreshGraphChart(true);
}

void DashboardController::setGraphHistoryState(bool loading, const QString &error) {
  if (loading != m_graphHistoryLoading || error != m_graphHistoryError) {
    m_graphHistoryLoading = loading;
    m_graphHistoryError = error;
    emit graphHistoryStateChanged();
  }
}

void DashboardController::startGraphHistoryQuery(const QString &dbPath, GraphWindow window,
                                                 qint64 fromMs, qint64 toMs) {
  auto cancel = std::make_shared<std::atomic_bool>(false);
  m_cancelGraphQuery = cancel;
  m_graphQueryWindow = window;
  auto *watcher = new QFutureWatcher<GraphHistoryResult>(this);
  connect(watcher, &QFutureWatcher<GraphHistoryResult>::finished, this,
          [this, watcher, cancel, window, toMs]() {
    m_cancelGraphQuery.reset();
    if (!cancel->load(std::memory_order_relaxed) && m_graphActive
        && window == GraphWindow{graphLoggerId(), graphWindowMinutes()}) {
      m_graphHistory = watcher->result();
      if (!m_graphHistory.error.isEmpty())
        qWarning() << "Graph history query:" << m_graphHistory.error;
      m_graphHistoryWindow = window;
      m_graphHistoryQueriedToMs = toMs;
    }
    watcher->deleteLater();
    refreshGraphChart();
  });
  watcher->setFuture(QtConcurrent::run([dbPath, window, fromMs, toMs, cancel]() {
    return GraphHistoryQueryService::query(dbPath, window.loggerId, fromMs, toMs, *cancel);
  }));
}

void DashboardController::refreshGraphChart(bool forceQuery) {
  if (!m_graphActive)
    return;

  const qint64 graphId = graphLoggerId();
  const GraphWindow window{graphId, graphWindowMinutes()};
  const qint64 toMs = std::max(QDateTime::currentMSecsSinceEpoch(),
                               m_pollHistory.latestTimestamp(graphId));
  const qint64 spanMs = qint64(window.minutes) * Defaults::kSecondsPerMinute * Defaults::kMsPerSecond;
  const qint64 fromMs = toMs - spanMs;
  const QString dbPath = m_db && m_db->isOpen() ? m_db->connection().databaseName() : QString{};
  const bool needsHistory = graphId > 0 && !dbPath.isEmpty() && dbPath != Data::Database::memoryPath()
      && (spanMs > Defaults::kGraphRamMaxMs || !m_pollHistory.coversStart(graphId, fromMs));
  const bool matchingHistory = needsHistory && m_graphHistoryWindow == window;
  if (m_cancelGraphQuery && (!needsHistory || m_graphQueryWindow != window))
    m_cancelGraphQuery->store(true, std::memory_order_relaxed);

  // The RAM stream already covers new samples. Re-read SQLite only if some
  // samples newer than the cached query have now aged out, or on error/manual refresh.
  const bool cacheAged = m_pollHistory.latestEvictedTimestamp(graphId) > m_graphHistoryQueriedToMs;
  const bool mayRetry = toMs - m_graphHistoryQueriedToMs >= Defaults::kGraphSqlRefreshMs;
  if (needsHistory && !m_cancelGraphQuery
      && (forceQuery || !matchingHistory || (mayRetry && (cacheAged || !m_graphHistory.error.isEmpty())))) {
    startGraphHistoryQuery(dbPath, window, fromMs, toMs);
  }

  const bool loading = needsHistory && bool(m_cancelGraphQuery);
  setGraphHistoryState(loading, matchingHistory && !m_graphHistory.error.isEmpty()
      ? tr("Could not load stored readings.") : QString{});
  if (loading && !matchingHistory && m_displayedGraphWindow.loggerId == graphId
      && m_displayedGraphWindow != window && m_graphCharts.rowCount() > 0) {
    // Keep the current chart and its axis while zooming until SQLite returns.
    // Never hold over data from a different logger.
    return;
  }
  m_displayedGraphWindow = window;
  updateGraphChart(graphId, fromMs, toMs, matchingHistory ? &m_graphHistory.series : nullptr);
}

void DashboardController::updateGraphChart(qint64 graphId, qint64 fromMs, qint64 toMs,
                                            const GraphSeries *history) {
  using namespace CentralLogger::Ui;
  const QVariantMap axis{{QLatin1String(kChartXMin), double(fromMs)},
                         {QLatin1String(kChartXMax), double(toMs)}};
  const auto ram = m_pollHistory.seriesForLogger(graphId, fromMs, toMs);
  auto orderedIds = ram.keys();
  if (history) {
    for (auto it = history->cbegin(); it != history->cend(); ++it) {
      if (!ram.contains(it.key()))
        orderedIds.append(it.key());
    }
    std::sort(orderedIds.begin(), orderedIds.end());
  }

  // Catalog + live alarm rows for the graph logger.
  QHash<int, const Data::LoggerSensor *> catalogByEdge;
  const QVector<Data::LoggerSensor> catalog = m_catalogCache.value(graphId);
  for (const auto &sensor : catalog) {
    if (sensor.sensorType
        == QLatin1String(CentralLogger::Sensor::kTypeAnalog)) {
      catalogByEdge.insert(sensor.edgeSensorId, &sensor);
    }
  }
  QHash<int, bool> liveAlarmByEdge;
  for (const auto &liveRow : m_sensorCache.rowsFor(graphId)) {
    if (liveRow.sensorType == QLatin1String(CentralLogger::Sensor::kTypeAnalog)) {
      liveAlarmByEdge.insert(liveRow.edgeSensorId, !liveRow.alarmType.isEmpty());
    }
  }

  QVariantList sensorCharts;
  sensorCharts.reserve(orderedIds.size());
  for (const int edgeSensorId : orderedIds) {
    const Data::LoggerSensor *cat = catalogByEdge.value(edgeSensorId, nullptr);
    if (cat && !cat->active) {
      continue;
    }
    const auto live = ram.value(edgeSensorId);
    const auto stored = history ? history->value(edgeSensorId) : QVector<GraphPoint>{};
    Downsampler sampler(fromMs, toMs);
    // Prefer the finer live stream where it overlaps the persisted one.
    if (live.isEmpty()) {
      for (const auto &point : stored)
        sampler.append(point);
    } else {
      for (const auto &point : stored) {
        if (point.timestampMs >= live.constFirst().timestampMs)
          break;
        sampler.append(point);
      }
      for (const auto &point : live)
        sampler.append(point);
      for (const auto &point : stored) {
        if (point.timestampMs > live.constLast().timestampMs)
          sampler.append(point);
      }
    }
    const auto samples = sampler.takePoints();
    if (samples.isEmpty()) {
      continue;
    }

    const int decimals = std::clamp(
        cat ? cat->decimals : Defaults::kDecimalsDefault,
        CentralLogger::Defaults::kDecimalsMin,
        CentralLogger::Defaults::kDecimalsMax);
    const auto currentValue = m_pollHistory.currentValue(graphId, edgeSensorId);
    const double latest = currentValue.value_or(samples.constLast().value);
    const bool currentValid = currentValue.has_value();

    const bool hasMin = cat && cat->minThreshold.has_value();
    const bool hasMax = cat && cat->maxThreshold.has_value();
    const double minThreshold = hasMin ? *cat->minThreshold : 0.0;
    const double maxThreshold = hasMax ? *cat->maxThreshold : 0.0;
    const bool edgeAlarm = liveAlarmByEdge.value(edgeSensorId, false);
    // Single source of truth: alert state comes strictly from edge qualityAlarm,
    // never locally synthesized against mirrored thresholds.
    const bool outOfRange = currentValid && edgeAlarm;

    double lo = samples.constFirst().value;
    double hi = lo;
    QVariantList points;
    points.reserve(samples.size());
    for (const auto &sample : samples) {
      lo = std::min(lo, sample.value);
      hi = std::max(hi, sample.value);
      points.append(QVariantMap{{QLatin1String(kChartX), double(sample.timestampMs)},
                                {QLatin1String(kChartY), sample.value}});
    }
    // Data-driven window: the mini chart must use the full tile height for
    // the trend. A limit far away from the current readings would flatten
    // the line, so the axis only expands to include the max threshold when
    // it sits close to the data; otherwise SensorMiniChart draws an
    // off-scale marker. The min threshold is not drawn on the tile.
    double span = hi - lo;
    if (span <= 0.0) {
      const double resolution = std::pow(10.0, -decimals);
      span = std::max(std::abs(hi) * 0.02, resolution * 10.0);
    }
    const double pad = span * 0.15;
    double axisYMin = lo - pad;
    double axisYMax = hi + pad;
    if (hasMax && maxThreshold >= axisYMin
        && maxThreshold <= axisYMax + span * 2.0) {
      axisYMax = std::max(axisYMax, maxThreshold + pad);
    }

    QVariantMap chart;
    chart.insert(QLatin1String(CentralLogger::Ui::kChartEdgeSensorId),
                 edgeSensorId);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartLabel),
                 cat && !cat->name.isEmpty() ? cat->name : Sensor::fallbackSensorName(edgeSensorId));
    chart.insert(QLatin1String(CentralLogger::Ui::kChartUnit),
                 cat ? cat->unit : QString{});
    chart.insert(QLatin1String(CentralLogger::Ui::kChartDecimals), decimals);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartPoints), points);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartLatest), latest);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartLatestText),
                 currentValid ? CentralLogger::Sensor::formatAnalogValue(latest, decimals)
                              : QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder));
    chart.insert(QLatin1String(CentralLogger::Ui::kChartHasThresholds),
                 currentValid && (hasMin || hasMax));
    chart.insert(QLatin1String(CentralLogger::Ui::kChartHasMinThreshold), hasMin);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartHasMaxThreshold), hasMax);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartThresholdMin),
                 minThreshold);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartThresholdMax),
                 maxThreshold);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartOutOfRange), outOfRange);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartAxisYMin), axisYMin);
    chart.insert(QLatin1String(CentralLogger::Ui::kChartAxisYMax), axisYMax);
    sensorCharts.append(chart);
  }

  const bool axisChanged = m_graphChartAxis != axis;
  m_graphChartAxis = axis;
  m_graphCharts.setCharts(graphId, sensorCharts);
  if (axisChanged)
    emit graphChartChanged();
}

void DashboardController::refreshLoggerCatalog(qint64 loggerId) {
  if (!m_db || !m_db->isOpen() || loggerId <= 0) {
    return;
  }
  Data::SensorCatalogRepository repo(m_db->connection());
  QString error;
  const auto rows = repo.listByLoggerId(loggerId, &error);
  if (!error.isEmpty()) {
    qWarning() << "DashboardController::refreshLoggerCatalog:" << error;
    return;
  }
  m_catalogCache.insert(loggerId, rows);
  m_sensorCache.rebuildWithCatalog(loggerId, rows);

  // The bridge caches the catalog per logger; drop it so the next poll
  // uses the refreshed metadata and mirrored thresholds.
  invalidateBridgeCatalogCache(loggerId);
  refreshLiveModels();
  if (loggerId == graphLoggerId()) {
    refreshGraphChart();
  }
  refreshOverviewTable();
}

void DashboardController::refreshOverviewTable() {
  QVector<OverviewRow> rows;
  using OverviewKey = std::tuple<int, QString, QString>;
  QMap<OverviewKey, bool> sensorKeys;
  const qint64 graphId = graphLoggerId();

  const int n = m_loggers.rowCount();
  rows.reserve(n);

  for (int i = 0; i < n; ++i) {
    const QModelIndex idx = m_loggers.index(i, 0);
    const qint64 id = m_loggers.data(idx, LoggerListModel::IdRole).toLongLong();
    if (id <= 0 || id == graphId) {
      continue;
    }

    OverviewRow row;
    row.loggerId   = id;
    row.loggerName = m_loggers.data(idx, LoggerListModel::NameRole).toString();
    row.status     = m_loggers.data(idx, LoggerListModel::StatusRole).toString();
    row.online     = m_loggers.data(idx, LoggerListModel::OnlineRole).toBool();

    for (const auto &sensor : m_sensorCache.rowsFor(id)) {
      if (sensor.sensorType != QLatin1String(CentralLogger::Sensor::kTypeAnalog)) {
        continue;
      }
      row.values.insert(sensor.edgeSensorId, sensor.value);
      row.identities.insert(sensor.edgeSensorId, qMakePair(sensor.name, sensor.unit));
      sensorKeys.insert(OverviewKey{sensor.edgeSensorId, sensor.name, sensor.unit}, true);
    }
    rows.append(row);
  }

  QVector<OverviewColumn> columns;
  columns.reserve(sensorKeys.size());
  QHash<QString, int> headerCounts;
  auto headerFor = [](const QString &name, const QString &unit) {
    return unit.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, unit);
  };
  for (auto it = sensorKeys.constBegin(); it != sensorKeys.constEnd(); ++it) {
    const auto &key = it.key();
    ++headerCounts[headerFor(std::get<1>(key), std::get<2>(key))];
  }
  for (auto it = sensorKeys.constBegin(); it != sensorKeys.constEnd(); ++it) {
    const auto &key = it.key();
    OverviewColumn col;
    col.edgeSensorId = std::get<0>(key);
    col.sensorName = std::get<1>(key);
    col.sensorUnit = std::get<2>(key);
    col.matchMetadata = true;
    col.header = headerFor(col.sensorName, col.sensorUnit);
    if (headerCounts.value(col.header) > 1)
      col.header += QStringLiteral(" [ID %1]").arg(col.edgeSensorId);
    columns.append(col);
  }

  m_overviewTable.setData(rows, columns);
}

bool DashboardController::isLoggerOnline(qint64 loggerId) const {
  if (loggerId <= 0) {
    return false;
  }
  const int row = m_loggers.indexOfLogger(loggerId);
  if (row < 0) {
    return false;
  }
  const QModelIndex idx = m_loggers.index(row, 0);
  return m_loggers.data(idx, LoggerListModel::EnabledRole).toBool()
      && m_loggers.data(idx, LoggerListModel::OnlineRole).toBool();
}

void DashboardController::markLoggerDisabled(qint64 id) {
  if (id <= 0) {
    return;
  }
  m_sensorCache.markOffline(id);
  m_pollHistory.markOffline(id);
  m_lastStatus.insert(id, QLatin1String(CentralLogger::Sensor::kLoggerOffline));
  const int row = m_loggers.indexOfLogger(id);
  if (row >= 0) {
    const int sensorCount =
        m_loggers.data(m_loggers.index(row, 0), LoggerListModel::SensorCountRole).toInt();
    m_loggers.updateLoggerRow(
        id, QLatin1String(CentralLogger::Sensor::kLoggerOffline),
        sensorCount, false, false);
  }
  if (m_appState) {
    m_appState->updateAlarmState(id, false);
  }
  refreshLiveModels();
  refreshSummary();
  emit loggerSnapshotUpdated(id, false);
}

void DashboardController::refreshLiveModels() {
  const QTimeZone tz = m_settings
      ? m_settings->effectiveTimeZone()
      : SettingsController::resolveTimeZone();

  QVariantList sections;
  QVector<LoggerLiveListEntry> listEntries;

  const int n = m_loggers.rowCount();
  sections.reserve(n);

  for (int i = 0; i < n; ++i) {
    const QModelIndex idx = m_loggers.index(i, 0);
    const qint64 loggerId = m_loggers.data(idx, LoggerListModel::IdRole).toLongLong();
    if (loggerId <= 0) {
      continue;
    }

    const QString name        = m_loggers.data(idx, LoggerListModel::NameRole).toString();
    const QString stationCode = m_loggers.data(idx, LoggerListModel::StationCodeRole).toString();
    const QString host        = m_loggers.data(idx, LoggerListModel::HostRole).toString();
    const bool enabled        = m_loggers.data(idx, LoggerListModel::EnabledRole).toBool();
    const bool online         = enabled && m_loggers.data(idx, LoggerListModel::OnlineRole).toBool();
    const bool polling        = online && m_loggers.data(idx, LoggerListModel::PollingRole).toBool();
    const bool headerAlarm    = online && m_loggers.data(idx, LoggerListModel::AnyAlarmRole).toBool();

    const QVector<Data::LoggerSensor> catalog = m_catalogCache.value(loggerId);
    QHash<QString, const Data::LoggerSensor *> catByKey;
    catByKey.reserve(catalog.size());
    for (const auto &s : catalog) {
      catByKey.insert(QStringLiteral("%1:%2").arg(s.sensorType).arg(s.edgeSensorId), &s);
    }

    const QVector<SensorLiveRow> rawRows = m_sensorCache.has(loggerId)
        ? m_sensorCache.rowsFor(loggerId)
        : SensorMerger::buildCatalogPlaceholders(loggerId, catalog);

    QVariantList sensorMaps;
    sensorMaps.reserve(rawRows.size());
    bool anySensorAlarm = false;

    for (const auto &r : rawRows) {
      const Data::LoggerSensor *cat =
          catByKey.value(QStringLiteral("%1:%2").arg(r.sensorType).arg(r.edgeSensorId), nullptr);
      if (!cat && r.sensorType == QLatin1String(CentralLogger::Sensor::kTypeUnknown)) {
        cat = catByKey.value(
            QStringLiteral("%1:%2")
                .arg(QLatin1String(CentralLogger::Sensor::kTypeAnalog))
                .arg(r.edgeSensorId),
            nullptr);
      }
      if (cat && !cat->active) {
        continue;
      }

      const bool isAnalog =
          (r.sensorType == QLatin1String(CentralLogger::Sensor::kTypeAnalog));
      const bool isDigital =
          (r.sensorType == QLatin1String(CentralLogger::Sensor::kTypeDi)
           || r.sensorType == QLatin1String(CentralLogger::Sensor::kTypeDo));
      const bool hasMin = isAnalog && cat && cat->minThreshold.has_value();
      const bool hasMax = isAnalog && cat && cat->maxThreshold.has_value();
      const bool hasLimit = hasMin || hasMax;
      const QString compactLimit =
          hasLimit ? CentralLogger::Sensor::formatCompactLimit(cat->minThreshold, cat->maxThreshold)
                   : QString{};
      const QString cardLimit =
          hasLimit ? CentralLogger::Sensor::formatBracketLimit(cat->minThreshold, cat->maxThreshold)
                   : QString{};

      const bool rowValid = online && r.valid;
      const QString displayVal = rowValid
          ? r.value
          : QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
      const QString displayStat = online ? r.displayStatus : QString{};
      const QString alarmTyp    = online ? r.alarmType : QString{};
      const QStringList diCodes  = online ? r.attachDiTypeCodes : QStringList{};
      const QStringList diLabels = online ? r.attachDiTypeLabels : QStringList{};
      const bool digitalOn = isDigital && rowValid
          && r.value == QLatin1String(CentralLogger::Sensor::kBitOn);

      if (!alarmTyp.isEmpty()
          || displayStat == QLatin1String(CentralLogger::Sensor::kStatusErr)) {
        anySensorAlarm = true;
      }

      QVariantMap sm;
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleEdgeSensorId), r.edgeSensorId);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleName), r.name);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleUnit), r.unit);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleValue), displayVal);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleSensorType), r.sensorType);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleDisplayStatus), displayStat);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleStatus03Auto), r.status03Auto);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleAlarmType), alarmTyp);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleAttachDiCodes), diCodes);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleAttachDiLabels), diLabels);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleHasLimit), hasLimit);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleLimitText), cardLimit);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleValid), rowValid);
      sm.insert(QLatin1String(CentralLogger::Ui::kRoleDigitalOn), digitalOn);
      sensorMaps.append(sm);

      LoggerLiveListEntry entry;
      entry.loggerId           = loggerId;
      entry.loggerName         = name;
      entry.stationCode        = stationCode;
      entry.host               = host;
      entry.online             = online;
      entry.edgeSensorId       = r.edgeSensorId;
      entry.sensorName         = r.name;
      entry.sensorType         = r.sensorType;
      entry.value              = displayVal;
      entry.unit               = r.unit;
      entry.displayStatus      = displayStat;
      entry.status03Auto       = r.status03Auto;
      entry.alarmType          = alarmTyp;
      entry.attachDiTypeCodes  = diCodes;
      entry.attachDiTypeLabels = diLabels;
      entry.hasLimit           = hasLimit;
      entry.limitText          = compactLimit;
      entry.valid              = rowValid;
      listEntries.append(std::move(entry));
    }

    QString lastUpdate;
    if (online) {
      const QDateTime dt = m_sensorCache.measuredAtFor(loggerId);
      if (dt.isValid()) {
        lastUpdate = dt.toTimeZone(tz).toString(
            QLatin1String(CentralLogger::Format::kDateDdMmYyyyHm));
      }
    }

    QVariantMap sec;
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleLoggerId), QVariant::fromValue(loggerId));
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleName), name);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleStationCode), stationCode);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleHost), host);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleEnabled), enabled);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleOnline), online);
    sec.insert(QLatin1String(CentralLogger::Ui::kRolePolling), polling);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleAnyAlarm), headerAlarm || anySensorAlarm);
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleSensorCount), int(sensorMaps.size()));
    sec.insert(QLatin1String(CentralLogger::Ui::kRoleLastUpdate), lastUpdate);
    sec.insert(QStringLiteral("sensors"), sensorMaps);
    sections.append(sec);
  }

  m_liveList.setEntries(std::move(listEntries));
  m_liveCards.setSections(std::move(sections));
}

void DashboardController::setLiveFilterText(const QString &text) {
  m_liveCards.setFilterText(text);
  m_liveList.setFilterText(text);
}

void DashboardController::reloadRecentEvents() { m_recentEvents.reload(); }

void DashboardController::refreshReadingsChart() {
  if (!m_db || !m_db->isOpen())
    return;

  // L-20: use the configured system_timezone so chart labels show local
  // time instead of UTC. Fall back to the Qt system timezone when unset.
  const QTimeZone tz = m_settings
      ? m_settings->effectiveTimeZone()
      : SettingsController::resolveTimeZone();

  // H-E (audit): the 24h GROUP BY scan is comparatively heavy; run it off
  // the UI thread on a dedicated connection and apply the result when the
  // background query finishes. Coalesce with m_chartQueryRunning so a fast
  // timer/trigger storm can't stack concurrent queries.
  if (m_chartQueryRunning)
    return;
  m_chartQueryRunning = true;
  const QString dbPath = m_db->connection().databaseName();
  const bool inMemory = (dbPath == Data::Database::memoryPath());

  auto applyPoints = [this, tz](const QVector<ReadingBucketPoint> &points) {
    QVariantList data;
    data.reserve(points.size());
    for (const auto &pt : points) {
      QVariantMap m;
      m.insert(QLatin1String(CentralLogger::Ui::kChartLabel), pt.label);
      m.insert(QLatin1String(CentralLogger::Ui::kChartBucketMs), pt.bucketMs);
      m.insert(QLatin1String(CentralLogger::Ui::kChartCount), pt.count);
      data.append(m);
    }
    const auto presentation =
        buildReadingsChartPresentation(data, Defaults::kChartDisplayPointCount,
                                      CentralLogger::Defaults::kChartDefaultBucketMin, tz);
    m_readingsChartPlotPoints = presentation.plotPoints;
    m_readingsChartAxis = presentation.axis;
    m_readingsChartHasData = false;
    for (const auto &pt : points) {
      if (pt.count > 0) {
        m_readingsChartHasData = true;
        break;
      }
    }
    emit readingsChartChanged();
  };

  if (inMemory) {
    // :memory: DBs are per-connection; query synchronously on the main conn.
    ChartQueryService svc(m_db->connection());
    applyPoints(svc.readingCountsLast24h(CentralLogger::Defaults::kChartDefaultBucketMin, tz));
    m_chartQueryRunning = false;
    return;
  }

  auto *watcher = new QFutureWatcher<QVector<ReadingBucketPoint>>(this);
  connect(watcher, &QFutureWatcher<QVector<ReadingBucketPoint>>::finished,
          this, [this, watcher, applyPoints]() {
            const auto points = watcher->result();
            watcher->deleteLater();
            m_chartQueryRunning = false;
            applyPoints(points);
          });
  QFuture<QVector<ReadingBucketPoint>> future =
      QtConcurrent::run(executeChartQuery, dbPath, tz,
                       CentralLogger::Defaults::kChartDefaultBucketMin);
  watcher->setFuture(future);
}

void DashboardController::purgeOldData() {
  if (!m_db || !m_db->isOpen())
    return;

  // Coalesce overlapping triggers (hourly timer + settings saved).
  if (m_purgeRunning) {
    return;
  }
  m_purgeRunning = true;

  // Determine retention days: prefer the live SettingsController value,
  // fall back to reading from the DB directly.
  int retentionDays = CentralLogger::Defaults::kDefaultRetentionDays;
  if (m_settings) {
    retentionDays = m_settings->dataRetentionDays();
  } else {
    Data::SettingsRepository settingsRepo(m_db->connection());
    const auto s = settingsRepo.get();
    retentionDays = s.dataRetentionDays;
  }
  if (retentionDays <= 0)
    retentionDays = 1;

  const QDateTime cutoff =
      QDateTime::currentDateTimeUtc().addDays(-retentionDays);

  // Audit H-B: purge on a background thread with its own connection so the
  // chunked DELETE + incremental_vacuum never block the UI.
  // In-memory databases keep synchronous execution: a throw-away QSQLITE
  // connection to ":memory:" would see a different, empty database.
  const QString dbPath = m_db->connection().databaseName();
  if (dbPath == Data::Database::memoryPath()) {
    PurgeResult result;
    {
      Data::SensorReadingRepository readings(m_db->connection());
      result.deletedReadings = readings.purgeOlderThan(cutoff, &result.error);
      if (result.error.isEmpty()) {
        Data::EventRepository events(m_db->connection());
        result.deletedEvents = events.purgeOlderThan(cutoff, &result.error);
      }
    }
    if (!result.error.isEmpty()) {
      m_purgeRunning = false;
      qWarning() << "DashboardController::purgeOldData error:" << result.error;
      return;
    }
    const int totalDeleted = result.deletedReadings + result.deletedEvents;
    m_purgeRunning = false;
    if (totalDeleted > 0) {
      refreshReadingsChart();
    }
    return;
  }

  auto *watcher = new QFutureWatcher<PurgeResult>(this);
  connect(watcher, &QFutureWatcher<PurgeResult>::finished, this,
          [this, watcher]() {
            m_purgeRunning = false;
            const PurgeResult result = watcher->result();
            watcher->deleteLater();

            if (!result.error.isEmpty()) {
              qWarning() << "DashboardController::purgeOldData error:"
                         << result.error;
              return;
            }

            const int totalDeleted =
                result.deletedReadings + result.deletedEvents;
            if (totalDeleted > 0) {
              refreshReadingsChart();
            }
          });
  QFuture<PurgeResult> future =
      QtConcurrent::run(executeRetentionPurge, dbPath, cutoff);
  watcher->setFuture(future);
}

void DashboardController::startModbusPolling() { syncModbusRegistry(); }

void DashboardController::reconnectLogger(qint64 loggerId) {
  if (m_modbus) {
    QMetaObject::invokeMethod(m_modbus, &Network::ModbusService::reconnectLogger,
                              Qt::QueuedConnection, loggerId);
  }
}

void DashboardController::syncModbusRegistry() {
  if (!m_modbus || !m_db || !m_db->isOpen())
    return;

  Data::LoggerRepository repo(m_db->connection());
  const auto rows = repo.findAll();

  QVector<Network::LoggerRuntimeConfig> configs;
  configs.reserve(rows.size());
  for (const auto &info : rows) {
    if (!info.enabled) {
      // M-19 fix: disabled loggers never produce Modbus snapshots, so
      // their alarm entry in AppState is never updated. Clear it here so
      // AppState.alarmCount doesn't show phantom alarms after disabling.
      if (m_appState) {
        m_appState->updateAlarmState(info.id, false);
      }
      continue;
    }
    Network::LoggerRuntimeConfig c;
    c.loggerId = info.id;
    c.host = info.host;
    c.modbusPort = info.modbusPort;
    c.unitId = info.modbusUnitId;
    c.pollIntervalMs =
        (info.centralPollIntervalS > 0 ? info.centralPollIntervalS
                                       : Defaults::kDefaultPollIntervalSec)
        * Defaults::kMsPerSecond;
    c.timeoutMs = static_cast<int>(
        info.timeoutS > 0 ? info.timeoutS * Defaults::kMsPerSecond
                          : Defaults::kDefaultTimeoutMs);
    c.enabled = info.enabled;
    configs.append(c);
  }
  QMetaObject::invokeMethod(
      m_modbus, "syncLoggers", Qt::QueuedConnection,
      Q_ARG(QVector<CentralLogger::Network::LoggerRuntimeConfig>, configs));
}

void DashboardController::onSnapshotApplied(
    const Network::PollSnapshot &snapshot, int sensorCount,
    const QVector<Data::LoggerSensor> &catalogRows) {
  const qint64 loggerId = snapshot.loggerId;
  const int loggerRow = m_loggers.indexOfLogger(loggerId);
  if (loggerRow >= 0
      && !m_loggers.data(m_loggers.index(loggerRow, 0), LoggerListModel::EnabledRole).toBool()) {
    return;
  }
  const bool online = snapshot.success;
  const QString newStatus =
      online ? QLatin1String(CentralLogger::Sensor::kLoggerOnline)
             : QLatin1String(CentralLogger::Sensor::kLoggerOffline);

  // Snapshot the previous status before patching the list model — once
  // updateLoggerRow runs the row reflects the new state.
  const auto prevIt = m_lastStatus.constFind(loggerId);
  const QString prevStatus =
      prevIt != m_lastStatus.constEnd() ? *prevIt : QString();

  bool anySensorAlarm = false;
  if (online) {
    for (const auto &a : snapshot.analogs) {
      if (a.isValid() && !a.isStale() && a.isAlarm()) {
        anySensorAlarm = true;
        break;
      }
    }
  }
  const bool effectiveAlarm = online && (snapshot.header.isAnyAlarm() || anySensorAlarm);

  m_loggers.updateLoggerRow(
      loggerId, newStatus, sensorCount, snapshot.header.isPolling(),
      effectiveAlarm);

  if (online) {
    // Audit H-A: catalog rows arrive pre-fetched from the bridge thread;
    // the UI thread no longer touches the DB on this path.
    m_sensorCache.apply(snapshot, catalogRows);
    if (!catalogRows.isEmpty()) {
      m_catalogCache.insert(loggerId, catalogRows);
    }

  } else {
    m_sensorCache.markOffline(loggerId);
  }

  maybeLogStatusTransition(loggerId, prevStatus, newStatus);
  m_lastStatus.insert(loggerId, newStatus);

  m_pollHistory.append(snapshot);
  refreshLiveModels();

  // Dashboard summary: refresh the graph only for the selected logger;
  // the secondary-logger table follows every snapshot (and status changes).
  const qint64 graphId = graphLoggerId();
  if (graphId != m_lastGraphLoggerId) {
    refreshSummary();
  } else {
    if (loggerId == graphId) {
      refreshGraphChart();
    }
    refreshGraphLoggerOptions();
    refreshOverviewTable();
  }

  emit loggerSnapshotUpdated(loggerId, snapshot.success);

  if (m_appState) {
    // Push live alarm state so AppState.alarmCount reflects current device
    // state, not historical system_event rows. updateAlarmState already
    // triggers a refreshFromDatabase() when the alarm bit flips; here we
    // also refresh when the status itself transitioned so totalLoggers /
    // onlineLoggers stay accurate without forcing two COUNT queries per
    // poll on the UI thread when nothing changed.
    m_appState->updateAlarmState(loggerId, effectiveAlarm);
    if (prevStatus != newStatus) {
      m_appState->refreshFromDatabase();
    }
  }
}

void DashboardController::maybeLogStatusTransition(qint64 loggerId,
                                                   const QString &prevStatus,
                                                   const QString &newStatus) {
  if (!m_db || !m_db->isOpen())
    return;
  if (prevStatus == newStatus)
    return;
  if (prevStatus.isEmpty()) {
    // First snapshot after app start. Log Online so the user sees the
    // initial connection succeed. Skip Offline — the DB default is
    // already 'offline' and logging it would generate spurious events
    // for every unreachable logger at startup.
    if (newStatus != CentralLogger::Sensor::kLoggerOnline)
      return;
    // Fall through to log the Online event below.
  }

  const int row = m_loggers.indexOfLogger(loggerId);
  QString stationCode;
  QString name;
  if (row >= 0) {
    const QModelIndex idx = m_loggers.index(row, 0);
    stationCode =
        m_loggers.data(idx, LoggerListModel::StationCodeRole).toString();
    name = m_loggers.data(idx, LoggerListModel::NameRole).toString();
  }
  const QString label =
      stationCode.isEmpty()
          ? (name.isEmpty() ? QStringLiteral("#%1").arg(loggerId) : name)
          : stationCode;

  Data::EventRepository events(m_db->connection());
  Data::SystemEvent ev;
  ev.loggerId = loggerId;
  if (newStatus == CentralLogger::Sensor::kLoggerOnline) {
    ev.eventType = CentralLogger::Sensor::kEventTypeOnline;
    ev.level = CentralLogger::Sensor::kLevelInfo;
    ev.message = QStringLiteral("Logger %1 is online").arg(label);
  } else {
    ev.eventType = CentralLogger::Sensor::kEventTypeOffline;
    ev.level = CentralLogger::Sensor::kLevelWarning;
    ev.message = QStringLiteral("Logger %1 went offline").arg(label);
  }
  if (events.insert(ev)) {
    m_recentEvents.reload();
  }
}

void DashboardController::cleanupRemovedLogger(qint64 id) {
  m_sensorCache.remove(id);
  m_pollHistory.remove(id);
  m_lastStatus.remove(id);
  m_catalogCache.remove(id);
  invalidateBridgeCatalogCache(id);
  if (m_appState) {
    m_appState->removeLogger(id);
  }
  if (m_graphLoggerId == id) {
    m_graphLoggerId = Defaults::kNoPrimaryLoggerId;
  }
  refreshLiveModels();
  refreshSummary();
}

void DashboardController::logEvent(qint64 loggerId, const QString &eventType,
                                   const QString &message) {
  if (!m_db || !m_db->isOpen())
    return;
  Data::EventRepository events(m_db->connection());
  Data::SystemEvent ev;
  if (loggerId > 0) {
    ev.loggerId = loggerId;
  }
  ev.eventType = eventType;
  ev.message = message;
  ev.level = displayLevelForEvent(eventType, QString{});
  events.insert(ev);
  m_recentEvents.reload();
}

void DashboardController::afterMutation() {
  reloadLoggers();
  syncModbusRegistry();
  reloadRecentEvents();
  // Audit H-A: catalog mutations can change names/thresholds the bridge
  // cached — drop every cached entry so the next poll rebuilds from the current
  // catalog.
  invalidateBridgeCatalogCache(0);
  // L-14: refresh the readings chart after any CRUD operation that can
  // change the set of active loggers (same pattern as purgeOldData).
  refreshReadingsChart();
  if (m_appState) {
    m_appState->refreshFromDatabase();
  }
  refreshSummary();
}

QVariantMap DashboardController::snapReadingsChart(double mouseX, double mouseY,
                                                   double plotX, double plotY,
                                                   double plotW,
                                                   double plotH) const {
  const double xMin =
      m_readingsChartAxis.value(CentralLogger::Ui::kChartXMin).toDouble();
  const double xMax =
      m_readingsChartAxis.value(CentralLogger::Ui::kChartXMax).toDouble();
  return Core::snapReadingsChart(m_readingsChartPlotPoints, xMin, xMax, plotX,
                                 plotY, plotW, plotH, mouseX, mouseY,
                                 CentralLogger::Defaults::kChartDefaultBucketMin);
}

} // namespace CentralLogger::Core
