#include "LoggerDetailViewModel.h"

#include "DesktopService.h"
#include "core/charts/ChartPresentation.h"

#include <QFileInfo>

#include "AppState.h"
#include "DashboardController.h"
#include "LoggerListModel.h"
#include "core/sensors/SensorMerger.h"
#include "core/sensors/SensorSnapshotCache.h"
#include "core/charts/PollHistoryStore.h"
#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/rest/RestConfigService.h"

#include <QtDebug>

namespace CentralLogger::Core {

namespace {
Data::Database             *g_db        = nullptr;
Network::RestConfigService *g_rest      = nullptr;
AppState                   *g_appState  = nullptr;
DashboardController        *g_dashboard = nullptr;
} // namespace

void LoggerDetailViewModel::registerServices(Data::Database *db,
                                             Network::RestConfigService *rest,
                                             AppState *appState,
                                             DashboardController *dashboard)
{
    g_db        = db;
    g_rest      = rest;
    g_appState  = appState;
    g_dashboard = dashboard;
}

LoggerDetailViewModel::LoggerDetailViewModel(QObject *parent)
    : QObject(parent)
{
    if (g_db || g_rest || g_appState || g_dashboard) {
        setServices(g_db, g_rest, g_appState, g_dashboard);
    }
}

void LoggerDetailViewModel::setServices(Data::Database *db,
                                        Network::RestConfigService *rest,
                                        AppState *appState,
                                        DashboardController *dashboard)
{
    m_db       = db;
    m_appState = appState;

    if (m_rest != rest) {
        if (m_rest) {
            disconnect(m_rest, nullptr, this, nullptr);
        }
        m_rest = rest;
        if (m_rest) {
            connect(m_rest, &Network::RestConfigService::readingsDebugFetched,
                    this,   &LoggerDetailViewModel::onReadingsDebug);
            connect(m_rest, &Network::RestConfigService::reportDownloaded,
                    this,   &LoggerDetailViewModel::onReportDownloaded);
        }
    }

    if (m_dashboard != dashboard) {
        if (m_dashboard) {
            disconnect(m_dashboard, &DashboardController::loggerSnapshotUpdated,
                       this,        &LoggerDetailViewModel::onLoggerSnapshotUpdated);
        }
        m_dashboard = dashboard;
        if (m_dashboard) {
            connect(m_dashboard, &DashboardController::loggerSnapshotUpdated,
                    this,        &LoggerDetailViewModel::onLoggerSnapshotUpdated);
        }
    }

    if (m_loggerId >= 0) {
        reloadFromDatabase();
        m_ownSensorTable.setLoggerId(m_loggerId);
        refreshSensorTableFromCache();
        refreshLiveState();
    }
}

void LoggerDetailViewModel::setLoggerId(qint64 id)
{
    if (m_loggerId == id) return;
    m_loggerId = id;
    emit loggerIdChanged();

    setError({});
    setReadingsDebug({});
    m_configCatalog.clear();
    setLastModbusError({});
    reloadFromDatabase();
    reloadConfigCatalogFromDatabase();

    m_ownSensorTable.setLoggerId(id);
    refreshSensorTableFromCache();
    refreshLiveState();
    refreshTrendingSeries();
}

void LoggerDetailViewModel::refreshSensorTableFromCache()
{
    if (m_loggerId < 0) {
        m_ownSensorTable.clear();
        return;
    }

    QVector<SensorLiveRow> rows;
    if (m_dashboard) {
        if (auto *cache = m_dashboard->sensorCache()) {
            rows = cache->rowsFor(m_loggerId);
        }
    }

    if (rows.isEmpty() && !m_configCatalog.isEmpty()) {
        rows = SensorMerger::buildCatalogPlaceholders(m_loggerId, m_configCatalog);
    } else if (rows.isEmpty() && m_db && m_db->isOpen()) {
        reloadConfigCatalogFromDatabase();
        if (!m_configCatalog.isEmpty()) {
            rows = SensorMerger::buildCatalogPlaceholders(m_loggerId, m_configCatalog);
        }
    }

    m_ownSensorTable.setRows(rows);
}

void LoggerDetailViewModel::reloadConfigCatalogFromDatabase()
{
    m_configCatalog.clear();
    if (!m_db || !m_db->isOpen() || m_loggerId < 0) {
        return;
    }
    Data::SensorCatalogRepository catalog(m_db->connection());
    m_configCatalog = catalog.listByLoggerId(m_loggerId);
}

void LoggerDetailViewModel::refreshLiveState()
{
    bool online = false, polling = false, anyAlarm = false, rtu = false;

    if (m_dashboard && m_loggerId >= 0) {
        auto *loggers = m_dashboard->loggers();
        if (loggers) {
            const int row = loggers->indexOfLogger(m_loggerId);
            if (row >= 0) {
                const QModelIndex idx = loggers->index(row, 0);
                online   = loggers->data(idx, LoggerListModel::OnlineRole).toBool();
                polling  = loggers->data(idx, LoggerListModel::PollingRole).toBool();
                anyAlarm = loggers->data(idx, LoggerListModel::AnyAlarmRole).toBool();
                rtu      = loggers->data(idx, LoggerListModel::RtuConnectedRole).toBool();
            }
        }
    }

    if (online == m_online && polling == m_polling
        && anyAlarm == m_anyAlarm && rtu == m_rtuConnected) {
        return;
    }
    m_online       = online;
    m_polling      = polling;
    m_anyAlarm     = anyAlarm;
    m_rtuConnected = rtu;
    emit liveStateChanged();
}

void LoggerDetailViewModel::onLoggerSnapshotUpdated(qint64 loggerId,
                                                    bool pollSuccess,
                                                    QString modbusError)
{
    if (loggerId != m_loggerId) return;
    setLastModbusError(pollSuccess ? QString{} : modbusError);
    refreshLiveState();
    refreshSensorTableFromCache();
    refreshTrendingSeries();
}

void LoggerDetailViewModel::reloadFromDatabase()
{
    if (!m_db || !m_db->isOpen() || m_loggerId < 0) {
        setHasApiToken(false);
        return;
    }
    Data::LoggerRepository repo(m_db->connection());
    const auto info = repo.findById(m_loggerId);
    if (!info) {
        setHasApiToken(false);
        return;
    }
    setHasApiToken(!info->apiToken.isEmpty());
}

void LoggerDetailViewModel::fetchReadingsDebug()
{
    if (!m_rest || m_loggerId < 0) {
        setError(QStringLiteral("REST service not ready"));
        return;
    }
    if (!m_hasApiToken) {
        setError(QStringLiteral("Device REST token empty — Scan QR on logger"));
        return;
    }
    setBusy(true);
    setError({});
    setReadingsDebug({});
    m_rest->fetchReadingsDebug(m_loggerId);
}

void LoggerDetailViewModel::clearReadingsDebug()
{
    setReadingsDebug({});
}

void LoggerDetailViewModel::downloadReport(const QUrl &fileUrl)
{
    // M-21: toLocalFile() decodes percent-encoded characters (%20 → space)
    // and strips the file:// scheme correctly on both Linux and Windows.
    const QString savePath = fileUrl.toLocalFile();
    if (!m_rest || m_loggerId < 0) {
        emit reportDownloaded(false, QString{},
                              QStringLiteral("REST service not ready"));
        return;
    }
    if (!m_hasApiToken) {
        emit reportDownloaded(false, QString{},
                              QStringLiteral("Device REST token empty — Scan QR on logger"));
        return;
    }
    if (savePath.isEmpty()) {
        emit reportDownloaded(false, QString{},
                              QStringLiteral("No save path specified"));
        return;
    }
    if (m_reportBusy) return;
    m_reportBusy = true;
    emit reportBusyChanged();
    m_rest->downloadLatestReport(m_loggerId, savePath);
}

void LoggerDetailViewModel::onReadingsDebug(qint64 loggerId, bool ok, int httpStatus,
                                            QString rawJson, QString errorMessage)
{
    if (loggerId != m_loggerId) return;
    setBusy(false);
    setReadingsDebug(rawJson);
    if (!ok) {
        setError(errorMessage.isEmpty()
            ? QStringLiteral("HTTP %1").arg(httpStatus)
            : errorMessage);
    }
    emit readingsDebugReady(ok);
}

void LoggerDetailViewModel::onReportDownloaded(qint64 loggerId, bool ok,
                                               QString savePath, QString errorMessage)
{
    if (loggerId != m_loggerId) return;
    m_reportBusy = false;
    emit reportBusyChanged();

    if (g_dashboard) {
        if (ok) {
            const QString absPath = QFileInfo(savePath).absoluteFilePath();
            g_dashboard->logEvent(
                loggerId,
                QStringLiteral("Info"),
                DesktopService::reportSavedMessagePrefix() + absPath);
        } else {
            g_dashboard->logEvent(loggerId, QStringLiteral("Warning"),
                                  QStringLiteral("Report download failed: %1").arg(errorMessage));
        }
    }

    emit reportDownloaded(ok, savePath, errorMessage);
}

void LoggerDetailViewModel::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

void LoggerDetailViewModel::setError(const QString &message)
{
    if (m_lastError == message) return;
    m_lastError = message;
    emit lastErrorChanged();
}

void LoggerDetailViewModel::setReadingsDebug(const QString &raw)
{
    if (m_readingsDebugJson == raw) return;
    m_readingsDebugJson = raw;
    emit readingsDebugJsonChanged();
}

void LoggerDetailViewModel::setHasApiToken(bool has)
{
    if (m_hasApiToken == has) return;
    m_hasApiToken = has;
    emit hasApiTokenChanged();
}

void LoggerDetailViewModel::setLastModbusError(const QString &message)
{
    if (m_lastModbusError == message) return;
    m_lastModbusError = message;
    emit lastModbusErrorChanged();
}

void LoggerDetailViewModel::refreshTrendingSeries()
{
    if (!m_dashboard || m_loggerId < 0) {
        if (!m_trendingSeries.isEmpty()) {
            m_trendingSeries.clear();
            m_chartAxisRange = Core::computeTrendingAxisRange(m_trendingSeries);
            emit trendingSeriesChanged();
        }
        return;
    }
    auto *history = m_dashboard->pollHistory();
    if (!history) {
        if (!m_trendingSeries.isEmpty()) {
            m_trendingSeries.clear();
            m_chartAxisRange = Core::computeTrendingAxisRange(m_trendingSeries);
            emit trendingSeriesChanged();
        }
        return;
    }
    QVariantList newSeries = history->seriesForLogger(m_loggerId);
    if (newSeries != m_trendingSeries) {
        m_trendingSeries = std::move(newSeries);
        m_chartAxisRange = Core::computeTrendingAxisRange(m_trendingSeries);
        emit trendingSeriesChanged();
    }
}

QVariantMap LoggerDetailViewModel::snapTrendingChart(double mouseX,
                                                     double mouseY,
                                                     double plotX,
                                                     double plotY,
                                                     double plotW,
                                                     double plotH) const
{
    const double xMin = m_chartAxisRange.value(QStringLiteral("xMin")).toDouble();
    const double xMax = m_chartAxisRange.value(QStringLiteral("xMax")).toDouble();
    return Core::snapTrendingChart(m_trendingSeries,
                                  xMin,
                                  xMax,
                                  plotX,
                                  plotY,
                                  plotW,
                                  plotH,
                                  mouseX,
                                  mouseY);
}

} // namespace CentralLogger::Core
