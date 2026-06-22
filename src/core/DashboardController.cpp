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
#include "utils/charts/ChartDisplayLimits.h"

#include <QDateTime>
#include <QJSEngine>
#include <QQmlEngine>
#include <QTimeZone>
#include <QTimer>
#include <QVector>

namespace CentralLogger::Core {

using CentralLogger::Utils::displayLevelForEvent;
using CentralLogger::Utils::kChartDisplayPointCount;

namespace {

DashboardController *g_instance = nullptr;

// Retention purge cadence — hourly (Task 16 / FE-016).
constexpr int kPurgeIntervalMs = 3600 * 1000;

} // namespace

DashboardController::DashboardController(QObject *parent) : QObject(parent) {
  // Hourly retention purge timer — Task 16 (FE-016).
  m_purgeTimer.setInterval(kPurgeIntervalMs);
  connect(&m_purgeTimer, &QTimer::timeout, this,
          &DashboardController::purgeOldData);
  m_purgeTimer.start();
}

void DashboardController::setDatabase(Data::Database *db) {
  m_db = db;
  m_loggers.setDatabase(db);
  m_recentEvents.setDatabase(db);
}

void DashboardController::setModbusBridge(Network::ModbusBridge *bridge) {
  m_bridge = bridge;
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

  ChartQueryService svc(m_db->connection());
  const auto points = svc.readingCountsLast24h(5, tz);

  QVariantList data;
  data.reserve(points.size());
  for (const auto &pt : points) {
    QVariantMap m;
    m.insert(QStringLiteral("label"), pt.label);
    m.insert(QStringLiteral("bucketMs"), pt.bucketMs);
    m.insert(QStringLiteral("count"), pt.count);
    data.append(m);
  }
  const auto presentation =
      buildReadingsChartPresentation(data, kChartDisplayPointCount, 5, tz);
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
}

void DashboardController::purgeOldData() {
  if (!m_db || !m_db->isOpen())
    return;

  // Determine retention days: prefer the live SettingsController value,
  // fall back to reading from the DB directly.
  int retentionDays = 30;
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
  Data::SensorReadingRepository repo(m_db->connection());
  QString err;
  const int deleted = repo.purgeOlderThan(cutoff, &err);

  if (deleted < 0) {
    qWarning() << "DashboardController::purgeOldData error:" << err;
    return;
  }

  emit retentionPurgeCompleted(deleted);

  if (deleted > 0) {
    refreshReadingsChart();
  }
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
    const Network::PollSnapshot &snapshot, int sensorCount) {
  const qint64 loggerId = snapshot.loggerId;
  const bool online = snapshot.success;
  const QString newStatus =
      online ? QStringLiteral("online") : QStringLiteral("offline");

  // Snapshot the previous status before patching the list model — once
  // updateLoggerRow runs the row reflects the new state.
  const auto prevIt = m_lastStatus.constFind(loggerId);
  const QString prevStatus =
      prevIt != m_lastStatus.constEnd() ? *prevIt : QString();

  m_loggers.updateLoggerRow(
      loggerId, newStatus, sensorCount, snapshot.header.isPolling(),
      snapshot.header.isAnyAlarm(), snapshot.header.isRtuConnected());

  if (online && m_db && m_db->isOpen()) {
    Data::SensorCatalogRepository catalog(m_db->connection());
    const auto rows = catalog.listByLoggerId(loggerId);
    m_sensorCache.apply(snapshot, rows);
    if (m_sensorTable.loggerId() == loggerId) {
      m_sensorTable.setRows(m_sensorCache.rowsFor(loggerId));
    }

    // L-21: build an edgeSensorId→name map for analog sensors so the
    // trending chart uses catalog names instead of "Sensor #N".
    QHash<int, QString> nameMap;
    QHash<int, int> decimalsMap;
    for (const auto &row : rows) {
      if (row.sensorType == QStringLiteral("ANALOG")) {
        nameMap.insert(row.edgeSensorId,
                       row.name.isEmpty()
                           ? QStringLiteral("Sensor #%1").arg(row.edgeSensorId)
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
    // state, not historical system_event rows.
    m_appState->updateAlarmState(loggerId,
                                 online && snapshot.header.isAnyAlarm());
    // ModbusBridge persists status to DB before this slot; refresh totals
    // even when alarm state did not change (updateAlarmState may no-op).
    m_appState->refreshFromDatabase();
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
    if (newStatus != QStringLiteral("online"))
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
  if (newStatus == QStringLiteral("online")) {
    ev.eventType = QStringLiteral("Online");
    ev.level = QStringLiteral("info");
    ev.message = QStringLiteral("Logger %1 is online").arg(label);
  } else {
    ev.eventType = QStringLiteral("Offline");
    ev.level = QStringLiteral("warning");
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
      m_readingsChartAxis.value(QStringLiteral("xMin")).toDouble();
  const double xMax =
      m_readingsChartAxis.value(QStringLiteral("xMax")).toDouble();
  return Core::snapReadingsChart(m_readingsChartPlotPoints, xMin, xMax, plotX,
                                 plotY, plotW, plotH, mouseX, mouseY, 5);
}

} // namespace CentralLogger::Core
