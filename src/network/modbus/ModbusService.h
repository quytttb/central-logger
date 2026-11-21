#pragma once

#include "ModbusPollPlan.h"
#include "ModbusTypes.h"

#include "utils/AppConstants.h"

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
    int     modbusPort     = Defaults::kDefaultModbusPort;
    int     unitId         = Defaults::kDefaultModbusUnitId;
    int     pollIntervalMs = Defaults::kDefaultPollIntervalMs;
    int     timeoutMs      = Defaults::kDefaultTimeoutMs;
    bool    enabled        = true;
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
    /// ModbusDataDispatcher::onPollFinished via Qt::QueuedConnection.
    void pollFinished(const CentralLogger::Network::PollSnapshot &snapshot);

public slots:
    /// Replaces the registry with @p configs and (re)starts polling for the
    /// enabled rows. Closes connections for loggers not in @p configs.
    void syncLoggers(const QVector<CentralLogger::Network::LoggerRuntimeConfig> &configs);

    /// Discard the current TCP connection and poll now (enabled loggers only).
    void reconnectLogger(qint64 loggerId);

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
        QTimer               *watchdog       = nullptr;
        bool                  pollInFlight   = false;
        bool                  awaitingConnection = false;
        quint64               consecutiveFailures = 0;
        // Unique to the client instance, including if a logger ID is removed
        // and registered again while a reply or socket signal is pending.
        quint64               clientEpoch    = 0;
        PollSnapshot          currentSnapshot; // built up across PDUs
        QVector<AnalogSample> analogAccum;
        QVector<PollPdu>      analogPlan;      // sequential chunk list, set in readAnalogChunks
    };

    LoggerState *stateFor(qint64 loggerId);
    void registerLogger(const LoggerRuntimeConfig &config);
    void ensureClient(LoggerState &state);
    void forceReconnect(LoggerState &state);
    bool isStaleReply(const LoggerState &state, quint64 replyEpoch) const;
    void destroyState(qint64 loggerId);
    void startPollCycle(LoggerState &state);
    void readHeader(LoggerState &state);
    void readAnalogChunks(LoggerState &state);
    void readNextAnalogChunk(LoggerState &state, int chunkIdx);
    void readDiscreteInputs(LoggerState &state);
    void readCoils(LoggerState &state);
    void finishCycle(LoggerState &state, bool success, const QString &errorMessage = {});

    QHash<qint64, LoggerState *> m_states;
    quint64 m_nextClientEpoch = 0;
    // M-4 fix: sequential stagger for the first poll of every logger so
    // startup does not open all TCP connections simultaneously.
    int m_staggerCounter = 0;
};

} // namespace CentralLogger::Network
