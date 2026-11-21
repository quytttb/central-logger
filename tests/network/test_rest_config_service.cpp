#include "../fixtures/ReadOnlyEdgeHttpServer.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "network/rest/RestConfigService.h"

#include <QSignalSpy>
#include <QTest>

using namespace CentralLogger::Network;
using namespace CentralLogger::Data;

class TestRestConfigService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void fetchConfigEmitsErrorWhenNoDB();
    void probeRejectsEmptyHost();
    void fetchConfigEmitsErrorWhenLoggerNotFound();
    void readingsDebugEmitsErrorWhenNoToken();
    void fetchConfigRoundTripCoalescesDuplicate();

private:
    Database      m_db;
};

void TestRestConfigService::initTestCase()
{
    QVERIFY(m_db.open(QStringLiteral("test_rest_config_service_conn"), QStringLiteral(":memory:")));
}

void TestRestConfigService::cleanupTestCase()
{
    m_db.close();
}

void TestRestConfigService::fetchConfigEmitsErrorWhenNoDB()
{
    RestConfigService svc;
    // No DB set — resolveEndpoint will fail.
    QSignalSpy spy(&svc, &RestConfigService::configFetched);
    svc.fetchConfig(1);
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toLongLong(), qint64(1));  // loggerId
    QCOMPARE(args.at(1).toBool(), false);           // ok
    QVERIFY(!args.at(4).toString().isEmpty());      // errorMessage
}

void TestRestConfigService::probeRejectsEmptyHost()
{
    RestConfigService svc;
    QSignalSpy spy(&svc, &RestConfigService::probeConfigFetched);
    svc.probeConfig(QString{}, 8080, QString{});
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toBool(), false);
    QVERIFY(!args.at(3).toString().isEmpty());
}

void TestRestConfigService::fetchConfigEmitsErrorWhenLoggerNotFound()
{
    RestConfigService svc;
    svc.setDatabase(&m_db);
    QSignalSpy spy(&svc, &RestConfigService::configFetched);
    svc.fetchConfig(999);
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(4).toString().contains(QStringLiteral("not found")));
}

void TestRestConfigService::readingsDebugEmitsErrorWhenNoToken()
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
    QSignalSpy spy(&svc, &RestConfigService::readingsDebugFetched);
    svc.fetchReadingsDebug(info.id);
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QVERIFY(args.at(4).toString().contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void TestRestConfigService::fetchConfigRoundTripCoalescesDuplicate()
{
    ReadOnlyEdgeHttpServer server;
    QVERIFY(server.port() != 0);
    LoggerRepository repo(m_db.connection());
    LoggerInfo info;
    info.stationCode = QStringLiteral("READ-HTTP");
    info.name = QStringLiteral("Read Config Logger");
    info.host = QStringLiteral("127.0.0.1");
    info.apiPort = server.port();
    info.apiToken = QStringLiteral("test-token");
    QVERIFY(repo.insert(info));

    RestConfigService svc;
    svc.setDatabase(&m_db);
    QSignalSpy spy(&svc, &RestConfigService::configFetched);
    svc.fetchConfig(info.id);
    svc.fetchConfig(info.id);
    QTRY_COMPARE(spy.count(), 1);
    const auto reply = spy.takeFirst();
    QCOMPARE(reply.at(0).toLongLong(), info.id);
    QVERIFY(reply.at(1).toBool());
    QCOMPARE(reply.at(2).toInt(), 200);
    QCOMPARE(server.requests, QList<QByteArray>{"GET /api/v1/config HTTP/1.1"});
    const auto headers = server.headers.constFirst().toLower();
    QVERIFY(headers.contains("\r\nauthorization: bearer test-token"));
    QVERIFY(!headers.contains("\r\ncontent-type:"));
    QVERIFY(!headers.contains("\r\ncontent-length:"));
}

QTEST_MAIN(TestRestConfigService)
#include "test_rest_config_service.moc"
