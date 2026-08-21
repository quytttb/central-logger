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
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"
#include "utils/network/HostValidator.h"

#include <QDebug>
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
    connect(m_restConfig, &Network::RestConfigService::configApplied, this,
            &LoggerFormController::onConfigAppliedPending);
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
    m_dashboard->logEvent(loggerId, CentralLogger::Sensor::kEventTypeInfo,
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
    m_dashboard->logEvent(id, CentralLogger::Sensor::kEventTypeInfo,
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
    m_dashboard->logEvent(0, CentralLogger::Sensor::kEventTypeInfo,
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
    setError(QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen));
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
                           QLatin1String(CentralLogger::Format::kErrRestUnavailable));
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
        false, QLatin1String(CentralLogger::Format::kErrRestUnavailable));
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
                            ? QString(QLatin1String(CentralLogger::Format::kErrHttpFmt)).arg(httpStatus)
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
  qInfo().noquote() << "[save] committed OK savedId=" << savedId;

  // P2 #16 (audit M-1): the DB row is committed BEFORE the REST push so the
  // ~15 s REST round-trip no longer holds the WAL write lock. If the push
  // fails the logger still exists locally; the caller receives
  // configApplyFailed and can show a non-blocking warning.
  //
  // The REST round-trip used to spin a nested QEventLoop (waitForConfigApply),
  // which blocked the UI thread for the full timeout. The whole save flow is
  // now fully async: we keep a small PendingApply record, fire the POST,
  // and complete the save in onConfigAppliedPending() / finishPendingApply().
  setError({});

  // Notify the dashboard / QML that the DB row is in. The REST part, if
  // any, will land later via formSaveFinished (already emitted below for
  // the synchronous no-POST path) and configApplyFailed.
  if (m_dashboard) {
    m_dashboard->logEvent(savedId, CentralLogger::Sensor::kEventTypeInfo,
                          isAdd ? QStringLiteral("Logger added: %1").arg(code)
                                : QStringLiteral("Logger updated: %1").arg(code));
    m_dashboard->afterMutation();
  }
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

  if (needsPost) {
    // Emit formSaveFinished now — the DB row is committed and the dialog
    // can close. The REST push continues in the background; failures land
    // later as configApplyFailed so the UI can show a non-blocking banner.
    // (Replaces the old QEventLoop wait that blocked the UI thread up to
    // 15 s during Save.)
    emit formSaveFinished(true, savedId, QString{});

    // Hand off to the async apply path. m_formSaveInProgress stays true
    // until finishPendingApply() runs; onConfigAppliedPending() will clear
    // it on success or on the 15 s safety timer.
    m_pendingApply.loggerId        = savedId;
    m_pendingApply.isAdd           = isAdd;
    m_pendingApply.stationCode     = code;
    m_pendingApply.probedRevision  = m_probedRevision;
    m_pendingApply.appliedRevision = -1;
    m_pendingApply.responded       = false;
    if (!m_pendingApply.timeout) {
      m_pendingApply.timeout = new QTimer(this);
      m_pendingApply.timeout->setSingleShot(true);
    }
    m_pendingApply.timeout->disconnect(this);
    connect(m_pendingApply.timeout, &QTimer::timeout, this, [this]() {
      if (!m_pendingApply.responded) {
        qWarning() << "LoggerFormController: REST apply timeout for logger"
                   << m_pendingApply.loggerId;
        finishPendingApply(false, QStringLiteral("Config push timed out."));
      }
    });
    m_pendingApply.timeout->start(15000);

    m_restConfig->applyConfig(savedId, m_probedRevision, patchJson);
  } else {
    m_formSaveInProgress = false;
    clearProbedConfig();
    emit formSaveFinished(true, savedId, QString{});
  }
}

void LoggerFormController::onConfigAppliedPending(qint64 loggerId, bool ok,
                                                  int httpStatus,
                                                  QString rawJson,
                                                  QString errorMessage)
{
  if (m_pendingApply.loggerId != loggerId || m_pendingApply.responded) {
    return;
  }
  m_pendingApply.responded = true;
  if (m_pendingApply.timeout) {
    m_pendingApply.timeout->stop();
  }
  if (ok) {
    const auto result =
        Network::RestConfigParser::parseApplyResponse(rawJson.toUtf8());
    m_pendingApply.appliedRevision = result.appliedRevision;
  }
  finishPendingApply(ok,
                     errorMessage.isEmpty() && !ok
                         ? QString(QLatin1String(CentralLogger::Format::kErrHttpFmt)).arg(httpStatus)
                         : errorMessage);
}

void LoggerFormController::finishPendingApply(bool ok,
                                              const QString &errorMessage)
{
  const qint64 loggerId = m_pendingApply.loggerId;
  const bool isAdd = m_pendingApply.isAdd;
  const QString stationCode = m_pendingApply.stationCode;
  const int probedRevision = m_pendingApply.probedRevision;
  int appliedRevision = m_pendingApply.appliedRevision;

  m_pendingApply.loggerId = -1;
  m_pendingApply.isAdd = false;
  m_pendingApply.stationCode.clear();
  m_pendingApply.probedRevision = -1;
  m_pendingApply.appliedRevision = -1;
  m_pendingApply.responded = false;

  if (!ok) {
    qWarning() << "LoggerFormController: REST config push failed for logger"
               << loggerId << "—" << errorMessage
               << "(logger already saved to local DB)";
    if (m_dashboard) {
      m_dashboard->logEvent(
          loggerId, CentralLogger::Sensor::kEventTypeWarning,
          QStringLiteral("Config push to device failed: %1").arg(errorMessage));
    }
  }

  if (appliedRevision < 0) {
    appliedRevision = probedRevision;
  }
  if (ok && appliedRevision != probedRevision) {
    Data::LoggerRepository repo(m_db->connection());
    QString revErr;
    if (const auto row = repo.findById(loggerId, &revErr)) {
      Data::LoggerInfo info = *row;
      info.lastRevision = appliedRevision;
      if (!repo.update(info, &revErr)) {
        qWarning().noquote()
            << "[save] warning: lastRevision update failed —" << revErr;
      }
    }
  }

  m_formSaveInProgress = false;
  clearProbedConfig();
  // formSaveFinished was already emitted synchronously when the DB row
  // committed; here we only report the (async) REST outcome.
  if (!ok) {
    // Emitted after formSaveFinished so the dialog can close first and the
    // non-blocking banner appears afterwards.
    emit configApplyFailed(loggerId, errorMessage);
  }
  // Silence unused-variable warnings for the kept-for-debug params.
  Q_UNUSED(isAdd);
  Q_UNUSED(stationCode);
}

void LoggerFormController::beginConfigApply(qint64 loggerId,
                                            int expectedRevision,
                                            const QJsonObject &patch)
{
  // Reserved for a future fully-non-blocking refactor where the caller can
  // queue multiple apply requests. Currently saveLoggerFromForm fires the
  // apply directly via m_restConfig->applyConfig; this slot is kept so
  // future code can route through a single funnel.
  Q_UNUSED(loggerId);
  Q_UNUSED(expectedRevision);
  Q_UNUSED(patch);
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
