#include "DashboardController.h"

#include "core/events/EventLevels.h"

#include "HostValidator.h"
#include "AppState.h"
#include "SettingsController.h"
#include "core/charts/ChartPresentation.h"
#include "core/charts/ChartQueryService.h"
#include "core/charts/ChartDisplayLimits.h"
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
#include "network/rest/RestConfigService.h"
#include "network/rest/RestConfigParser.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJSEngine>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QTimer>
#include <QTimeZone>
#include <QVector>

namespace CentralLogger::Core {

namespace {

DashboardController *g_instance = nullptr;

} // namespace

DashboardController::DashboardController(QObject *parent)
    : QObject(parent)
{
    // Hourly retention purge timer — Task 16 (FE-016).
    m_purgeTimer.setInterval(3600 * 1000);
    connect(&m_purgeTimer, &QTimer::timeout, this, &DashboardController::purgeOldData);
    m_purgeTimer.start();

    m_readingsChartTimer.setInterval(kReadingsChartRefreshMs);
    connect(&m_readingsChartTimer, &QTimer::timeout, this, &DashboardController::refreshReadingsChart);
    m_readingsChartTimer.start();
}

void DashboardController::setDatabase(Data::Database *db)
{
    m_db = db;
    m_loggers.setDatabase(db);
    m_recentEvents.setDatabase(db);
}

void DashboardController::setModbusBridge(Network::ModbusBridge *bridge)
{
    m_bridge = bridge;
}

void DashboardController::setModbusService(Network::ModbusService *service)
{
    m_modbus = service;
}

void DashboardController::setSettingsController(SettingsController *settings)
{
    if (m_settings == settings) return;
    m_settings = settings;
    if (m_settings) {
        connect(m_settings, &SettingsController::maintenanceModeChanged,
                this, [this]() {
            if (m_modbus) {
                QMetaObject::invokeMethod(m_modbus, "setMaintenanceMode",
                                          Qt::QueuedConnection,
                                          Q_ARG(bool, m_settings->maintenanceMode()));
            }
        });
        // Purge when settings are saved (retention days may have changed).
        connect(m_settings, &SettingsController::saved,
                this, &DashboardController::purgeOldData);
    }
}

DashboardController *DashboardController::instance() { return g_instance; }

void DashboardController::setInstance(DashboardController *controller)
{
    g_instance = controller;
}

DashboardController *DashboardController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(g_instance);
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

void DashboardController::reloadLoggers()
{
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
            const QString status = m_loggers.data(idx, LoggerListModel::StatusRole).toString();
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

void DashboardController::reloadRecentEvents()
{
    m_recentEvents.reload();
}

void DashboardController::refreshReadingsChart()
{
    if (!m_db || !m_db->isOpen()) return;

    // L-20: use the configured system_timezone so chart labels show local
    // time instead of UTC. Fall back to the Qt system timezone when unset.
    QTimeZone tz = QTimeZone::systemTimeZone();
    if (m_settings && !m_settings->systemTimezone().isEmpty()) {
        const QTimeZone configured(m_settings->systemTimezone().toUtf8());
        if (configured.isValid()) tz = configured;
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
    m_readingsChartAxis       = presentation.axis;
    m_readingsChartHasData    = false;
    for (const auto &pt : points) {
        if (pt.count > 0) {
            m_readingsChartHasData = true;
            break;
        }
    }

    emit readingsChartChanged();
}

void DashboardController::purgeOldData()
{
    if (!m_db || !m_db->isOpen()) return;

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
    if (retentionDays <= 0) retentionDays = 1;

    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-retentionDays);
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

void DashboardController::startModbusPolling()
{
    if (m_modbus && m_settings) {
        QMetaObject::invokeMethod(m_modbus, "setMaintenanceMode",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, m_settings->maintenanceMode()));
    }
    syncModbusRegistry();
}

void DashboardController::stopModbusPolling()
{
    if (!m_modbus) return;
    QMetaObject::invokeMethod(m_modbus, "shutdown", Qt::QueuedConnection);
}

void DashboardController::syncModbusRegistry()
{
    if (!m_modbus || !m_db || !m_db->isOpen()) return;

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
        c.loggerId       = info.id;
        c.host           = info.host;
        c.modbusPort     = info.modbusPort;
        c.unitId         = info.modbusUnitId;
        c.pollIntervalMs = (info.centralPollIntervalS > 0 ? info.centralPollIntervalS : 2) * 1000;
        c.timeoutMs      = static_cast<int>(info.timeoutS > 0 ? info.timeoutS * 1000.0 : 2000.0);
        c.enabled        = info.enabled;
        configs.append(c);
    }
    QMetaObject::invokeMethod(m_modbus, "syncLoggers", Qt::QueuedConnection,
                              Q_ARG(QVector<CentralLogger::Network::LoggerRuntimeConfig>, configs));
}

void DashboardController::onSnapshotApplied(const Network::PollSnapshot &snapshot,
                                            int sensorCount)
{
    const qint64 loggerId = snapshot.loggerId;
    const bool   online   = snapshot.success;
    const QString newStatus = online ? QStringLiteral("online")
                                     : QStringLiteral("offline");

    // Snapshot the previous status before patching the list model — once
    // updateLoggerRow runs the row reflects the new state.
    const auto prevIt = m_lastStatus.constFind(loggerId);
    const QString prevStatus = prevIt != m_lastStatus.constEnd() ? *prevIt : QString();

    m_loggers.updateLoggerRow(loggerId,
                              newStatus,
                              sensorCount,
                              snapshot.header.isPolling(),
                              snapshot.header.isAnyAlarm(),
                              snapshot.header.isRtuConnected());

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
        for (const auto &row : rows) {
            if (row.sensorType == QStringLiteral("ANALOG")) {
                nameMap.insert(row.edgeSensorId,
                               row.name.isEmpty()
                                   ? QStringLiteral("Sensor #%1").arg(row.edgeSensorId)
                                   : row.name);
            }
        }
        m_pollHistory.updateSensorNames(loggerId, nameMap);
    }

    maybeLogStatusTransition(loggerId, prevStatus, newStatus);
    m_lastStatus.insert(loggerId, newStatus);

    // Task 14: accumulate analog trending data for LoggerDetailView chart.
    m_pollHistory.append(snapshot);

    emit loggerSnapshotUpdated(loggerId, snapshot.success, snapshot.errorMessage);

    if (m_appState) {
        // Push live alarm state so AppState.alarmCount reflects current device
        // state, not historical system_event rows.
        m_appState->updateAlarmState(loggerId, online && snapshot.header.isAnyAlarm());
        // ModbusBridge persists status to DB before this slot; refresh totals
        // even when alarm state did not change (updateAlarmState may no-op).
        m_appState->refreshFromDatabase();
    }
}

void DashboardController::maybeLogStatusTransition(qint64 loggerId,
                                                   const QString &prevStatus,
                                                   const QString &newStatus)
{
    if (!m_db || !m_db->isOpen()) return;
    if (prevStatus == newStatus) return;
    if (prevStatus.isEmpty()) {
        // First snapshot after app start. Log Online so the user sees the
        // initial connection succeed. Skip Offline — the DB default is
        // already 'offline' and logging it would generate spurious events
        // for every unreachable logger at startup.
        if (newStatus != QStringLiteral("online")) return;
        // Fall through to log the Online event below.
    }

    const int row = m_loggers.indexOfLogger(loggerId);
    QString stationCode;
    QString name;
    if (row >= 0) {
        const QModelIndex idx = m_loggers.index(row, 0);
        stationCode = m_loggers.data(idx, LoggerListModel::StationCodeRole).toString();
        name        = m_loggers.data(idx, LoggerListModel::NameRole).toString();
    }
    const QString label = stationCode.isEmpty()
        ? (name.isEmpty() ? QStringLiteral("#%1").arg(loggerId) : name)
        : stationCode;

    Data::EventRepository events(m_db->connection());
    Data::SystemEvent ev;
    ev.loggerId = loggerId;
    if (newStatus == QStringLiteral("online")) {
        ev.eventType = QStringLiteral("Online");
        ev.level     = QStringLiteral("info");
        ev.message   = QStringLiteral("Logger %1 is online").arg(label);
    } else {
        ev.eventType = QStringLiteral("Offline");
        ev.level     = QStringLiteral("warning");
        ev.message   = QStringLiteral("Logger %1 went offline").arg(label);
    }
    if (events.insert(ev)) {
        m_recentEvents.reload();
    }
}

qint64 DashboardController::addLogger(const QString &stationCode,
                                      const QString &name,
                                      const QString &host,
                                      int modbusPort,
                                      int apiPort,
                                      const QString &apiToken,
                                      const QString &note,
                                      int modbusUnitId,
                                      int pollIntervalS,
                                      int timeoutS)
{
    if (!ensureDatabase()) return -1;

    const QString code = stationCode.trimmed();
    const QString nm   = name.trimmed();
    const QString hst  = host.trimmed();
    if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
        return -1;
    }

    Data::LoggerInfo info;
    info.stationCode         = code;
    info.name                = nm;
    info.host                = hst;
    info.modbusPort          = modbusPort;
    info.modbusUnitId        = modbusUnitId > 0 ? modbusUnitId : 1;
    info.centralPollIntervalS = pollIntervalS > 0 ? pollIntervalS : 2;
    info.timeoutS            = timeoutS > 0 ? static_cast<double>(timeoutS) : 2.0;
    info.enabled             = true;
    info.apiPort             = apiPort;
    info.apiToken            = apiToken;
    info.note                = note;

    Data::LoggerRepository repo(m_db->connection());
    QString err;
    if (!repo.insert(info, &err)) {
        setError(humanizeSqlError(err, code));
        return -1;
    }

    const qint64 loggerId = info.id;

    setError(QString{});
    logEvent(loggerId, QStringLiteral("Info"),
             QStringLiteral("Logger added: %1").arg(code));
    afterMutation();
    emit loggerAdded(loggerId);

    return loggerId;
}

bool DashboardController::updateLogger(qint64 id,
                                       const QString &stationCode,
                                       const QString &name,
                                       const QString &host,
                                       int modbusPort,
                                       int apiPort,
                                       const QString &apiToken,
                                       const QString &note,
                                       int modbusUnitId,
                                       int pollIntervalS,
                                       int timeoutS)
{
    if (!ensureDatabase()) return false;

    const QString code = stationCode.trimmed();
    const QString nm   = name.trimmed();
    const QString hst  = host.trimmed();
    if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
        return false;
    }

    Data::LoggerRepository repo(m_db->connection());
    QString err;
    const auto existing = repo.findById(id, &err);
    if (!existing) {
        setError(err.isEmpty() ? QStringLiteral("Logger không tồn tại") : err);
        return false;
    }

    Data::LoggerInfo info = *existing;
    info.stationCode          = code;
    info.name                 = nm;
    info.host                 = hst;
    info.modbusPort           = modbusPort;
    info.modbusUnitId         = modbusUnitId > 0 ? modbusUnitId : existing->modbusUnitId;
    info.centralPollIntervalS = pollIntervalS > 0 ? pollIntervalS : existing->centralPollIntervalS;
    info.timeoutS             = timeoutS > 0 ? static_cast<double>(timeoutS) : existing->timeoutS;
    info.enabled              = true;
    info.apiPort              = apiPort;
    info.apiToken             = apiToken;
    info.note                 = note;

    if (!repo.update(info, &err)) {
        setError(humanizeSqlError(err, code));
        return false;
    }

    setError(QString{});
    logEvent(id, QStringLiteral("Info"),
             QStringLiteral("Logger updated: %1").arg(code));
    afterMutation();
    emit loggerUpdated(id);

    return true;
}

bool DashboardController::removeLogger(qint64 id)
{
    if (!ensureDatabase()) return false;

    Data::LoggerRepository repo(m_db->connection());
    QString err;
    const auto existing = repo.findById(id, &err);
    if (!existing) {
        setError(err.isEmpty() ? QStringLiteral("Logger không tồn tại") : err);
        return false;
    }
    if (!repo.remove(id, &err)) {
        setError(err);
        return false;
    }
    m_sensorCache.remove(id);
    m_pollHistory.remove(id);
    m_lastStatus.remove(id);
    if (m_appState) {
        m_appState->removeLogger(id);
    }
    if (m_sensorTable.loggerId() == id) {
        m_sensorTable.setLoggerId(-1);
    }
    setError(QString{});
    // App-wide event (logger_id = NULL) so the FK CASCADE doesn't wipe it
    // along with the row we just removed.
    logEvent(0, QStringLiteral("Info"),
             QStringLiteral("Logger removed: %1").arg(existing->stationCode));
    afterMutation();
    emit loggerRemoved(id);
    return true;
}

QVariantMap DashboardController::getLoggerFormData(qint64 id) const
{
    QVariantMap result;
    if (!m_db || !m_db->isOpen()) {
        return result;
    }
    Data::LoggerRepository repo(m_db->connection());
    const auto found = repo.findById(id);
    if (!found) {
        return result;
    }
    result.insert(QStringLiteral("id"),                  QVariant::fromValue(found->id));
    result.insert(QStringLiteral("stationCode"),         found->stationCode);
    result.insert(QStringLiteral("name"),                found->name);
    result.insert(QStringLiteral("host"),                found->host);
    result.insert(QStringLiteral("modbusPort"),          found->modbusPort);
    result.insert(QStringLiteral("modbusUnitId"),        found->modbusUnitId);
    result.insert(QStringLiteral("centralPollIntervalS"), found->centralPollIntervalS);
    result.insert(QStringLiteral("timeoutS"),            static_cast<int>(found->timeoutS));
    result.insert(QStringLiteral("apiPort"),             found->apiPort);
    result.insert(QStringLiteral("apiToken"),            found->apiToken);
    result.insert(QStringLiteral("note"),                found->note);
    result.insert(QStringLiteral("status"),              found->status);
    return result;
}

bool DashboardController::ensureDatabase()
{
    if (!m_db || !m_db->isOpen()) {
        setError(QStringLiteral("Database not open"));
        return false;
    }
    return true;
}

bool DashboardController::validateCommon(const QString &stationCode,
                                         const QString &name,
                                         const QString &host,
                                         int modbusPort,
                                         int apiPort)
{
    if (stationCode.isEmpty()) {
        setError(QStringLiteral("Station code không được để trống"));
        return false;
    }
    if (name.isEmpty()) {
        setError(QStringLiteral("Name không được để trống"));
        return false;
    }
    if (host.isEmpty()) {
        setError(QStringLiteral("Host không được để trống"));
        return false;
    }
    if (!HostValidator::isValidHost(host)) {
        setError(QStringLiteral("Host phải là IPv4 hoặc hostname hợp lệ"));
        return false;
    }
    if (modbusPort < 1 || modbusPort > 65535) {
        setError(QStringLiteral("Modbus port phải trong khoảng 1–65535"));
        return false;
    }
    if (apiPort < 1 || apiPort > 65535) {
        setError(QStringLiteral("API port phải trong khoảng 1–65535"));
        return false;
    }
    return true;
}

void DashboardController::setError(const QString &message)
{
    if (m_lastError == message) return;
    m_lastError = message;
    emit lastErrorChanged();
}

QString DashboardController::humanizeSqlError(const QString &raw,
                                              const QString &stationCode) const
{
    if (raw.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)
        && raw.contains(QStringLiteral("station_code"))) {
        return QStringLiteral("Station code \"%1\" đã tồn tại").arg(stationCode);
    }
    return raw;
}

void DashboardController::logEvent(qint64 loggerId,
                                   const QString &eventType,
                                   const QString &message)
{
    if (!m_db || !m_db->isOpen()) return;
    Data::EventRepository events(m_db->connection());
    Data::SystemEvent ev;
    if (loggerId > 0) {
        ev.loggerId = loggerId;
    }
    ev.eventType = eventType;
    ev.message   = message;
    ev.level     = displayLevelForEvent(eventType, QString{});
    events.insert(ev);
}

void DashboardController::afterMutation()
{
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

// ---------------------------------------------------------------------------
// Task 22: probeConfig — delegate to RestConfigService
// ---------------------------------------------------------------------------

void DashboardController::setRestConfigService(Network::RestConfigService *rest)
{
    if (m_restConfig == rest) return;
    if (m_restConfig) {
        disconnect(m_restConfig, nullptr, this, nullptr);
    }
    m_restConfig = rest;
    if (m_restConfig) {
        connect(m_restConfig, &Network::RestConfigService::probeConfigFetched,
                this, &DashboardController::onProbeConfigFetched);
        connect(m_restConfig, &Network::RestConfigService::configFetched,
                this, &DashboardController::onConfigFetchedForForm);
    }
}

void DashboardController::probeConfig(const QString &host, int apiPort, const QString &token)
{
    if (!m_restConfig) {
        emit probeConfigResult(false,
            QStringLiteral("REST service not available."));
        return;
    }
    if (!HostValidator::isValidHost(host)) {
        emit probeConfigResult(false,
            QStringLiteral("Host phải là IPv4 hoặc hostname hợp lệ"));
        return;
    }
    m_restConfig->probeConfig(host, apiPort, token);
}

bool DashboardController::isValidHost(const QString &host) const
{
    return HostValidator::isValidHost(host);
}

QVariantMap DashboardController::snapReadingsChart(double mouseX,
                                                   double mouseY,
                                                   double plotX,
                                                   double plotY,
                                                   double plotW,
                                                   double plotH) const
{
    const double xMin = m_readingsChartAxis.value(QStringLiteral("xMin")).toDouble();
    const double xMax = m_readingsChartAxis.value(QStringLiteral("xMax")).toDouble();
    return Core::snapReadingsChart(m_readingsChartPlotPoints,
                                  xMin,
                                  xMax,
                                  plotX,
                                  plotY,
                                  plotW,
                                  plotH,
                                  mouseX,
                                  mouseY,
                                  5);
}

void DashboardController::clearLastError()
{
    setError(QString{});
}

void DashboardController::clearProbedConfig()
{
    m_probedRevision = -1;
    m_probedConfigObject = QJsonObject();
    m_probedSensors.clear();
    m_probedModbusUnitId = -1;
    m_formLoadLoggerId = -1;
}

void DashboardController::storeProbedFromParsed(
    const Network::RestConfigParser::ConfigPayload &parsed,
    qint64 loggerIdForSensors)
{
    m_probedRevision     = parsed.revision;
    m_probedConfigObject = parsed.configObject;
    m_probedSensors      = parsed.sensors;
    m_probedModbusUnitId = parsed.modbusTcpUnitId;
    for (auto &sensor : m_probedSensors) {
        sensor.loggerId = loggerIdForSensors;
    }
}

int DashboardController::probedPollInterval() const
{
    const auto v = m_probedConfigObject.value(QLatin1String("poll_interval"));
    if (v.isDouble()) {
        return v.toInt();
    }
    return -1;
}

QString DashboardController::probedStationName() const
{
    const auto v = m_probedConfigObject.value(QLatin1String("station_name"));
    if (v.isString()) {
        return v.toString();
    }
    return {};
}

QString DashboardController::probedStationCode() const
{
    const auto v = m_probedConfigObject.value(QLatin1String("station_code"));
    if (v.isString()) {
        return v.toString().trimmed();
    }
    return {};
}

void DashboardController::loadConfigForForm(qint64 loggerId)
{
    if (!m_restConfig) {
        emit configLoadForFormFinished(false,
            QStringLiteral("REST service not available."));
        return;
    }
    if (loggerId < 0) {
        emit configLoadForFormFinished(false,
            QStringLiteral("Invalid logger."));
        return;
    }
    m_formLoadLoggerId = loggerId;
    m_restConfig->fetchConfig(loggerId);
}

void DashboardController::onConfigFetchedForForm(qint64 loggerId, bool ok, int httpStatus,
                                                 QString rawJson, QString errorMessage)
{
    if (m_formLoadLoggerId != loggerId) {
        return;
    }
    m_formLoadLoggerId = -1;

    if (!ok) {
        clearProbedConfig();
        const QString msg = errorMessage.isEmpty()
            ? QStringLiteral("HTTP %1").arg(httpStatus)
            : errorMessage;
        emit configLoadForFormFinished(false, msg);
        return;
    }

    const auto parsed = Network::RestConfigParser::parseConfigResponse(
        loggerId, rawJson.toUtf8());
    if (!parsed.valid) {
        clearProbedConfig();
        emit configLoadForFormFinished(false,
            QStringLiteral("Invalid config response"));
        return;
    }

    storeProbedFromParsed(parsed, loggerId);
    emit configLoadForFormFinished(true, QString{});
}

bool DashboardController::upsertProbedCatalog(qint64 loggerId, QString *errorOut)
{
    if (m_probedSensors.isEmpty()) {
        return true;
    }
    Data::SensorCatalogRepository catalogRepo(m_db->connection());
    for (auto sensor : m_probedSensors) {
        sensor.loggerId = loggerId;
        QString catErr;
        if (!catalogRepo.upsert(sensor, &catErr)) {
            if (errorOut) {
                *errorOut = catErr;
            }
            return false;
        }
    }
    return true;
}

bool DashboardController::waitForConfigApply(qint64 loggerId, int expectedRevision,
                                             const QJsonObject &patch,
                                             int *appliedRevisionOut,
                                             QString *errorOut)
{
    if (!m_restConfig) {
        if (errorOut) {
            *errorOut = QStringLiteral("REST service not available.");
        }
        return false;
    }

    bool    finished  = false;
    bool    applyOk   = false;
    QString applyErr;
    int     appliedRev = -1;

    auto handler = [&](qint64 lid, bool ok, int httpStatus, QString rawJson, QString errMsg) {
        if (lid != loggerId) {
            return;
        }
        finished = true;
        applyOk  = ok;
        applyErr = errMsg.isEmpty() && !ok
            ? QStringLiteral("HTTP %1").arg(httpStatus)
            : errMsg;
        if (ok) {
            const auto result = Network::RestConfigParser::parseApplyResponse(rawJson.toUtf8());
            appliedRev = result.appliedRevision;
        }
    };

    const QMetaObject::Connection conn = connect(
        m_restConfig, &Network::RestConfigService::configApplied,
        this, handler);

    m_restConfig->applyConfig(loggerId, expectedRevision, patch);

    QEventLoop loop;
    QTimer     timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (!finished) {
            finished = true;
            applyOk  = false;
            applyErr = QStringLiteral("Config apply timed out.");
        }
        loop.quit();
    });
    connect(m_restConfig, &Network::RestConfigService::configApplied,
            &loop, &QEventLoop::quit, Qt::QueuedConnection);

    timer.start(15000);
    loop.exec();
    disconnect(conn);
    timer.stop();

    if (!applyOk) {
        if (errorOut) {
            *errorOut = applyErr.isEmpty()
                ? QStringLiteral("Config push failed.")
                : applyErr;
        }
        return false;
    }
    if (appliedRevisionOut) {
        *appliedRevisionOut = appliedRev;
    }
    return true;
}

void DashboardController::saveLoggerFromForm(bool isAdd,
                                           qint64 loggerId,
                                           const QString &stationCode,
                                           const QString &name,
                                           const QString &host,
                                           int modbusPort,
                                           int apiPort,
                                           const QString &apiToken,
                                           const QString &note,
                                           int modbusUnitId,
                                           int pollIntervalS,
                                           int timeoutS)
{
    if (m_formSaveInProgress) {
        emit formSaveFinished(false, loggerId,
            QStringLiteral("Save already in progress."));
        return;
    }
    if (!ensureDatabase()) {
        emit formSaveFinished(false, loggerId, m_lastError);
        return;
    }
    if (m_probedRevision < 0) {
        setError(QStringLiteral("Load config from the device first (Connect)."));
        emit formSaveFinished(false, loggerId, m_lastError);
        return;
    }

    const QString code = stationCode.trimmed();
    const QString nm   = name.trimmed();
    const QString hst  = host.trimmed();
    if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
        emit formSaveFinished(false, loggerId, m_lastError);
        return;
    }

    const QVariantMap originalMap = m_probedConfigObject.toVariantMap();
    QVariantMap       editedMap   = originalMap;
    editedMap.insert(QStringLiteral("station_name"), nm);
    const int pollS = pollIntervalS > 0 ? pollIntervalS : 2;
    editedMap.insert(QStringLiteral("poll_interval"), pollS);
    const QVariantMap patchMap =
        buildDeviceConfigPatch(isAdd, originalMap, editedMap);
    const QJsonObject patchJson = QJsonObject::fromVariantMap(patchMap);
    const bool        needsPost = !patchJson.isEmpty();

    m_formSaveInProgress = true;
    setError({});

    QSqlDatabase conn = m_db->connection();
    if (!conn.transaction()) {
        m_formSaveInProgress = false;
        setError(QStringLiteral("Could not start database transaction."));
        emit formSaveFinished(false, loggerId, m_lastError);
        return;
    }

    Data::LoggerRepository repo(conn);
    QString                err;
    qint64                 savedId = loggerId;

    if (isAdd) {
        Data::LoggerInfo info;
        info.stationCode          = code;
        info.name                 = nm;
        info.host                 = hst;
        info.modbusPort           = modbusPort;
        info.modbusUnitId         = modbusUnitId > 0 ? modbusUnitId : 1;
        info.centralPollIntervalS = pollS;
        info.timeoutS             = timeoutS > 0 ? static_cast<double>(timeoutS) : 2.0;
        info.enabled              = true;
        info.apiPort              = apiPort;
        info.apiToken             = apiToken;
        info.note                 = note;
        info.lastRevision         = m_probedRevision;

        if (!repo.insert(info, &err)) {
            conn.rollback();
            m_formSaveInProgress = false;
            setError(humanizeSqlError(err, code));
            emit formSaveFinished(false, -1, m_lastError);
            return;
        }
        savedId = info.id;
    } else {
        const auto existing = repo.findById(loggerId, &err);
        if (!existing) {
            conn.rollback();
            m_formSaveInProgress = false;
            setError(err.isEmpty() ? QStringLiteral("Logger không tồn tại") : err);
            emit formSaveFinished(false, loggerId, m_lastError);
            return;
        }

        Data::LoggerInfo info = *existing;
        info.stationCode          = code;
        info.name                 = nm;
        info.host                 = hst;
        info.modbusPort           = modbusPort;
        info.modbusUnitId         = modbusUnitId > 0 ? modbusUnitId : existing->modbusUnitId;
        info.centralPollIntervalS = pollS;
        info.timeoutS             = timeoutS > 0 ? static_cast<double>(timeoutS) : existing->timeoutS;
        info.enabled              = true;
        info.apiPort              = apiPort;
        info.apiToken             = apiToken;
        info.note                 = note;
        info.lastRevision         = m_probedRevision;

        if (!repo.update(info, &err)) {
            conn.rollback();
            m_formSaveInProgress = false;
            setError(humanizeSqlError(err, code));
            emit formSaveFinished(false, loggerId, m_lastError);
            return;
        }
    }

    int appliedRevision = m_probedRevision;
    if (needsPost) {
        QString applyErr;
        if (!waitForConfigApply(savedId, m_probedRevision, patchJson, &appliedRevision, &applyErr)) {
            conn.rollback();
            m_formSaveInProgress = false;
            setError(applyErr);
            logEvent(0, QStringLiteral("Warning"),
                     QStringLiteral("Failed to push config to logger #%1: %2")
                         .arg(savedId).arg(applyErr));
            m_recentEvents.reload();
            emit formSaveFinished(false, savedId, m_lastError);
            emit configApplyFailed(savedId, applyErr);
            return;
        }
    }

    if (appliedRevision < 0) {
        appliedRevision = m_probedRevision;
    }

    if (const auto row = repo.findById(savedId, &err)) {
        Data::LoggerInfo info = *row;
        info.lastRevision = appliedRevision;
        if (!repo.update(info, &err)) {
            conn.rollback();
            m_formSaveInProgress = false;
            setError(err);
            emit formSaveFinished(false, savedId, m_lastError);
            return;
        }
    }

    QString catalogErr;
    if (!upsertProbedCatalog(savedId, &catalogErr)) {
        conn.rollback();
        m_formSaveInProgress = false;
        setError(catalogErr);
        emit formSaveFinished(false, savedId, m_lastError);
        return;
    }

    if (!conn.commit()) {
        conn.rollback();
        m_formSaveInProgress = false;
        setError(QStringLiteral("Could not commit database transaction."));
        emit formSaveFinished(false, savedId, m_lastError);
        return;
    }

    m_formSaveInProgress = false;
    setError({});
    logEvent(savedId, QStringLiteral("Info"),
             isAdd ? QStringLiteral("Logger added: %1").arg(code)
                   : QStringLiteral("Logger updated: %1").arg(code));
    clearProbedConfig();
    afterMutation();
    if (isAdd) {
        emit loggerAdded(savedId);
    } else {
        emit loggerUpdated(savedId);
    }
    emit formSaveFinished(true, savedId, QString{});
}

void DashboardController::onProbeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage)
{
    Q_UNUSED(httpStatus);
    if (!ok) {
        clearProbedConfig();
        emit probeConfigResult(false, errorMessage);
        return;
    }

    const auto parsed = Network::RestConfigParser::parseConfigResponse(0, rawJson.toUtf8());
    if (!parsed.valid) {
        clearProbedConfig();
        emit probeConfigResult(false, QStringLiteral("Invalid config response"));
        return;
    }

    storeProbedFromParsed(parsed, 0);
    emit probeConfigResult(true, QString{});
}

// ---------------------------------------------------------------------------
// Task 23: buildEditPatch — diff original vs edited QVariantMap
// ---------------------------------------------------------------------------

QVariantMap DashboardController::buildEditPatch(const QVariantMap &original,
                                                 const QVariantMap &edited)
{
    QVariantMap patch;
    // Compare each key in `edited`; include it in the patch only when it
    // actually differs from `original`. This avoids sending unchanged fields
    // to POST /config (optimistic lock payload should be minimal).
    for (auto it = edited.constBegin(); it != edited.constEnd(); ++it) {
        const auto origVal = original.value(it.key());
        if (origVal != it.value()) {
            patch.insert(it.key(), it.value());
        }
    }
    return patch;
}

QVariantMap DashboardController::buildDeviceConfigPatch(bool isAdd,
                                                        const QVariantMap &probedConfig,
                                                        const QVariantMap &editedConfig)
{
    // Add: probe is for catalog + validation only — never push to edge (legacy
    // LoggerFormLogic: config patch only when mode === "edit").
    if (isAdd) {
        return {};
    }
    QVariantMap patch = buildEditPatch(probedConfig, editedConfig);
    // `logger_info.station_code` is Central catalog id; edge keeps its own code.
    patch.remove(QStringLiteral("station_code"));
    return patch;
}

} // namespace CentralLogger::Core

