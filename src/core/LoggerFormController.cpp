#include "LoggerFormController.h"

#include "DashboardController.h"
#include "SettingsController.h"
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
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QSqlError>

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
            &LoggerFormController::onConfigReloadFetched);
  }
}

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

bool LoggerFormController::removeLogger(qint64 id) {
  QString err;
  if (!ensureDatabase(&err)) {
    qWarning().noquote() << "[removeLogger]" << err;
    return false;
  }

  Data::LoggerRepository repo(m_db->connection());
  const auto existing = repo.findById(id, &err);
  if (!existing) {
    qWarning().noquote() << "[removeLogger]"
                         << (err.isEmpty() ? QLatin1String(CentralLogger::Format::kErrLoggerNotFound) : err);
    return false;
  }
  if (!repo.remove(id, &err)) {
    qWarning().noquote() << "[removeLogger]" << err;
    return false;
  }
  if (auto *sc = SettingsController::instance()) {
    sc->clearDeletedPrimaryLogger(id);
  }
  if (m_dashboard) {
    m_dashboard->cleanupRemovedLogger(id);
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

bool LoggerFormController::setLoggerEnabled(qint64 id, bool enabled) {
  QString err;
  if (!ensureDatabase(&err)) {
    qWarning().noquote() << "[setLoggerEnabled]" << err;
    return false;
  }
  Data::LoggerRepository repo(m_db->connection());
  const auto existing = repo.findById(id, &err);
  if (!existing) {
    qWarning().noquote() << "[setLoggerEnabled]"
                         << (err.isEmpty() ? QLatin1String(CentralLogger::Format::kErrLoggerNotFound) : err);
    return false;
  }
  if (existing->enabled == enabled) {
    return true;
  }

  Data::LoggerInfo info = *existing;
  info.enabled = enabled;
  if (!enabled) {
    info.status = QLatin1String(CentralLogger::Sensor::kLoggerOffline);
  }
  if (!repo.update(info, &err)) {
    qWarning().noquote() << "[setLoggerEnabled]" << err;
    return false;
  }

  if (m_dashboard) {
    if (!enabled) {
      m_dashboard->markLoggerDisabled(id);
    }
    m_dashboard->logEvent(
        id, CentralLogger::Sensor::kEventTypeInfo,
        enabled ? QStringLiteral("Logger enabled: %1").arg(info.stationCode)
                : QStringLiteral("Logger disabled: %1").arg(info.stationCode));
    m_dashboard->afterMutation();
  }
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
  result.insert(QStringLiteral("enabled"), found->enabled);
  result.insert(QStringLiteral("note"), found->note);
  result.insert(QStringLiteral("status"), found->status);
  return result;
}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------

bool LoggerFormController::ensureDatabase(QString *errorOut) const {
  if (!m_db || !m_db->isOpen()) {
    if (errorOut) {
      *errorOut = QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen);
    }
    return false;
  }
  return true;
}

bool LoggerFormController::validateCommon(const QString &stationCode,
                                          const QString &name,
                                          const QString &host, int modbusPort,
                                          int apiPort, QString *errorOut) {
  auto fail = [errorOut](const QString &msg) {
    if (errorOut) {
      *errorOut = msg;
    }
    return false;
  };
  if (stationCode.isEmpty()) {
    return fail(tr("Station code is required."));
  }
  if (name.isEmpty()) {
    return fail(tr("Name is required."));
  }
  if (host.isEmpty()) {
    return fail(tr("Host is required."));
  }
  if (!HostValidator::isValidHost(host)) {
    return fail(QLatin1String(CentralLogger::Format::kErrInvalidHost));
  }
  if (modbusPort < 1 || modbusPort > 65535) {
    return fail(tr("Modbus port must be between 1 and 65535."));
  }
  if (apiPort < 1 || apiPort > 65535) {
    return fail(tr("API port must be between 1 and 65535."));
  }
  return true;
}

bool LoggerFormController::isValidHost(const QString &host) const {
  return HostValidator::isValidHost(host);
}

// ---------------------------------------------------------------------------
// probeConfig — delegate to RestConfigService
// ---------------------------------------------------------------------------

void LoggerFormController::probeConfig(const QString &host, int apiPort,
                                        const QString &token) {
  clearProbedConfig();
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
        false, QLatin1String(CentralLogger::Format::kErrInvalidHost));
    return;
  }
  m_probeEndpoint = {host.trimmed(), apiPort, token.trimmed()};
  m_probeRequested = true;
  m_restConfig->probeConfig(m_probeEndpoint.host, m_probeEndpoint.apiPort,
                           m_probeEndpoint.token);
}

void LoggerFormController::clearProbedConfig() {
  if (m_probeRequested && m_restConfig) {
    m_restConfig->cancelProbe();
  }
  m_probeRequested = false;
  m_probeEndpoint = {};
  m_probedConfig = {};
}

void LoggerFormController::reloadLoggerConfig(qint64 loggerId) {
  if (m_reloadingLoggerId > 0)
    return;

  QString error;
  if (!m_restConfig) {
    error = QLatin1String(CentralLogger::Format::kErrRestUnavailable);
  } else if (ensureDatabase(&error)) {
    Data::LoggerRepository repo(m_db->connection());
    const auto logger = repo.findById(loggerId, &error);
    if (!logger && error.isEmpty())
      error = QLatin1String(CentralLogger::Format::kErrLoggerNotFound);
    if (logger && !HostValidator::isValidHost(logger->host))
      error = QLatin1String(CentralLogger::Format::kErrInvalidHost);
  }
  if (!error.isEmpty()) {
    emit configReloadFinished(loggerId, false, error);
    return;
  }

  m_reloadingLoggerId = loggerId;
  emit reloadingLoggerChanged();
  m_restConfig->fetchConfig(loggerId);
}

void LoggerFormController::onConfigReloadFetched(qint64 loggerId, bool ok, int httpStatus,
                                                 const QString &rawJson, QString error) {
  if (m_reloadingLoggerId <= 0 || m_reloadingLoggerId != loggerId)
    return;

  if (!ok && error.isEmpty())
    error = QString(QLatin1String(CentralLogger::Format::kErrHttpFmt)).arg(httpStatus);
  if (ok) {
    const auto parsed = Network::RestConfigParser::parseConfigResponse(loggerId, rawJson.toUtf8());
    if (!parsed.valid || parsed.revision < 0) {
      error = QLatin1String(CentralLogger::Format::kErrInvalidConfig);
    } else if (ensureDatabase(&error)) {
      auto conn = m_db->connection();
      if (!conn.transaction()) {
        error = conn.lastError().text();
      } else {
        Data::LoggerRepository loggers(conn);
        Data::SensorCatalogRepository catalog(conn);
        // Update revision first to acquire the write transaction before
        // the catalog reads existing thresholds (the bridge also writes).
        if (!loggers.updateLastRevision(loggerId, parsed.revision, &error)) {
          conn.rollback();
          if (error.isEmpty())
            error = QLatin1String(CentralLogger::Format::kErrLoggerNotFound);
        } else if (!catalog.upsertBatchPreservingThresholds(loggerId, parsed.sensors, &error)
                   || !conn.commit()) {
          if (error.isEmpty())
            error = conn.lastError().text();
          if (error.isEmpty())
            error = tr("Could not update the local configuration copy.");
          conn.rollback();
        } else if (m_dashboard) {
          m_dashboard->refreshLoggerCatalog(loggerId);
          m_dashboard->logEvent(loggerId, CentralLogger::Sensor::kEventTypeInfo,
                                QStringLiteral("Config reloaded from device"));
          m_dashboard->reconnectLogger(loggerId);
        }
      }
    }
  }

  m_reloadingLoggerId = 0;
  emit reloadingLoggerChanged();
  emit configReloadFinished(loggerId, error.isEmpty(), error);
}

void LoggerFormController::saveLoggerFromForm(
    bool isAdd, qint64 loggerId, const QString &stationCode, const QString &name, const QString &host,
    int modbusPort, int apiPort, const QString &apiToken, int modbusUnitId,
    int pollIntervalS, int timeoutS, bool enabled, const QString &note) {
  const auto fail = [this, loggerId](const QString &error) {
    qWarning().noquote() << "[save] loggerId=" << loggerId << "—" << error;
    emit formSaveFinished(false, loggerId, error);
  };
  if (m_reloadingLoggerId > 0) {
    fail(tr("Reload config in progress."));
    return;
  }
  QString error;
  if (!ensureDatabase(&error)) {
    fail(error);
    return;
  }

  const ProbeEndpoint endpoint{host.trimmed(), apiPort, apiToken.trimmed()};
  QSqlDatabase conn = m_db->connection();
  Data::LoggerRepository repo(conn);
  Data::LoggerInfo info;
  if (isAdd) {
    if (!hasProbedConfig() || endpoint != m_probeEndpoint) {
      fail(tr("Load config from the current connection first (Connect)."));
      return;
    }
    info.stationCode = stationCode.trimmed();
    if (info.stationCode.isEmpty()) {
      info.stationCode = probedStationCode().trimmed();
    }
    if (info.stationCode.isEmpty()) {
      fail(tr("The device has no station code. Enter one manually."));
      return;
    }
    info.lastRevision = m_probedConfig.revision;
  } else {
    const auto existing = repo.findById(loggerId, &error);
    if (!existing) {
      fail(error.isEmpty() ? QLatin1String(CentralLogger::Format::kErrLoggerNotFound) : error);
      return;
    }
    info = *existing;
  }

  info.name = name.trimmed();
  info.host = endpoint.host;
  if (!validateCommon(info.stationCode, info.name, info.host, modbusPort, apiPort, &error)) {
    fail(error);
    return;
  }
  info.modbusPort = modbusPort;
  if (modbusUnitId > 0) {
    info.modbusUnitId = modbusUnitId;
  }
  info.centralPollIntervalS = pollIntervalS > 0 ? pollIntervalS : Defaults::kDefaultPollIntervalSec;
  if (timeoutS > 0) {
    info.timeoutS = static_cast<double>(timeoutS);
  }
  info.enabled = enabled;
  if (!enabled) {
    info.status = QLatin1String(CentralLogger::Sensor::kLoggerOffline);
  }
  info.apiPort = endpoint.apiPort;
  info.apiToken = endpoint.token;
  info.note = note;

  if (isAdd) {
    if (!conn.transaction()) {
      fail(tr("Could not start database transaction."));
      return;
    }
    if (!repo.insert(info, &error)) {
      conn.rollback();
      fail(Data::humanizeSqlError(error, info.stationCode));
      return;
    }
    Data::SensorCatalogRepository catalog(conn);
    if (!catalog.upsertBatchPreservingThresholds(info.id, m_probedConfig.sensors, &error)
        || !conn.commit()) {
      conn.rollback();
      fail(error.isEmpty() ? tr("Could not save the local logger and catalog.") : error);
      return;
    }
    clearProbedConfig();
  } else if (!repo.update(info, &error)) {
    fail(error.isEmpty() ? tr("Could not save the local logger settings.") : error);
    return;
  }

  if (m_dashboard) {
    if (!enabled) {
      m_dashboard->markLoggerDisabled(info.id);
    }
    m_dashboard->logEvent(info.id, CentralLogger::Sensor::kEventTypeInfo,
                          isAdd ? QStringLiteral("Logger added: %1").arg(info.stationCode)
                                : QStringLiteral("Logger updated: %1").arg(info.stationCode));
    m_dashboard->afterMutation();
  }
  emit formSaveFinished(true, info.id, QString{});
}

void LoggerFormController::onProbeConfigFetched(bool ok, int httpStatus,
                                                QString rawJson,
                                                QString errorMessage) {
  if (!m_probeRequested) {
    return;
  }
  m_probeRequested = false;

  if (!ok) {
    if (errorMessage.isEmpty()) {
      errorMessage = QString(QLatin1String(CentralLogger::Format::kErrHttpFmt)).arg(httpStatus);
    }
    qWarning().noquote() << "[probe] failed httpStatus=" << httpStatus << "—"
                         << errorMessage;
    clearProbedConfig();
    emit probeConfigResult(false, errorMessage);
    return;
  }

  m_probedConfig = Network::RestConfigParser::parseConfigResponse(0, rawJson.toUtf8());
  if (!hasProbedConfig()) {
    qWarning().noquote() << "[probe] invalid config response httpStatus="
                         << httpStatus;
    clearProbedConfig();
    emit probeConfigResult(false, QLatin1String(CentralLogger::Format::kErrInvalidConfig));
    return;
  }

  qInfo().noquote() << "[probe] OK revision=" << m_probedConfig.revision
                    << "stationCode=" << probedStationCode()
                    << "stationName=" << probedStationName()
                    << "sensors=" << m_probedConfig.sensors.size();
  emit probeConfigResult(true, QString{});
}

} // namespace CentralLogger::Core
