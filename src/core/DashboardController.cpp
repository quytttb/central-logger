#include "DashboardController.h"

#include "utils/events/EventLevels.h"

#include "AppState.h"
#include "SettingsController.h"
#include "core/charts/ChartPresentation.h"
#include "core/charts/ChartQueryService.h"
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
#include "utils/DbConstants.h"
#include "utils/SensorConstants.h"
#include "utils/charts/ChartDisplayLimits.h"

#include <QDateTime>
#include <QFuture>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QSqlError>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QVector>
#include <QtConcurrent>
#include <QSqlQuery>

namespace CentralLogger::Core {

using CentralLogger::Utils::displayLevelForEvent;
using CentralLogger::Utils::kChartDisplayPointCount;
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
/// (audit H-B). Follows the same throw-away-connection pattern used by
/// HistoryViewModel::executeHistorySearch.
PurgeResult executeRetentionPurge(const QString &dbPath, const QDateTime &cutoff)
{
  PurgeResult result;

  const QString connName = QStringLiteral("retention_purge_%1").arg(
      reinterpret_cast<quintptr>(QThread::currentThreadId()));
  QSqlDatabase db = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), connName);
  db.setDatabaseName(dbPath);
  if (!db.open()) {
    result.error = db.lastError().text();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    return result;
  }

  Data::Database::applyPerformancePragmas(db, nullptr);

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
    // Bound the loop: incremental_vacuum returns success even when no
    // pages are freed (e.g. another writer is holding a lock); without
    // a cap we could spin forever on a misconfigured or stale DB.
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

  db.close();
  db = QSqlDatabase();
  QSqlDatabase::removeDatabase(connName);
  return result;
}

/// H-E: run the 24h chart COUNT query on a dedicated throw-away connection
/// (thread pool) so the GROUP BY scan never blocks the UI thread.
QVector<ReadingBucketPoint> executeChartQuery(const QString &dbPath,
                                              const QTimeZone &tz,
                                              int bucketMinutes)
{
  const QString connName = QStringLiteral("chart_query_%1").arg(
      reinterpret_cast<quintptr>(QThread::currentThreadId()));
  QSqlDatabase db = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), connName);
  db.setDatabaseName(dbPath);
  if (!db.open()) {
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    return {};
  }

  Data::Database::applyPerformancePragmas(db, nullptr);

  QVector<ReadingBucketPoint> points;
  {
    ChartQueryService svc(db);
    points = svc.readingCountsLast24h(bucketMinutes, tz);
  }

  db.close();
  db = QSqlDatabase();
  QSqlDatabase::removeDatabase(connName);
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
  // Do not leave an in-flight retention purge behind at shutdown.
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
  m_settings = settings;
  if (m_settings) {
    // Purge when settings are saved (retention days may have changed).
    connect(m_settings, &SettingsController::saved, this,
            &DashboardController::purgeOldData);
  }
}

DashboardController *DashboardController::instance() { return g_instance; }

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
  }

  syncModbusRegistry();
  if (m_appState) {
    m_appState->refreshFromDatabase();
  }
}

void DashboardController::reloadRecentEvents() { m_recentEvents.reload(); }

void DashboardController::refreshReadingsChart() {
  if (!m_db || !m_db->isOpen())
    return;

  // L-20: use the configured system_timezone so chart labels show local
  // time instead of UTC. Fall back to the Qt system timezone when unset.
  QTimeZone tz = QTimeZone::systemTimeZone();
  if (m_settings && !m_settings->systemTimezone().isEmpty()) {
    const QTimeZone configured(m_settings->systemTimezone().toUtf8());
    if (configured.isValid())
      tz = configured;
  }

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
        buildReadingsChartPresentation(data, kChartDisplayPointCount,
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
    emit retentionPurgeCompleted(totalDeleted);
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
            emit retentionPurgeCompleted(totalDeleted);
            if (totalDeleted > 0) {
              refreshReadingsChart();
            }
          });
  QFuture<PurgeResult> future =
      QtConcurrent::run(executeRetentionPurge, dbPath, cutoff);
  watcher->setFuture(future);
}

void DashboardController::startModbusPolling() { syncModbusRegistry(); }

void DashboardController::stopModbusPolling() {
  if (!m_modbus)
    return;
  QMetaObject::invokeMethod(m_modbus, "shutdown", Qt::QueuedConnection);
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
  const bool online = snapshot.success;
  const QString newStatus =
      online ? QLatin1String(CentralLogger::Sensor::kLoggerOnline)
             : QLatin1String(CentralLogger::Sensor::kLoggerOffline);

  // Snapshot the previous status before patching the list model — once
  // updateLoggerRow runs the row reflects the new state.
  const auto prevIt = m_lastStatus.constFind(loggerId);
  const QString prevStatus =
      prevIt != m_lastStatus.constEnd() ? *prevIt : QString();

  m_loggers.updateLoggerRow(
      loggerId, newStatus, sensorCount, snapshot.header.isPolling(),
      snapshot.header.isAnyAlarm(), snapshot.header.isRtuConnected());

  if (online) {
    // Audit H-A: catalog rows arrive pre-fetched from the bridge thread;
    // the UI thread no longer touches the DB on this path.
    m_sensorCache.apply(snapshot, catalogRows);
    if (m_sensorTable.loggerId() == loggerId) {
      m_sensorTable.setRows(m_sensorCache.rowsFor(loggerId));
    }

    // L-21: build an edgeSensorId→name map for analog sensors so the
    // trending chart uses catalog names instead of "Sensor #N".
    QHash<int, QString> nameMap;
    QHash<int, int> decimalsMap;
    for (const auto &row : catalogRows) {
      if (row.sensorType == CentralLogger::Sensor::kTypeAnalog) {
        nameMap.insert(row.edgeSensorId,
                       row.name.isEmpty()
                           ? QString(QLatin1String(CentralLogger::Sensor::kFallbackNameFmt)).arg(row.edgeSensorId)
                           : row.name);
        decimalsMap.insert(row.edgeSensorId, row.decimals);
      }
    }
    m_pollHistory.updateSensorNames(loggerId, nameMap);
    m_pollHistory.updateSensorDecimals(loggerId, decimalsMap);
  }

  maybeLogStatusTransition(loggerId, prevStatus, newStatus);
  m_lastStatus.insert(loggerId, newStatus);

  // Task 14: accumulate analog trending data for LoggerDetailView chart.
  m_pollHistory.append(snapshot);

  emit loggerSnapshotUpdated(loggerId, snapshot.success, snapshot.errorMessage);

  if (m_appState) {
    // Push live alarm state so AppState.alarmCount reflects current device
    // state, not historical system_event rows. updateAlarmState already
    // triggers a refreshFromDatabase() when the alarm bit flips; here we
    // also refresh when the status itself transitioned so totalLoggers /
    // onlineLoggers stay accurate without forcing two COUNT queries per
    // poll on the UI thread when nothing changed.
    m_appState->updateAlarmState(loggerId,
                                 online && snapshot.header.isAnyAlarm());
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
  invalidateBridgeCatalogCache(id);
  if (m_appState) {
    m_appState->removeLogger(id);
  }
  if (m_sensorTable.loggerId() == id) {
    m_sensorTable.setLoggerId(-1);
  }
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
  // Audit H-A: catalog mutations (via LoggerFormController::upsertProbedCatalog
  // or sensor upserts) can change names/thresholds the bridge cached — drop
  // every cached entry so the next poll rebuilds from the current catalog.
  invalidateBridgeCatalogCache(0);
  // L-14: refresh the readings chart after any CRUD operation that can
  // change the set of active loggers (same pattern as purgeOldData).
  refreshReadingsChart();
  if (m_appState) {
    m_appState->refreshFromDatabase();
  }
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
