#include "HistoryWriterWorker.h"

#include "data/db/Database.h"
#include "network/modbus/ModbusBridge.h"

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
    const int clampedMs = qBound(1, seconds, 3600) * 1000;
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

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
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
    QMutexLocker lock(&m_mutex);
    if (m_quit) {
        return;
    }
    m_queue.enqueue(std::move(snapshot));
    m_condition.wakeOne();
}

void HistoryWriterWorker::flushPending()
{
    QMutexLocker lock(&m_mutex);
    if (m_quit) {
        return;
    }
    m_flushRequested = true;
    m_condition.wakeAll();
    while (m_flushRequested) {
        m_condition.wait(&m_mutex);
    }
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
                if (!batch.isEmpty()) {
                    flushBatch(batch);
                }
                if (m_flushRequested) {
                    m_flushRequested = false;
                    m_condition.wakeAll();
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
                m_condition.wakeAll();
            }
        }
    }
}

void HistoryWriterWorker::flushBatch(QList<PollSnapshot> &batch)
{
    if (batch.isEmpty() || !m_db.isOpen()) {
        return;
    }
    m_bridge->applyBatch(batch);
    batch.clear();
}

} // namespace CentralLogger::Network
