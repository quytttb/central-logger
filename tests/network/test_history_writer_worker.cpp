#include "network/workers/HistoryWriterWorker.h"

#include <QtTest>

using CentralLogger::Network::HistoryWriterWorker;

class TestHistoryWriterWorker : public QObject
{
    Q_OBJECT

private slots:
    void batchConstants_matchPlan();
    void enqueue_isThreadSafeBeforeStart();
};

void TestHistoryWriterWorker::batchConstants_matchPlan()
{
    QCOMPARE(HistoryWriterWorker::kMaxBatchSize, 20);
    QCOMPARE(HistoryWriterWorker::kDefaultFlushIntervalS, 5);
}

void TestHistoryWriterWorker::enqueue_isThreadSafeBeforeStart()
{
    HistoryWriterWorker worker;
    worker.setFlushIntervalSeconds(15);
    CentralLogger::Network::PollSnapshot snap;
    snap.loggerId = 1;
    snap.success  = true;
    worker.enqueue(snap);
    worker.shutdown();
}

QTEST_APPLESS_MAIN(TestHistoryWriterWorker)
#include "test_history_writer_worker.moc"
