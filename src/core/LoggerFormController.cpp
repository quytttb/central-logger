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
#include "utils/network/HostValidator.h"

#include <QDebug>
#include <QEventLoop>
#include <QJSEngine>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QSqlError>
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
  qInfo().noquote() << "[probe] begin host=" << host.trimmed()
                    << "apiPort=" << apiPort;
  if (!m_restConfig) {
    qWarning().noquote() << "[probe] aborted: REST service not available";
    emit probeConfigResult(false,
                           QStringLiteral("REST service not available."));
    return;
  }
  if (!HostValidator::isValidHost(host)) {
    qWarning().noquote() << "[probe] aborted: invalid host" << host.trimmed();
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

void LoggerFormController::loadConfigForForm(int loggerId) {
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
    bool isAdd, int loggerId, const QString &stationCode, const QString &name, const QString &host,
    int modbusPort, int apiPort, const QString &apiToken, int modbusUnitId,
    int pollIntervalS, int timeoutS) {
  qInfo().noquote() << "[save] begin isAdd=" << isAdd
                    << "loggerId=" << loggerId
                    << "name=" << name.trimmed()
                    << "host=" << host.trimmed()
                    << "modbusPort=" << modbusPort << "apiPort=" << apiPort
                    << "probedRevision=" << m_probedRevision;

  if (m_formSaveInProgress) {
    qWarning().noquote() << "[save] aborted: save already in progress";
    emit formSaveFinished(false, loggerId,
                          QStringLiteral("Save already in progress."));
    return;
  }
  if (!ensureDatabase()) {
    qWarning().noquote() << "[save] aborted: database not open";
    emit formSaveFinished(false, loggerId, m_lastError);
    return;
  }
  if (m_probedRevision < 0) {
    qWarning().noquote()
        << "[save] aborted: no probed config (m_probedRevision < 0)";
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
    code = stationCode.trimmed();
    if (code.isEmpty()) {
      code = probedStationCode().trimmed();
    }
    if (code.isEmpty()) {
      qWarning().noquote()
          << "[save] aborted: device config has no station_code";
      setError(QStringLiteral(
          "Thiết bị chưa có mã trạm. Vui lòng nhập Mã trạm (Station Code) thủ công."));
      emit formSaveFinished(false, loggerId, m_lastError);
      return;
    }
  } else {
    code = existingRow->stationCode;
  }
  qInfo().noquote() << "[save] stationCode=" << code;

  if (!validateCommon(code, nm, hst, modbusPort, apiPort)) {
    qWarning().noquote() << "[save] aborted: validation failed —" << m_lastError;
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
  qInfo().noquote() << "[save] needsPost=" << needsPost
                    << "patchKeys=" << patchMap.keys();

  m_formSaveInProgress = true;
  setError({});

  QSqlDatabase conn = m_db->connection();
  if (!conn.transaction()) {
    qWarning().noquote() << "[save] aborted: conn.transaction() failed —"
                         << conn.lastError().text();
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
      qWarning().noquote() << "[save] aborted: insert failed —" << err;
      conn.rollback();
      m_formSaveInProgress = false;
      setError(Data::humanizeSqlError(err, code));
      emit formSaveFinished(false, -1, m_lastError);
      return;
    }
    savedId = info.id;
    qInfo().noquote() << "[save] inserted row id=" << savedId;
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
      qWarning().noquote() << "[save] aborted: update failed —" << err;
      conn.rollback();
      m_formSaveInProgress = false;
      setError(Data::humanizeSqlError(err, code));
      emit formSaveFinished(false, loggerId, m_lastError);
      return;
    }
  }

  // Attempt to push the changed fields to the edge device. If the REST call
  // fails (device offline, timeout, etc.) we still commit the local DB record
  // so the logger is not lost. The caller receives configApplyFailed and can
  // show a non-blocking warning; Modbus polling will resume when the device
  // comes back online.
  int appliedRevision = m_probedRevision;
  bool restApplyFailed = false;
  QString restApplyErr;
  if (needsPost) {
    if (!waitForConfigApply(savedId, m_probedRevision, patchJson,
                            &appliedRevision, &restApplyErr)) {
      restApplyFailed = true;
      qWarning() << "LoggerFormController: REST config push failed for logger"
                 << savedId << "—" << restApplyErr
                 << "(logger will still be saved to local DB)";
      if (m_dashboard) {
        m_dashboard->logEvent(
            savedId, QStringLiteral("Warning"),
            QStringLiteral("Config push to device failed: %1").arg(restApplyErr));
      }
    }
  }

  if (appliedRevision < 0) {
    appliedRevision = m_probedRevision;
  }

  if (const auto row = repo.findById(savedId, &err)) {
    Data::LoggerInfo info = *row;
    info.lastRevision = appliedRevision;
    if (!repo.update(info, &err)) {
      qWarning().noquote()
          << "[save] aborted: lastRevision update failed —" << err;
      conn.rollback();
      m_formSaveInProgress = false;
      setError(err);
      emit formSaveFinished(false, savedId, m_lastError);
      return;
    }
  }

  QString catalogErr;
  if (!upsertProbedCatalog(savedId, &catalogErr)) {
    qWarning().noquote() << "[save] aborted: catalog upsert failed —"
                         << catalogErr;
    conn.rollback();
    m_formSaveInProgress = false;
    setError(catalogErr);
    emit formSaveFinished(false, savedId, m_lastError);
    return;
  }

  if (!conn.commit()) {
    qWarning().noquote() << "[save] aborted: commit failed —"
                         << conn.lastError().text();
    conn.rollback();
    m_formSaveInProgress = false;
    setError(QStringLiteral("Could not commit database transaction."));
    emit formSaveFinished(false, savedId, m_lastError);
    return;
  }
  qInfo().noquote() << "[save] committed OK savedId=" << savedId
                    << "restApplyFailed=" << restApplyFailed;

  m_formSaveInProgress = false;
  setError({});
  if (m_dashboard) {
    m_dashboard->logEvent(savedId, QStringLiteral("Info"),
                          isAdd ? QStringLiteral("Logger added: %1").arg(code)
                                : QStringLiteral("Logger updated: %1").arg(code));
    m_dashboard->afterMutation();
  }
  clearProbedConfig();
  if (m_db && m_db->isOpen()) {
    Data::LoggerRepository verifyRepo(m_db->connection());
    qInfo().noquote() << "[save] post-commit logger_info rowCount="
                      << verifyRepo.findAll().size();
  }
  if (isAdd) {
    emit loggerAdded(savedId);
  } else {
    emit loggerUpdated(savedId);
  }
  emit formSaveFinished(true, savedId, QString{});

  // Emit the REST warning AFTER formSaveFinished so QML can close the dialog
  // first and then show the non-blocking banner.
  if (restApplyFailed) {
    emit configApplyFailed(savedId, restApplyErr);
  }
}

void LoggerFormController::onProbeConfigFetched(bool ok, int httpStatus,
                                                QString rawJson,
                                                QString errorMessage) {
  if (!ok) {
    qWarning().noquote() << "[probe] failed httpStatus=" << httpStatus << "—"
                         << errorMessage;
    clearProbedConfig();
    emit probeConfigResult(false, errorMessage);
    return;
  }

  const auto parsed =
      Network::RestConfigParser::parseConfigResponse(0, rawJson.toUtf8());
  if (!parsed.valid) {
    qWarning().noquote() << "[probe] invalid config response httpStatus="
                         << httpStatus;
    clearProbedConfig();
    emit probeConfigResult(false, QStringLiteral("Invalid config response"));
    return;
  }

  storeProbedFromParsed(parsed, 0);
  qInfo().noquote() << "[probe] OK revision=" << parsed.revision
                    << "stationCode=" << probedStationCode()
                    << "stationName=" << probedStationName()
                    << "sensors=" << parsed.sensors.size();
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
