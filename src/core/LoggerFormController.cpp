#include "LoggerFormController.h"

#include "DashboardController.h"
#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SqlHelper.h"
#include "network/rest/RestConfigParser.h"
#include "network/rest/RestConfigService.h"
#include "utils/AppConstants.h"
#include "utils/HostValidator.h"

#include <QEventLoop>
#include <QJSEngine>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QTimer>

namespace CentralLogger::Core {

using CentralLogger::Utils::HostValidator;

namespace {

LoggerFormController *g_instance = nullptr;

} // namespace

LoggerFormController::LoggerFormController(QObject *parent) : QObject(parent) {}

void LoggerFormController::setRestConfigService(Network::RestConfigService *rest) {
  if (m_restConfig == rest)
    return;
  if (m_restConfig) {
    disconnect(m_restConfig, nullptr, this, nullptr);
  }
  m_restConfig = rest;
  if (m_restConfig) {
    connect(m_restConfig, &Network::RestConfigService::probeConfigFetched, this,
            &LoggerFormController::onProbeConfigFetched);
    connect(m_restConfig, &Network::RestConfigService::configFetched, this,
            &LoggerFormController::onConfigFetchedForForm);
  }
}

LoggerFormController *LoggerFormController::instance() { return g_instance; }

void LoggerFormController::setInstance(LoggerFormController *controller) {
  g_instance = controller;
}

LoggerFormController *LoggerFormController::create(QQmlEngine *, QJSEngine *) {
  Q_ASSERT(g_instance);
  QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
  return g_instance;
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

qint64 LoggerFormController::addLogger(const QString &stationCode,
                                       const QString &name, const QString &host,
                                       int modbusPort, int apiPort,
                                       const QString &apiToken,
                                       const QString &note, int modbusUnitId,
                                       int pollIntervalS, int timeoutS) {
  if (!ensureDatabase())
    return -1;

  const QString code = stationCode.trimmed();
  const QString nm = name.trimmed();
  const QString hst = host.trimmed();
  if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
    return -1;
  }

  Data::LoggerInfo info;
  info.stationCode = code;
  info.name = nm;
  info.host = hst;
  info.modbusPort = modbusPort;
  info.modbusUnitId = modbusUnitId > 0 ? modbusUnitId : Defaults::kDefaultModbusUnitId;
  info.centralPollIntervalS =
      pollIntervalS > 0 ? pollIntervalS : Defaults::kDefaultPollIntervalSec;
  info.timeoutS = timeoutS > 0 ? static_cast<double>(timeoutS)
                               : static_cast<double>(Defaults::kDefaultTimeoutSec);
  info.enabled = true;
  info.apiPort = apiPort;
  info.apiToken = apiToken;
  info.note = note;

  Data::LoggerRepository repo(m_db->connection());
  QString err;
  if (!repo.insert(info, &err)) {
    setError(Data::humanizeSqlError(err, code));
    return -1;
  }

  const qint64 loggerId = info.id;

  setError(QString{});
  if (m_dashboard) {
    m_dashboard->logEvent(loggerId, QStringLiteral("Info"),
                          QStringLiteral("Logger added: %1").arg(code));
    m_dashboard->afterMutation();
  }
  emit loggerAdded(loggerId);

  return loggerId;
}

bool LoggerFormController::updateLogger(qint64 id, const QString &stationCode,
                                        const QString &name, const QString &host,
                                        int modbusPort, int apiPort,
                                        const QString &apiToken,
                                        const QString &note, int modbusUnitId,
                                        int pollIntervalS, int timeoutS) {
  if (!ensureDatabase())
    return false;

  const QString code = stationCode.trimmed();
  const QString nm = name.trimmed();
  const QString hst = host.trimmed();
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
  info.stationCode = code;
  info.name = nm;
  info.host = hst;
  info.modbusPort = modbusPort;
  info.modbusUnitId = modbusUnitId > 0 ? modbusUnitId : existing->modbusUnitId;
  info.centralPollIntervalS =
      pollIntervalS > 0 ? pollIntervalS : existing->centralPollIntervalS;
  info.timeoutS =
      timeoutS > 0 ? static_cast<double>(timeoutS) : existing->timeoutS;
  info.enabled = true;
  info.apiPort = apiPort;
  info.apiToken = apiToken;
  info.note = note;

  if (!repo.update(info, &err)) {
    setError(Data::humanizeSqlError(err, code));
    return false;
  }

  setError(QString{});
  if (m_dashboard) {
    m_dashboard->logEvent(id, QStringLiteral("Info"),
                          QStringLiteral("Logger updated: %1").arg(code));
    m_dashboard->afterMutation();
  }
  emit loggerUpdated(id);

  return true;
}

bool LoggerFormController::removeLogger(qint64 id) {
  if (!ensureDatabase())
    return false;

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
  if (m_dashboard) {
    m_dashboard->cleanupRemovedLogger(id);
  }
  setError(QString{});
  if (m_dashboard) {
    // App-wide event (logger_id = NULL) so the FK CASCADE doesn't wipe it
    // along with the row we just removed.
    m_dashboard->logEvent(0, QStringLiteral("Info"),
                          QStringLiteral("Logger removed: %1")
                              .arg(existing->stationCode));
    m_dashboard->afterMutation();
  }
  emit loggerRemoved(id);
  return true;
}

QVariantMap LoggerFormController::getLoggerFormData(qint64 id) const {
  QVariantMap result;
  if (!m_db || !m_db->isOpen()) {
    return result;
  }
  Data::LoggerRepository repo(m_db->connection());
  const auto found = repo.findById(id);
  if (!found) {
    return result;
  }
  result.insert(QStringLiteral("id"), QVariant::fromValue(found->id));
  result.insert(QStringLiteral("stationCode"), found->stationCode);
  result.insert(QStringLiteral("name"), found->name);
  result.insert(QStringLiteral("host"), found->host);
  result.insert(QStringLiteral("modbusPort"), found->modbusPort);
  result.insert(QStringLiteral("modbusUnitId"), found->modbusUnitId);
  result.insert(QStringLiteral("centralPollIntervalS"),
                found->centralPollIntervalS);
  result.insert(QStringLiteral("timeoutS"), static_cast<int>(found->timeoutS));
  result.insert(QStringLiteral("apiPort"), found->apiPort);
  result.insert(QStringLiteral("apiToken"), found->apiToken);
  result.insert(QStringLiteral("note"), found->note);
  result.insert(QStringLiteral("status"), found->status);
  return result;
}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------

bool LoggerFormController::ensureDatabase() {
  if (!m_db || !m_db->isOpen()) {
    setError(QStringLiteral("Database not open"));
    return false;
  }
  return true;
}

bool LoggerFormController::validateCommon(const QString &stationCode,
                                          const QString &name,
                                          const QString &host, int modbusPort,
                                          int apiPort) {
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

void LoggerFormController::setError(const QString &message) {
  if (m_lastError == message)
    return;
  m_lastError = message;
  emit lastErrorChanged();
}

void LoggerFormController::clearLastError() { setError(QString{}); }

bool LoggerFormController::isValidHost(const QString &host) const {
  return HostValidator::isValidHost(host);
}

// ---------------------------------------------------------------------------
// probeConfig — delegate to RestConfigService
// ---------------------------------------------------------------------------

void LoggerFormController::probeConfig(const QString &host, int apiPort,
                                       const QString &token) {
  if (!m_restConfig) {
    emit probeConfigResult(false,
                           QStringLiteral("REST service not available."));
    return;
  }
  if (!HostValidator::isValidHost(host)) {
    emit probeConfigResult(
        false, QStringLiteral("Host phải là IPv4 hoặc hostname hợp lệ"));
    return;
  }
  m_restConfig->probeConfig(host, apiPort, token);
}

void LoggerFormController::clearProbedConfig() {
  m_probedRevision = -1;
  m_probedConfigObject = QJsonObject();
  m_probedSensors.clear();
  m_probedModbusUnitId = -1;
  m_formLoadLoggerId = -1;
}

void LoggerFormController::storeProbedFromParsed(
    const Network::RestConfigParser::ConfigPayload &parsed,
    qint64 loggerIdForSensors) {
  m_probedRevision = parsed.revision;
  m_probedConfigObject = parsed.configObject;
  m_probedSensors = parsed.sensors;
  m_probedModbusUnitId = parsed.modbusTcpUnitId;
  for (auto &sensor : m_probedSensors) {
    sensor.loggerId = loggerIdForSensors;
  }
}

int LoggerFormController::probedPollInterval() const {
  const auto v = m_probedConfigObject.value(QLatin1String("poll_interval"));
  if (v.isDouble()) {
    return v.toInt();
  }
  return -1;
}

QString LoggerFormController::probedStationName() const {
  const auto v = m_probedConfigObject.value(QLatin1String("station_name"));
  if (v.isString()) {
    return v.toString();
  }
  return {};
}

QString LoggerFormController::probedStationCode() const {
  const auto v = m_probedConfigObject.value(QLatin1String("station_code"));
  if (v.isString()) {
    return v.toString();
  }
  return {};
}

void LoggerFormController::loadConfigForForm(qint64 loggerId) {
  if (!m_restConfig) {
    emit configLoadForFormFinished(
        false, QStringLiteral("REST service not available."));
    return;
  }
  if (loggerId < 0) {
    emit configLoadForFormFinished(false, QStringLiteral("Invalid logger."));
    return;
  }
  m_formLoadLoggerId = loggerId;
  m_restConfig->fetchConfig(loggerId);
}

void LoggerFormController::onConfigFetchedForForm(qint64 loggerId, bool ok,
                                                  int httpStatus,
                                                  QString rawJson,
                                                  QString errorMessage) {
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

bool LoggerFormController::upsertProbedCatalog(qint64 loggerId,
                                               QString *errorOut) {
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

bool LoggerFormController::waitForConfigApply(qint64 loggerId,
                                              int expectedRevision,
                                              const QJsonObject &patch,
                                              int *appliedRevisionOut,
                                              QString *errorOut) {
  if (!m_restConfig) {
    if (errorOut) {
      *errorOut = QStringLiteral("REST service not available.");
    }
    return false;
  }

  bool finished = false;
  bool applyOk = false;
  QString applyErr;
  int appliedRev = -1;

  auto handler = [&](qint64 lid, bool ok, int httpStatus, QString rawJson,
                     QString errMsg) {
    if (lid != loggerId) {
      return;
    }
    finished = true;
    applyOk = ok;
    applyErr = errMsg.isEmpty() && !ok
                   ? QStringLiteral("HTTP %1").arg(httpStatus)
                   : errMsg;
    if (ok) {
      const auto result =
          Network::RestConfigParser::parseApplyResponse(rawJson.toUtf8());
      appliedRev = result.appliedRevision;
    }
  };

  const QMetaObject::Connection conn = connect(
      m_restConfig, &Network::RestConfigService::configApplied, this, handler);

  m_restConfig->applyConfig(loggerId, expectedRevision, patch);

  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);
  connect(&timer, &QTimer::timeout, &loop, [&]() {
    if (!finished) {
      finished = true;
      applyOk = false;
      applyErr = QStringLiteral("Config apply timed out.");
    }
    loop.quit();
  });
  connect(m_restConfig, &Network::RestConfigService::configApplied, &loop,
          &QEventLoop::quit, Qt::QueuedConnection);

  timer.start(15000);
  loop.exec();
  disconnect(conn);
  timer.stop();

  if (!applyOk) {
    if (errorOut) {
      *errorOut =
          applyErr.isEmpty() ? QStringLiteral("Config push failed.") : applyErr;
    }
    return false;
  }
  if (appliedRevisionOut) {
    *appliedRevisionOut = appliedRev;
  }
  return true;
}

void LoggerFormController::saveLoggerFromForm(
    bool isAdd, qint64 loggerId, const QString &name, const QString &host,
    int modbusPort, int apiPort, const QString &apiToken, int modbusUnitId,
    int pollIntervalS, int timeoutS) {
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

  const QString nm = name.trimmed();
  const QString hst = host.trimmed();

  Data::LoggerRepository repoEarly(m_db->connection());
  QString lookupErr;
  std::optional<Data::LoggerInfo> existingRow;
  if (!isAdd) {
    existingRow = repoEarly.findById(loggerId, &lookupErr);
    if (!existingRow) {
      setError(lookupErr.isEmpty() ? QStringLiteral("Logger không tồn tại")
                                   : lookupErr);
      emit formSaveFinished(false, loggerId, m_lastError);
      return;
    }
  }

  QString code;
  if (isAdd) {
    code = probedStationCode().trimmed();
    if (code.isEmpty()) {
      setError(QStringLiteral(
          "Device config has no station code. Connect again, then save."));
      emit formSaveFinished(false, loggerId, m_lastError);
      return;
    }
  } else {
    code = existingRow->stationCode;
  }

  if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
    emit formSaveFinished(false, loggerId, m_lastError);
    return;
  }

  const QVariantMap originalMap = m_probedConfigObject.toVariantMap();
  QVariantMap editedMap = originalMap;
  editedMap.insert(QStringLiteral("station_name"), nm);
  const int pollS = pollIntervalS > 0 ? pollIntervalS : Defaults::kDefaultPollIntervalSec;
  editedMap.insert(QStringLiteral("poll_interval"), pollS);
  // station_code: Central DB only (from probe on add); never in edge POST
  // patch.
  const QVariantMap patchMap = buildEditPatch(originalMap, editedMap);
  const QJsonObject patchJson = QJsonObject::fromVariantMap(patchMap);
  const bool needsPost = !patchJson.isEmpty();

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
  QString err;
  qint64 savedId = loggerId;

  if (isAdd) {
    Data::LoggerInfo info;
    info.stationCode = code;
    info.name = nm;
    info.host = hst;
    info.modbusPort = modbusPort;
    info.modbusUnitId = modbusUnitId > 0 ? modbusUnitId : Defaults::kDefaultModbusUnitId;
    info.centralPollIntervalS = pollS;
    info.timeoutS = timeoutS > 0 ? static_cast<double>(timeoutS)
                                 : static_cast<double>(Defaults::kDefaultTimeoutSec);
    info.enabled = true;
    info.apiPort = apiPort;
    info.apiToken = apiToken;
    info.lastRevision = m_probedRevision;

    if (!repo.insert(info, &err)) {
      conn.rollback();
      m_formSaveInProgress = false;
      setError(Data::humanizeSqlError(err, code));
      emit formSaveFinished(false, -1, m_lastError);
      return;
    }
    savedId = info.id;
  } else {
    Data::LoggerInfo info = *existingRow;
    info.stationCode = code;
    info.name = nm;
    info.host = hst;
    info.modbusPort = modbusPort;
    info.modbusUnitId =
        modbusUnitId > 0 ? modbusUnitId : existingRow->modbusUnitId;
    info.centralPollIntervalS = pollS;
    info.timeoutS =
        timeoutS > 0 ? static_cast<double>(timeoutS) : existingRow->timeoutS;
    info.enabled = true;
    info.apiPort = apiPort;
    info.apiToken = apiToken;
    info.lastRevision = m_probedRevision;

    if (!repo.update(info, &err)) {
      conn.rollback();
      m_formSaveInProgress = false;
      setError(Data::humanizeSqlError(err, code));
      emit formSaveFinished(false, loggerId, m_lastError);
      return;
    }
  }

  int appliedRevision = m_probedRevision;
  if (needsPost) {
    QString applyErr;
    if (!waitForConfigApply(savedId, m_probedRevision, patchJson,
                            &appliedRevision, &applyErr)) {
      conn.rollback();
      m_formSaveInProgress = false;
      setError(applyErr);
      if (m_dashboard) {
        m_dashboard->logEvent(
            0, QStringLiteral("Warning"),
            QStringLiteral("Failed to push config to logger #%1: %2")
                .arg(savedId)
                .arg(applyErr));
        m_dashboard->reloadRecentEvents();
      }
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
  if (m_dashboard) {
    m_dashboard->logEvent(savedId, QStringLiteral("Info"),
                          isAdd ? QStringLiteral("Logger added: %1").arg(code)
                                : QStringLiteral("Logger updated: %1").arg(code));
  }
  clearProbedConfig();
  if (m_dashboard) {
    m_dashboard->afterMutation();
  }
  if (isAdd) {
    emit loggerAdded(savedId);
  } else {
    emit loggerUpdated(savedId);
  }
  emit formSaveFinished(true, savedId, QString{});
}

void LoggerFormController::onProbeConfigFetched(bool ok, int httpStatus,
                                                QString rawJson,
                                                QString errorMessage) {
  Q_UNUSED(httpStatus);
  if (!ok) {
    clearProbedConfig();
    emit probeConfigResult(false, errorMessage);
    return;
  }

  const auto parsed =
      Network::RestConfigParser::parseConfigResponse(0, rawJson.toUtf8());
  if (!parsed.valid) {
    clearProbedConfig();
    emit probeConfigResult(false, QStringLiteral("Invalid config response"));
    return;
  }

  storeProbedFromParsed(parsed, 0);
  emit probeConfigResult(true, QString{});
}

// ---------------------------------------------------------------------------
// buildEditPatch — diff original vs edited QVariantMap
// ---------------------------------------------------------------------------

QVariantMap LoggerFormController::buildEditPatch(const QVariantMap &original,
                                                 const QVariantMap &edited) {
  QVariantMap patch;
  // Compare each key in `edited`; include it in the patch only when it
  // actually differs from `original`. This avoids sending unchanged fields
  // to POST /config (optimistic lock payload should be minimal).
  for (auto it = edited.constBegin(); it != edited.constEnd(); ++it) {
    // station_code is Central DB / probe only — never POST to edge.
    if (it.key() == QLatin1String("station_code")) {
      continue;
    }
    const auto origVal = original.value(it.key());
    if (origVal != it.value()) {
      patch.insert(it.key(), it.value());
    }
  }
  return patch;
}

} // namespace CentralLogger::Core
