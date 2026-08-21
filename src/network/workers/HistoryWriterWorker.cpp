#include "HistoryWriterWorker.h"

#include "data/db/Database.h"
#include "network/modbus/ModbusBridge.h"
#include "utils/AppConstants.h"
#include "utils/DbConstants.h"

#include <QElapsedTimer>
#include <QSqlError>
#include <QtDebug>
#include <QtGlobal>

namespace CentralLogger::Network {

HistoryWriterWorker::HistoryWriterWorker(QObject *parent)
    : QObject(parent)
    , m_bridge(new ModbusBridge(this))
{
}

void HistoryWriterWorker::setFlushIntervalSeconds(int seconds)
{
    const int clampedMs = qBound(Defaults::kMinIntervalSec, seconds,
                                 Defaults::kMaxIntervalSec) * Defaults::kMsPerSecond;
    m_flushIntervalMs.store(clampedMs, std::memory_order_relaxed);
    QMutexLocker lock(&m_mutex);
    m_condition.wakeAll();
}

HistoryWriterWorker::~HistoryWriterWorker()
{
    shutdown();
}

void HistoryWriterWorker::releaseDatabase()
{
    m_bridge->setConnection(QSqlDatabase());
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
    }
}

void HistoryWriterWorker::start()
{
    if (m_databasePath.isEmpty()) {
        qWarning() << "HistoryWriterWorker: database path not set";
        return;
    }

    m_connectionName = QStringLiteral("history_writer");

    m_db = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), m_connectionName);
    m_db.setDatabaseName(m_databasePath);
    if (!m_db.open()) {
        qWarning() << "HistoryWriterWorker: cannot open database:" << m_db.lastError().text();
        releaseDatabase();
        return;
    }

    QString pragmaErr;
    if (!Data::Database::applyPerformancePragmas(m_db, &pragmaErr)) {
        qWarning() << "HistoryWriterWorker: performance pragmas failed:" << pragmaErr;
    }

    m_bridge->setConnection(m_db);
    processLoop();
    releaseDatabase();
}

void HistoryWriterWorker::enqueue(PollSnapshot snapshot)
{
    qint64 droppedLoggerId = 0;
    bool dropped = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_quit) {
            return;
        }
        if (m_queue.size() >= kMaxQueueSize) {
            const PollSnapshot victim = m_queue.dequeue();
            droppedLoggerId = victim.loggerId;
            dropped = true;
        }
        m_queue.enqueue(std::move(snapshot));
        m_condition.wakeOne();
    }
    if (dropped) {
        // H-D: bounded queue — signal once per overflow event.
        qWarning() << "HistoryWriterWorker: queue full (" << kMaxQueueSize
                   << ") — dropping oldest snapshot for logger" << droppedLoggerId;
        emit droppedSnapshot(droppedLoggerId);
    }
}

void HistoryWriterWorker::flushPending()
{
    // H-D: async flush — the caller (UI thread) must never block waiting on
    // the writer. The worker loop drains the queue on its own connection and
    // acknowledges via flushFinished().
    QMutexLocker lock(&m_mutex);
    if (m_quit) {
        // No loop running: nothing to flush. Notify anyway so waiting UI
        // flows can proceed. (Queued emit — never emits from the caller's
        // stack frame into a slot on the same stack.)
        lock.unlock();
        emit flushFinished();
        return;
    }
    m_flushRequested = true;
    m_condition.wakeAll();
}

void HistoryWriterWorker::shutdown()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_quit) {
            return;
        }
        m_quit = true;
        m_condition.wakeAll();
    }
}

void HistoryWriterWorker::processLoop()
{
    QList<PollSnapshot> batch;
    batch.reserve(kMaxBatchSize);
    QElapsedTimer flushTimer;
    flushTimer.start();

    for (;;) {
        bool forceFlush = false;
        {
            QMutexLocker lock(&m_mutex);
            while (m_queue.isEmpty() && !m_quit && !m_flushRequested) {
                const int intervalMs = flushIntervalMs();
                const int remaining  = intervalMs - static_cast<int>(flushTimer.elapsed());
                if (remaining <= 0) {
                    break;
                }
                m_condition.wait(&m_mutex, remaining);
            }

            if (m_flushRequested) {
                forceFlush = true;
                while (!m_queue.isEmpty()) {
                    batch.append(m_queue.dequeue());
                }
            } else {
                while (!m_queue.isEmpty() && batch.size() < kMaxBatchSize) {
                    batch.append(m_queue.dequeue());
                }
            }

            if (m_quit && m_queue.isEmpty()) {
                const bool hadPendingFlush = m_flushRequested;
                m_flushRequested = false;
                QList<PollSnapshot> tail = std::move(batch);
                batch.clear();
                lock.unlock();
                if (!tail.isEmpty()) {
                    flushBatch(tail);
                }
                if (hadPendingFlush) {
                    emit flushFinished(); // H-D: acknowledge pending flush
                }
                return;
            }
        }

        const int intervalMs    = flushIntervalMs();
        const bool sizeFlush    = batch.size() >= kMaxBatchSize;
        const bool timeoutFlush = !batch.isEmpty()
            && flushTimer.elapsed() >= intervalMs;

        if (sizeFlush || timeoutFlush || forceFlush) {
            flushBatch(batch);
            batch.clear();
            flushTimer.restart();
            if (forceFlush) {
                QMutexLocker lock(&m_mutex);
                m_flushRequested = false;
                lock.unlock();
                // H-D: async flush acknowledgement (audit).
                emit flushFinished();
            }
        }
    }
}

void HistoryWriterWorker::flushBatch(QList<PollSnapshot> &batch)
{
    if (batch.isEmpty() || !m_db.isOpen()) {
        return;
    }
    m_bridge->applyBatch(batch, m_db);
    batch.clear();
}

} // namespace CentralLogger::Network
