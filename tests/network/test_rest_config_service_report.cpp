#include "network/rest/RestConfigService.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace CentralLogger::Network;
using namespace CentralLogger::Data;

class TestRestConfigServiceReport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void downloadReportEmitsErrorWhenNoDB();
    void downloadReportEmitsErrorWhenEmptySavePath();
    void downloadReportEmitsErrorWhenLoggerNotFound();
    void downloadReportEmitsErrorWhenNoToken();
    void downloadReportEmitsAlreadyInFlightOnDuplicate();

private:
    QTemporaryDir m_tmpDir;
    Database      m_db;
};

void TestRestConfigServiceReport::initTestCase()
{
    QVERIFY(m_tmpDir.isValid());
    QVERIFY(m_db.open(QStringLiteral("test_report_conn"), QStringLiteral(":memory:")));
}

void TestRestConfigServiceReport::cleanupTestCase()
{
    m_db.close();
}

void TestRestConfigServiceReport::downloadReportEmitsErrorWhenNoDB()
{
    RestConfigService svc;
    // No DB set — resolveEndpoint will fail.
    QSignalSpy spy(&svc, &RestConfigService::reportDownloaded);
    svc.downloadLatestReport(1, m_tmpDir.filePath(QStringLiteral("test.txt")));
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toLongLong(), qint64(1));  // loggerId
    QCOMPARE(args.at(1).toBool(), false);           // ok
    QVERIFY(!args.at(3).toString().isEmpty());      // errorMessage
}

void TestRestConfigServiceReport::downloadReportEmitsErrorWhenEmptySavePath()
{
    RestConfigService svc;
    svc.setDatabase(&m_db);
    QSignalSpy spy(&svc, &RestConfigService::reportDownloaded);
    svc.downloadLatestReport(1, QString{});
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(3).toString().contains(QStringLiteral("save path")));
}

void TestRestConfigServiceReport::downloadReportEmitsErrorWhenLoggerNotFound()
{
    RestConfigService svc;
    svc.setDatabase(&m_db);
    QSignalSpy spy(&svc, &RestConfigService::reportDownloaded);
    svc.downloadLatestReport(999, m_tmpDir.filePath(QStringLiteral("test.txt")));
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(3).toString().contains(QStringLiteral("not found")));
}

void TestRestConfigServiceReport::downloadReportEmitsErrorWhenNoToken()
{
    // Insert a logger with host + api_port but NO token.
    LoggerRepository repo(m_db.connection());
    LoggerInfo info;
    info.stationCode  = QStringLiteral("NOTOKEN");
    info.name         = QStringLiteral("No Token Logger");
    info.host         = QStringLiteral("192.168.1.1");
    info.modbusPort   = 502;
    info.modbusUnitId = 1;
    info.apiPort      = 8080;
    info.apiToken     = QString{}; // empty token
    QString err;
    QVERIFY2(repo.insert(info, &err), qPrintable(err));
    QVERIFY(info.id > 0);

    RestConfigService svc;
    svc.setDatabase(&m_db);
    QSignalSpy spy(&svc, &RestConfigService::reportDownloaded);
    svc.downloadLatestReport(info.id, m_tmpDir.filePath(QStringLiteral("test.txt")));
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(3).toString().contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void TestRestConfigServiceReport::downloadReportEmitsAlreadyInFlightOnDuplicate()
{
    // Insert a logger with full connection info + token.
    LoggerRepository repo(m_db.connection());
    LoggerInfo info;
    info.stationCode  = QStringLiteral("INFLGT");
    info.name         = QStringLiteral("Inflight Test");
    info.host         = QStringLiteral("127.0.0.1");
    info.modbusPort   = 502;
    info.modbusUnitId = 1;
    info.apiPort      = 1;     // unlikely port — request will fail/timeout
    info.apiToken     = QStringLiteral("testtoken");
    QString err;
    QVERIFY2(repo.insert(info, &err), qPrintable(err));
    QVERIFY(info.id > 0);

    RestConfigService svc;
    svc.setDatabase(&m_db);

    const QString path = m_tmpDir.filePath(QStringLiteral("test2.txt"));

    // First call: starts HTTP request (will time out eventually).
    svc.downloadLatestReport(info.id, path);

    // Second call while first is in flight → immediate error signal.
    QSignalSpy spy(&svc, &RestConfigService::reportDownloaded);
    svc.downloadLatestReport(info.id, path);
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(3).toString().contains(QStringLiteral("already in progress")));
}

QTEST_MAIN(TestRestConfigServiceReport)
#include "test_rest_config_service_report.moc"
