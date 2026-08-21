#pragma once

// QML-facing wrapper around CentralLogger::Defaults + UI constants.
// Centralizes ports, intervals, decimals, retention, history limits so QML
// views (LoggerFormDialog SpinBox defaults, AppNotifier copy, etc.) match
// the C++ defaults exactly without hardcoding literals.
//
// Registered as a QML_SINGLETON under CentralLogger.Core.AppDefaults.

#include "utils/AppConstants.h"
#include "utils/UiConstants.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

class AppDefaults : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // --- Network defaults ----------------------------------------------------
    Q_PROPERTY(int modbusPort READ modbusPort CONSTANT)
    Q_PROPERTY(int apiPort    READ apiPort    CONSTANT)
    Q_PROPERTY(int modbusUnitId READ modbusUnitId CONSTANT)

    // --- Polling / timing defaults ------------------------------------------
    Q_PROPERTY(int pollIntervalSec READ pollIntervalSec CONSTANT)
    Q_PROPERTY(int timeoutSec      READ timeoutSec      CONSTANT)
    Q_PROPERTY(int minIntervalSec  READ minIntervalSec  CONSTANT)
    Q_PROPERTY(int maxIntervalSec  READ maxIntervalSec  CONSTANT)
    Q_PROPERTY(int msPerSecond     READ msPerSecond     CONSTANT)

    // --- Modbus + REST -------------------------------------------------------
    Q_PROPERTY(int modbusHeartbeatMs      READ modbusHeartbeatMs      CONSTANT)
    Q_PROPERTY(int restTransferTimeoutMs  READ restTransferTimeoutMs  CONSTANT)
    Q_PROPERTY(int restProbeTimeoutMs     READ restProbeTimeoutMs     CONSTANT)
    Q_PROPERTY(int restReportTimeoutMs    READ restReportTimeoutMs    CONSTANT)
    Q_PROPERTY(int restConfigPushTimeoutMs READ restConfigPushTimeoutMs CONSTANT)

    // --- Retention / history -------------------------------------------------
    Q_PROPERTY(int defaultRetentionDays  READ defaultRetentionDays  CONSTANT)
    Q_PROPERTY(int recentEventsLimit     READ recentEventsLimit    CONSTANT)
    Q_PROPERTY(int historyFlushIntervalS READ historyFlushIntervalS CONSTANT)
    Q_PROPERTY(int historyMaxQueueSize   READ historyMaxQueueSize   CONSTANT)
    Q_PROPERTY(int purgeIntervalMs       READ purgeIntervalMs       CONSTANT)

    // --- Charts --------------------------------------------------------------
    Q_PROPERTY(int chartDisplayPointCount READ chartDisplayPointCount CONSTANT)
    Q_PROPERTY(int chartDefaultBucketMin  READ chartDefaultBucketMin  CONSTANT)

    // --- Sensor display precision -------------------------------------------
    Q_PROPERTY(int decimalsMin     READ decimalsMin     CONSTANT)
    Q_PROPERTY(int decimalsMax     READ decimalsMax     CONSTANT)
    Q_PROPERTY(int decimalsDefault READ decimalsDefault CONSTANT)

    // --- Logging -------------------------------------------------------------
    Q_PROPERTY(qint64 logMaxBytes READ logMaxBytes CONSTANT)
    Q_PROPERTY(int     logKeepBackups READ logKeepBackups CONSTANT)

    // --- SQLite --------------------------------------------------------------
    Q_PROPERTY(qint64 sqliteMmapSize      READ sqliteMmapSize      CONSTANT)
    Q_PROPERTY(int     sqliteBusyTimeoutMs READ sqliteBusyTimeoutMs CONSTANT)

    // --- Theme / timezone defaults ------------------------------------------
    Q_PROPERTY(QString themeDark  READ themeDark  CONSTANT)
    Q_PROPERTY(QString themeLight READ themeLight CONSTANT)
    Q_PROPERTY(QString defaultTimezone READ defaultTimezone CONSTANT)

public:
    explicit AppDefaults(QObject *parent = nullptr);

    int     modbusPort()      const { return Defaults::kDefaultModbusPort; }
    int     apiPort()         const { return Defaults::kDefaultApiPort; }
    int     modbusUnitId()    const { return Defaults::kDefaultModbusUnitId; }
    int     pollIntervalSec() const { return Defaults::kDefaultPollIntervalSec; }
    int     timeoutSec()      const { return Defaults::kDefaultTimeoutSec; }
    int     minIntervalSec()  const { return Defaults::kMinIntervalSec; }
    int     maxIntervalSec()  const { return Defaults::kMaxIntervalSec; }
    int     msPerSecond()     const { return Defaults::kMsPerSecond; }

    int     modbusHeartbeatMs()       const { return Defaults::kModbusHeartbeatMs; }
    int     restTransferTimeoutMs()   const { return Defaults::kRestTransferTimeoutMs; }
    int     restProbeTimeoutMs()      const { return Defaults::kRestProbeTimeoutMs; }
    int     restReportTimeoutMs()     const { return Defaults::kRestReportTimeoutMs; }
    int     restConfigPushTimeoutMs() const { return Defaults::kRestConfigPushTimeoutMs; }

    int     defaultRetentionDays()  const { return Defaults::kDefaultRetentionDays; }
    int     recentEventsLimit()     const { return Defaults::kRecentEventsLimit; }
    int     historyFlushIntervalS() const { return Defaults::kHistoryFlushIntervalS; }
    int     historyMaxQueueSize()   const { return Defaults::kHistoryMaxQueueSize; }
    int     purgeIntervalMs()       const { return Defaults::kPurgeIntervalMs; }

    int     chartDisplayPointCount() const { return Defaults::kChartDisplayPointCount; }
    int     chartDefaultBucketMin()  const { return Defaults::kChartDefaultBucketMin; }

    int     decimalsMin()     const { return Defaults::kDecimalsMin; }
    int     decimalsMax()     const { return Defaults::kDecimalsMax; }
    int     decimalsDefault() const { return Defaults::kDecimalsDefault; }

    qint64  logMaxBytes()     const { return Defaults::kLogMaxBytes; }
    int     logKeepBackups()  const { return Defaults::kLogKeepBackups; }

    qint64  sqliteMmapSize()       const { return Defaults::kSqliteMmapSize; }
    int     sqliteBusyTimeoutMs()  const { return Defaults::kSqliteBusyTimeoutMs; }

    QString themeDark()       const { return QString::fromUtf8(Ui::kThemeDark); }
    QString themeLight()      const { return QString::fromUtf8(Ui::kThemeLight); }
    QString defaultTimezone() const { return QString::fromUtf8(Ui::kDefaultTimezone); }
};

} // namespace CentralLogger::Core
