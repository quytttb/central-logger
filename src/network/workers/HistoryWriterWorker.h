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
    /// Audit H-D: cap the in-memory enqueue queue so a slow/blocked disk
    /// cannot grow memory without bound. When full, the oldest snapshot is
    /// dropped (with a qWarning) — bounded data loss beats unbounded RAM.
    static constexpr int kMaxQueueSize            = 5000;

    explicit HistoryWriterWorker(QObject *parent = nullptr);
    ~HistoryWriterWorker() override;

    void setDatabasePath(const QString &path) { m_databasePath = path; }
    void setFlushIntervalSeconds(int seconds);

    /// Number of snapshots currently waiting in the queue (thread-safe).
    int pendingCount() const;

    /// Requests an immediate async flush. Returns at once — the caller must
    /// not block. Watch flushFinished() if completion notification is needed.
    /// Audit H-D: replaces the old blocking spin-wait implementation.
    void flushPending();

signals:
    /// Emitted once an async flushPending request has drained the queue
    /// (audit H-D) — or immediately if the worker is already stopped.
    void flushFinished();

    /// Emitted when the queue overflowed and a snapshot was dropped.
    void droppedSnapshot(qint64 loggerId);

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
