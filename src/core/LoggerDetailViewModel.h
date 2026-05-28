#pragma once

#include "core/sensors/SensorMonitoringTableModel.h"
#include "data/models/LoggerSensor.h"

#include <QVector>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {
class RestConfigService;
} // namespace CentralLogger::Network

namespace CentralLogger::Core {

class AppState;
class DashboardController;

/// Per-Logger Detail page view-model. Live sensors, trending, debug readings,
/// and report download. Device config GET/POST is only in Add/Edit form.
class LoggerDetailViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qint64  loggerId          READ loggerId          WRITE setLoggerId          NOTIFY loggerIdChanged)
    Q_PROPERTY(bool    busy              READ busy                                          NOTIFY busyChanged)
    Q_PROPERTY(QString lastError         READ lastError                                     NOTIFY lastErrorChanged)
    Q_PROPERTY(bool    hasApiToken       READ hasApiToken                                   NOTIFY hasApiTokenChanged)
    Q_PROPERTY(QString lastModbusError  READ lastModbusError                               NOTIFY lastModbusErrorChanged)
    Q_PROPERTY(QString readingsDebugJson READ readingsDebugJson                             NOTIFY readingsDebugJsonChanged)
    Q_PROPERTY(CentralLogger::Core::SensorMonitoringTableModel *sensorTable
                   READ sensorTable CONSTANT)
    Q_PROPERTY(bool online       READ online       NOTIFY liveStateChanged)
    Q_PROPERTY(bool polling      READ polling      NOTIFY liveStateChanged)
    Q_PROPERTY(bool anyAlarm     READ anyAlarm     NOTIFY liveStateChanged)
    Q_PROPERTY(bool rtuConnected READ rtuConnected NOTIFY liveStateChanged)
    Q_PROPERTY(QVariantList trendingSeries READ trendingSeries NOTIFY trendingSeriesChanged)
    Q_PROPERTY(QVariantMap chartAxisRange READ chartAxisRange NOTIFY trendingSeriesChanged)
    Q_PROPERTY(bool reportBusy READ reportBusy NOTIFY reportBusyChanged)

public:
    explicit LoggerDetailViewModel(QObject *parent = nullptr);

    qint64  loggerId()          const { return m_loggerId; }
    bool    busy()              const { return m_busy; }
    QString lastError()         const { return m_lastError; }
    bool    hasApiToken()       const { return m_hasApiToken; }
    QString lastModbusError()   const { return m_lastModbusError; }
    QString readingsDebugJson() const { return m_readingsDebugJson; }
    SensorMonitoringTableModel *sensorTable() { return &m_ownSensorTable; }
    bool    online()       const { return m_online; }
    bool    polling()      const { return m_polling; }
    bool    anyAlarm()     const { return m_anyAlarm; }
    bool    rtuConnected() const { return m_rtuConnected; }
    QVariantList trendingSeries() const { return m_trendingSeries; }
    QVariantMap  chartAxisRange() const { return m_chartAxisRange; }
    bool    reportBusy() const { return m_reportBusy; }

    void setServices(Data::Database *db,
                     Network::RestConfigService *rest,
                     AppState *appState,
                     DashboardController *dashboard = nullptr);

    static void registerServices(Data::Database *db,
                                 Network::RestConfigService *rest,
                                 AppState *appState,
                                 DashboardController *dashboard = nullptr);

public slots:
    void setLoggerId(qint64 id);

    void fetchReadingsDebug();
    void clearReadingsDebug();

    Q_INVOKABLE void downloadReport(const QUrl &fileUrl);

    /// Multi-series trending chart tooltip snap (plot area + mouse). Empty map = miss.
    Q_INVOKABLE QVariantMap snapTrendingChart(double mouseX,
                                              double mouseY,
                                              double plotX,
                                              double plotY,
                                              double plotW,
                                              double plotH) const;

signals:
    void loggerIdChanged();
    void busyChanged();
    void lastErrorChanged();
    void hasApiTokenChanged();
    void lastModbusErrorChanged();
    void readingsDebugJsonChanged();
    void liveStateChanged();
    void trendingSeriesChanged();

    void readingsDebugReady(bool ok);
    void reportBusyChanged();
    void reportDownloaded(bool ok, QString savePath, QString errorMessage);

private slots:
    void onReadingsDebug(qint64 loggerId, bool ok, int httpStatus,
                         QString rawJson, QString errorMessage);
    void onLoggerSnapshotUpdated(qint64 loggerId, bool pollSuccess, QString modbusError);
    void onReportDownloaded(qint64 loggerId, bool ok, QString savePath, QString errorMessage);

private:
    void reloadFromDatabase();
    void setBusy(bool busy);
    void setError(const QString &message);
    void setReadingsDebug(const QString &raw);
    void setHasApiToken(bool has);
    void setLastModbusError(const QString &message);

    void reloadConfigCatalogFromDatabase();
    void refreshSensorTableFromCache();
    void refreshLiveState();
    void refreshTrendingSeries();

    Data::Database             *m_db        = nullptr;
    Network::RestConfigService *m_rest      = nullptr;
    AppState                   *m_appState  = nullptr;
    DashboardController        *m_dashboard = nullptr;
    SensorMonitoringTableModel  m_ownSensorTable;

    qint64      m_loggerId          = -1;
    bool        m_busy              = false;
    QString     m_lastError;
    bool        m_hasApiToken       = false;
    QString     m_lastModbusError;
    QString     m_readingsDebugJson;
    bool        m_online            = false;
    bool        m_polling           = false;
    bool        m_anyAlarm          = false;
    bool        m_rtuConnected      = false;
    QVariantList m_trendingSeries;
    QVariantMap  m_chartAxisRange;
    bool        m_reportBusy    = false;
    QVector<Data::LoggerSensor> m_configCatalog;
};

} // namespace CentralLogger::Core
