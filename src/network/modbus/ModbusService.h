#pragma once

#include "ModbusPollPlan.h"
#include "ModbusTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QModbusReply;
class QModbusTcpClient;
class QTimer;

namespace CentralLogger::Network {

/// Per-logger runtime config snapshot. Main thread populates this from
/// LoggerInfo and sends it to the service via queued slots.
struct LoggerRuntimeConfig
{
    qint64  loggerId       = 0;
    QString host;
    int     modbusPort     = 5020;
    int     unitId         = 1;
    int     pollIntervalMs = 2000;
    int     timeoutMs      = 2000;
    bool    enabled        = true;

    bool operator==(const LoggerRuntimeConfig &other) const
    {
        return loggerId == other.loggerId
            && host == other.host
            && modbusPort == other.modbusPort
            && unitId == other.unitId
            && pollIntervalMs == other.pollIntervalMs
            && timeoutMs == other.timeoutMs
            && enabled == other.enabled;
    }

    bool operator!=(const LoggerRuntimeConfig &other) const
    {
        return !(*this == other);
    }
};

/// Owns one QModbusTcpClient per logger and a per-logger QTimer. Designed
/// to live on a dedicated QThread; all public slots are invoked via queued
/// connections from the main thread.
class ModbusService : public QObject
{
    Q_OBJECT

public:
    explicit ModbusService(QObject *parent = nullptr);
    ~ModbusService() override;

signals:
    /// Emitted at the end of every poll cycle (success or failure). Wire to
    /// ModbusBridge::applySnapshot via Qt::QueuedConnection.
    void pollFinished(const CentralLogger::Network::PollSnapshot &snapshot);

public slots:
    /// Replaces the registry with @p configs and (re)starts polling for the
    /// enabled rows. Closes connections for loggers not in @p configs.
    void syncLoggers(const QVector<CentralLogger::Network::LoggerRuntimeConfig> &configs);

    void registerLogger(const CentralLogger::Network::LoggerRuntimeConfig &config);
    void unregisterLogger(qint64 loggerId);

    /// Pauses scheduling without dropping connections. Used by
    /// SettingsController.maintenanceMode.
    void setMaintenanceMode(bool maintenance);

    /// Stops timers + closes clients. Call before quitting the worker thread.
    void shutdown();

private slots:
    void onPollTimer();

private:
    struct LoggerState
    {
        LoggerRuntimeConfig   config;
        QModbusTcpClient     *client         = nullptr;
        QTimer               *timer          = nullptr;
        bool                  pollInFlight   = false;
        ModbusHeader          lastHeader;      // cached for plan reuse
        PollSnapshot          currentSnapshot; // built up across PDUs
        QVector<AnalogSample> analogAccum;
        QVector<PollPdu>      analogPlan;      // sequential chunk list, set in readAnalogChunks
        // M-9: heap-allocated connection handle for the transient stateChanged
        // lambda created in startPollCycle. Owned here so destroyState can
        // disconnect and free it when the state is torn down before the signal fires.
        QMetaObject::Connection *connectHolder = nullptr;
    };

    LoggerState *stateFor(qint64 loggerId);
    void ensureClient(LoggerState &state);
    void destroyState(qint64 loggerId);
    void startPollCycle(LoggerState &state);
    void readHeader(LoggerState &state);
    void readAnalogChunks(LoggerState &state);
    void readNextAnalogChunk(LoggerState &state, int chunkIdx);
    void readDiscreteInputs(LoggerState &state);
    void readCoils(LoggerState &state);
    void finishCycle(LoggerState &state, bool success, const QString &errorMessage = {});

    QHash<qint64, LoggerState *> m_states;
    bool m_maintenance = false;
};

} // namespace CentralLogger::Network
