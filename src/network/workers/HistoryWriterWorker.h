#pragma once

#include "network/modbus/ModbusTypes.h"

#include <QList>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSqlDatabase>
#include <QWaitCondition>
#include <atomic>

namespace CentralLogger::Network {

class ModbusBridge;

/// Background batch writer for sensor_reading rows. Lives on a dedicated
/// QThread; snapshots are enqueued from the main thread via
/// ModbusDataDispatcher.
class HistoryWriterWorker : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxBatchSize            = 20;
    static constexpr int kDefaultFlushIntervalS   = 5;

    explicit HistoryWriterWorker(QObject *parent = nullptr);
    ~HistoryWriterWorker() override;

    void setDatabasePath(const QString &path) { m_databasePath = path; }
    void setFlushIntervalSeconds(int seconds);

    /// Drains the pending queue and writes all snapshots to SQLite immediately.
    /// Blocks the caller until the flush completes (thread-safe).
    void flushPending();

public slots:
    void start();
    void enqueue(CentralLogger::Network::PollSnapshot snapshot);
    void shutdown();

private:
    int flushIntervalMs() const { return m_flushIntervalMs.load(std::memory_order_relaxed); }

    void processLoop();
    void flushBatch(QList<PollSnapshot> &batch);
    void releaseDatabase();

    QString              m_databasePath;
    QString              m_connectionName;
    QSqlDatabase         m_db;
    ModbusBridge        *m_bridge = nullptr;

    QMutex               m_mutex;
    QWaitCondition       m_condition;
    QQueue<PollSnapshot> m_queue;
    bool                 m_quit           = false;
    bool                 m_flushRequested = false;
    std::atomic<int>     m_flushIntervalMs{kDefaultFlushIntervalS * 1000};
};

} // namespace CentralLogger::Network
