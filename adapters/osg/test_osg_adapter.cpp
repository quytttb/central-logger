#include "OsgSource.h"
#include "common/ModbusV1Server.h"
#include "common/RestV1Server.h"
#include "network/modbus/ModbusMapParser.h"
#include "network/rest/RestConfigParser.h"
#include "network/rest/RestConfigService.h"

#include <QFile>
#include <QJsonDocument>
#include <QModbusTcpClient>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUuid>

using namespace CentralLogger::Adapters;
using namespace CentralLogger::Adapters::Osg;
using namespace CentralLogger::Network;

class TestOsgAdapter : public QObject
{
    Q_OBJECT

private slots:
    void parseEmptyZeroAndMilliseconds();
    void parseCatalogEnvelopeAndIds();
    void encodeV1AnalogAndQuality();
    void localRestAuthenticationAndStartup();
    void localModbusHeader();
    void unreachableSourceFailsClosed();
    void liveCentralConfigProbe();
    void osgStatusMapping();
    void osgQualityDoesNotTrustErrorStatus();
};

void TestOsgAdapter::parseEmptyZeroAndMilliseconds()
{
    float value = -1.0f;
    QVERIFY(!OsgSource::parseNumericValue(QStringLiteral(""), &value));
    QVERIFY(OsgSource::parseNumericValue(QStringLiteral("0"), &value));
    QCOMPARE(value, 0.0f);
    QVERIFY(!OsgSource::parseNumericValue(QStringLiteral("nan"), &value));
    QCOMPARE(OsgSource::parseTimestampMs(QJsonValue(1794620159951.0)), 1794620159951LL);
    QCOMPARE(OsgSource::parseTimestampMs(QJsonValue(QStringLiteral("1794620159951"))),
             1794620159951LL);
    const VerifiedQualityPolicy policy;
    QVERIFY(policy.validStatuses.isEmpty());
    QVERIFY(policy.acceptableSyncing.isEmpty());
    QVERIFY(policy.noAlarmValues.isEmpty());
    QVERIFY(policy.alarmValues.isEmpty());
    QVERIFY(policy.expectedUnits.isEmpty());
}

void TestOsgAdapter::parseCatalogEnvelopeAndIds()
{
    QJsonArray array;
    QVERIFY(OsgSource::parseArrayResponse(R"({"data":[{"id":159},{"id":160}]})", &array));
    QCOMPARE(array.size(), 2);
    quint16 id = 0;
    QVERIFY(OsgSource::parseId(array[0].toObject(), &id));
    QCOMPARE(id, quint16(159));
    QVERIFY(!OsgSource::parseId(QJsonObject{{QStringLiteral("id"), 65536}}, &id));
    QVERIFY(!OsgSource::parseArrayResponse(R"({"data":{"wrong":"shape"}})", &array));
}

void TestOsgAdapter::encodeV1AnalogAndQuality()
{
    EdgeSnapshot source;
    source.polling = true;
    source.analogs = {
        AnalogPoint{159, QStringLiteral("O2"), QStringLiteral("%"), true,
                    true, false, false, 12.5f, 1794620159951LL},
        AnalogPoint{160, QStringLiteral("CO"), QStringLiteral("mg"), true,
                    false, false, true, 0.0f, 1794620159951LL},
        AnalogPoint{161, QStringLiteral("NO"), QStringLiteral("mg"), true,
                    true, false, false, 0.0f, 1794620159951LL}
    };
    source.analogs[0].statusCode = QStringLiteral("00");
    source.analogs[1].statusCode = QStringLiteral("02");
    source.analogs[2].statusCode = QStringLiteral("03");
    const auto regs = ModbusV1Server::encodeSnapshot(source);
    const auto header = ModbusMapParser::parseHeader(regs);
    QCOMPARE(header.mapVersion, quint16(2));
    QCOMPARE(header.na, quint16(3));
    QCOMPARE(header.ndi, quint16(0));
    QCOMPARE(header.ndo, quint16(0));
    QCOMPARE(header.unixTimestamp, quint32(1794620159));
    QVERIFY(header.isPolling());
    QVERIFY(!header.isRtuConnected());
    const auto analog = ModbusMapParser::parseAnalogChunk(regs.constData() + 10, 24, header.mapVersion);
    QCOMPARE(analog.size(), 3);
    QCOMPARE(analog[0].edgeSensorId, quint16(159));
    QCOMPARE(analog[0].value, 12.5f);
    QVERIFY(analog[0].isValid());
    QCOMPARE(analog[0].statusCode, QStringLiteral("00"));
    QVERIFY(!analog[1].isValid());
    QVERIFY(analog[1].isStale());
    QCOMPARE(analog[1].statusCode, QStringLiteral("02"));
    QVERIFY(analog[2].isValid());
    QCOMPARE(analog[2].value, 0.0f);
    QCOMPARE(analog[2].statusCode, QStringLiteral("03"));
}

void TestOsgAdapter::osgStatusMapping()
{
    for (int i = 0; i <= 3; ++i)
        QCOMPARE(OsgSource::statusCodeFromOsg(QJsonValue(i)),
                 QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')));
    QVERIFY(OsgSource::statusCodeFromOsg(QJsonValue(4)).isEmpty());
    QVERIFY(OsgSource::statusCodeFromOsg(QJsonValue(1.5)).isEmpty());
    QVERIFY(OsgSource::statusCodeFromOsg(QJsonValue(QStringLiteral("0"))).isEmpty());
    QVERIFY(OsgSource::statusCodeFromOsg(QJsonValue()).isEmpty());
}

void TestOsgAdapter::osgQualityDoesNotTrustErrorStatus()
{
    QTcpServer upstream;
    QVERIFY(upstream.listen(QHostAddress::LocalHost, 0));
    int status = 0;
    bool alarm = false;
    connect(&upstream, &QTcpServer::newConnection, &upstream, [&]() {
        while (auto *socket = upstream.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                const QByteArray request = socket->readAll();
                QByteArray body;
                if (request.contains("/api/auth/login")) {
                    body = R"({"data":{"accessToken":"test-token"}})";
                } else if (request.contains("/api/sensors")) {
                    body = R"({"data":[{"id":164,"name":"O2","unit":"%V","precision":1,"enabled":true}]})";
                } else if (request.contains("/api/data/realtime")) {
                    const QJsonObject reading{{QStringLiteral("id"), 164},
                                              {QStringLiteral("name"), QStringLiteral("O2")},
                                              {QStringLiteral("unit"), QStringLiteral("%V")},
                                              {QStringLiteral("value"), QStringLiteral("7.5")},
                                              {QStringLiteral("enabled"), true},
                                              {QStringLiteral("syncing"), true},
                                              {QStringLiteral("alert"), alarm},
                                              {QStringLiteral("status"), status},
                                              {QStringLiteral("time"), double(QDateTime::currentMSecsSinceEpoch())}};
                    body = QJsonDocument(QJsonObject{{QStringLiteral("data"), QJsonArray{reading}}})
                               .toJson(QJsonDocument::Compact);
                } else {
                    body = "{}";
                }
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    Settings settings;
    settings.url = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(upstream.serverPort()));
    settings.pollIntervalMs = 30;
    settings.timeoutMs = 1000;
    settings.staleAfterMs = 30000;
    settings.analogIds = {164};
    settings.credentials = {QStringLiteral("test"), QStringLiteral("test")};
    // Even a mistaken policy cannot mark ERROR or AUTO as a good measurement.
    settings.quality.validStatuses = QJsonArray{0, 1, 2, 3};
    settings.quality.acceptableSyncing = QJsonArray{true};
    settings.quality.noAlarmValues = QJsonArray{false};
    settings.quality.alarmValues = QJsonArray{true};
    settings.quality.expectedUnits = QJsonObject{{QStringLiteral("164"), QStringLiteral("%V")}};
    OsgSource source(settings);
    source.start();
    QTRY_VERIFY_WITH_TIMEOUT(!source.snapshot().analogs.isEmpty(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(source.snapshot().analogs[0].valid, 3000);
    QCOMPARE(source.snapshot().analogs[0].statusCode, QStringLiteral("00"));
    QVERIFY(source.snapshot().analogs[0].decimals.has_value());
    QCOMPARE(*source.snapshot().analogs[0].decimals, 1);
    VirtualStation station{QStringLiteral("TEST"), QStringLiteral("Test"), 10,
                           QHostAddress::LocalHost, 1};
    RestV1Server rest(source, station, QByteArray("test-adapter-token"));
    QVERIFY(rest.listen(QHostAddress::LocalHost, 0));
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    QNetworkRequest configRequest(QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/config")
                                           .arg(rest.serverPort())));
    configRequest.setRawHeader("Authorization", "Bearer test-adapter-token");
    auto *reply = manager.get(configRequest);
    QSignalSpy configDone(reply, &QNetworkReply::finished);
    QVERIFY(configDone.wait(3000));
    const auto config = QJsonDocument::fromJson(reply->readAll()).object()
                            .value(QStringLiteral("config")).toObject();
    QCOMPARE(config.value(QStringLiteral("sensors")).toArray()[0].toObject()
                 .value(QStringLiteral("decimals")).toInt(), 1);
    reply->deleteLater();
    alarm = true;
    QTRY_VERIFY_WITH_TIMEOUT(source.snapshot().analogs[0].alarm, 3000);
    QVERIFY(source.snapshot().analogs[0].valid);
    QCOMPARE(source.snapshot().analogs[0].statusCode, QStringLiteral("00"));
    for (status = 1; status <= 3; ++status) {
        const QString code = QStringLiteral("%1").arg(status, 2, 10, QLatin1Char('0'));
        QTRY_COMPARE_WITH_TIMEOUT(source.snapshot().analogs[0].statusCode, code, 3000);
        QVERIFY(!source.snapshot().analogs[0].valid);
    }
    source.stop();
    QVERIFY(source.snapshot().analogs[0].statusCode.isEmpty());
    QVERIFY(source.snapshot().analogs[0].stale);
}

void TestOsgAdapter::localRestAuthenticationAndStartup()
{
    OsgSource source(Settings{});
    VirtualStation station{QStringLiteral("TEST"), QStringLiteral("Test"), 10,
                           QHostAddress::LocalHost, 1};
    const QByteArray token = QUuid::createUuid().toString(QUuid::Id128).toUtf8();
    RestV1Server server(source, station, token);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/api/v1/config").arg(server.serverPort()));
    auto request = QNetworkRequest(url);
    auto *reply = manager.get(request);
    QSignalSpy finished(reply, &QNetworkReply::finished);
    QVERIFY(finished.wait(3000));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
    reply->deleteLater();

    request.setRawHeader("Authorization", "Bearer " + token);
    reply = manager.get(request);
    QSignalSpy second(reply, &QNetworkReply::finished);
    QVERIFY(second.wait(3000));
    // Central must not save a misleading empty catalog before the first poll.
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 503);
    reply->deleteLater();
}

void TestOsgAdapter::localModbusHeader()
{
    QTcpServer freePort;
    QVERIFY(freePort.listen(QHostAddress::LocalHost, 0));
    const int port = freePort.serverPort();
    freePort.close();

    OsgSource source(Settings{});
    ModbusV1Server server(source, 5000);
    server.setServerAddress(1);
    server.setConnectionParameter(QModbusDevice::NetworkAddressParameter, QStringLiteral("127.0.0.1"));
    server.setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    QModbusDataUnitMap map;
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 10));
    QVERIFY(server.setMap(map));
    QVERIFY(server.connectDevice());

    QModbusTcpClient client;
    client.setConnectionParameter(QModbusDevice::NetworkAddressParameter, QStringLiteral("127.0.0.1"));
    client.setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    QVERIFY(client.connectDevice());
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QModbusDevice::ConnectedState, 3000);
    auto *reply = client.sendReadRequest(QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 10), 1);
    QVERIFY(reply);
    QSignalSpy done(reply, &QModbusReply::finished);
    if (!reply->isFinished())
        QVERIFY(done.wait(3000));
    QCOMPARE(reply->error(), QModbusDevice::NoError);
    QCOMPARE(reply->result().value(0), quint16(2));
    QCOMPARE(reply->result().value(4), quint16(0));
    reply->deleteLater();

    // The server must reject another client while the active client's
    // snapshot lease is in use; Qt's request callback is not per-client.
    QTcpSocket secondClient;
    secondClient.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(secondClient.waitForConnected(3000));
    QTRY_COMPARE_WITH_TIMEOUT(secondClient.state(), QAbstractSocket::UnconnectedState, 3000);

    client.disconnectDevice();
    server.disconnectDevice();
}

void TestOsgAdapter::unreachableSourceFailsClosed()
{
    QTcpServer freePort;
    QVERIFY(freePort.listen(QHostAddress::LocalHost, 0));
    const int port = freePort.serverPort();
    freePort.close();

    Settings settings;
    settings.url = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));
    settings.pollIntervalMs = 10'000;
    settings.timeoutMs = 1000;
    settings.staleAfterMs = 30'000;
    settings.credentials.username = QUuid::createUuid().toString(QUuid::Id128);
    settings.credentials.password = QUuid::createUuid().toString(QUuid::Id128);
    OsgSource source(std::move(settings));
    QSignalSpy changed(&source, &OsgSource::sourceStateChanged);
    source.start();
    QVERIFY(changed.wait(3000));
    const auto result = source.snapshot();
    QVERIFY(result.polling);
    QVERIFY(!result.upstreamConnected);
    QVERIFY(!result.catalogAvailable);
    QVERIFY(result.analogs.isEmpty());
    source.stop();
}

void TestOsgAdapter::liveCentralConfigProbe()
{
    bool ok = false;
    const int port = qEnvironmentVariableIntValue("OSG_ADAPTER_LIVE_PORT", &ok);
    const QString tokenPath = qEnvironmentVariable("OSG_ADAPTER_LIVE_TOKEN_FILE");
    if (!ok || port <= 0 || tokenPath.isEmpty())
        QSKIP("Optional live probe not requested.");

    QFile tokenFile(tokenPath);
    QVERIFY(tokenFile.open(QIODevice::ReadOnly));
    const QString token = QString::fromUtf8(tokenFile.readAll().trimmed());
    QVERIFY(!token.isEmpty());

    RestConfigService service;
    QSignalSpy fetched(&service, &RestConfigService::probeConfigFetched);
    service.probeConfig(QStringLiteral("127.0.0.1"), port, token);
    if (fetched.isEmpty())
        QVERIFY(fetched.wait(10'000));
    const auto result = fetched.takeFirst();
    QVERIFY2(result.at(0).toBool(), qPrintable(result.at(3).toString()));
    QCOMPARE(result.at(1).toInt(), 200);
    const auto parsed = RestConfigParser::parseConfigResponse(0, result.at(2).toString().toUtf8());
    QVERIFY(parsed.valid);
    QVERIFY(parsed.revision >= 0);
    QVERIFY(!parsed.sensors.isEmpty());
    for (const auto &sensor : parsed.sensors)
        QCOMPARE(sensor.sensorType, QStringLiteral("ANALOG"));
}

QTEST_GUILESS_MAIN(TestOsgAdapter)
#include "test_osg_adapter.moc"
