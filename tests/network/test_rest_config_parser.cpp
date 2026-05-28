#include "network/rest/RestConfigParser.h"

#include <QByteArray>
#include <QTest>

using namespace CentralLogger::Network;

class TestRestConfigParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesRevisionAndSensorsAtRoot();
    void parsesParentIdAndDiType();
    void parsesSensorsNestedUnderConfig();
    void parsesModbusTcpUnitIdFromConfig();
    void skipsSensorsMissingId();
    void invalidJsonReturnsValidFalse();
    void parsesApplyResponse();
    void formatRestErrorMaps401TokenEmpty();
    void formatRestErrorMaps401TokenMismatch();
    void formatRestErrorMaps409Revision();
    void formatRestErrorMaps422Revision();
    void formatRestErrorMaps422MissingFieldsNotRevision();
    void formatRestErrorMaps422MissingApiVersion();
    void formatRestErrorMaps404();
    void formatRestErrorTransportFallback();
    void prettyJsonRoundTrip();
};

void TestRestConfigParser::parsesRevisionAndSensorsAtRoot()
{
    const QByteArray body = R"({
        "revision": 7,
        "config": {
            "station_code": "STN01",
            "station_name": "Hanoi"
        },
        "sensors": [
            {"sensor_id": 1, "sensor_type": "ANALOG", "name": "PH", "unit": "pH", "min_threshold": 0.0, "max_threshold": 14.0},
            {"sensor_id": 2, "sensor_type": "di",    "name": "Pump-1"}
        ]
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(42, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.revision, 7);
    QCOMPARE(parsed.configObject.value("station_code").toString(), QStringLiteral("STN01"));
    QCOMPARE(parsed.sensors.size(), 2);
    QCOMPARE(parsed.sensors[0].loggerId,     qint64(42));
    QCOMPARE(parsed.sensors[0].edgeSensorId, 1);
    QCOMPARE(parsed.sensors[0].sensorType,   QStringLiteral("ANALOG"));
    QCOMPARE(parsed.sensors[0].name,         QStringLiteral("PH"));
    QCOMPARE(parsed.sensors[0].unit,         QStringLiteral("pH"));
    QVERIFY(parsed.sensors[0].minThreshold.has_value());
    QCOMPARE(parsed.sensors[0].minThreshold.value(), 0.0);
    QCOMPARE(parsed.sensors[1].sensorType,   QStringLiteral("DI"));
}

void TestRestConfigParser::parsesParentIdAndDiType()
{
    const QByteArray body = R"({
        "revision": 1,
        "sensors": [
            {"id": 10, "sensor_type": "ANALOG", "name": "Tank"},
            {"id": 11, "sensor_type": "DI", "parent_id": 10, "di_type": "02", "name": "FaultDI"}
        ]
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.sensors.size(), 2);
    QVERIFY(!parsed.sensors[1].parentEdgeSensorId.has_value()
            || parsed.sensors[1].parentEdgeSensorId.value() == 10);
    QCOMPARE(parsed.sensors[1].diType, QStringLiteral("02"));
}

void TestRestConfigParser::parsesModbusTcpUnitIdFromConfig()
{
    const QByteArray body = R"({
        "revision": 1,
        "config": {
            "modbus_tcp_unit_id": 2,
            "station_code": "STN01"
        }
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.modbusTcpUnitId, 2);
}

void TestRestConfigParser::parsesSensorsNestedUnderConfig()
{
    const QByteArray body = R"({
        "revision": 3,
        "config": {
            "sensors": [
                {"sensor_id": 10, "sensor_type": "DO", "name": "Valve"}
            ]
        }
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.revision, 3);
    QCOMPARE(parsed.sensors.size(), 1);
    QCOMPARE(parsed.sensors[0].edgeSensorId, 10);
    QCOMPARE(parsed.sensors[0].sensorType, QStringLiteral("DO"));
}

void TestRestConfigParser::skipsSensorsMissingId()
{
    const QByteArray body = R"({"revision":1, "sensors":[ {"name":"no id"}, {"sensor_id":5} ]})";
    const auto parsed = RestConfigParser::parseConfigResponse(7, body);
    QCOMPARE(parsed.sensors.size(), 1);
    QCOMPARE(parsed.sensors[0].edgeSensorId, 5);
    QCOMPARE(parsed.sensors[0].sensorType, QStringLiteral("UNKNOWN"));
}

void TestRestConfigParser::invalidJsonReturnsValidFalse()
{
    const QByteArray body = "not json";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(!parsed.valid);
    QCOMPARE(parsed.sensors.size(), 0);
}

void TestRestConfigParser::parsesApplyResponse()
{
    const QByteArray body = R"({"ok": true, "applied_revision": 8})";
    const auto result = RestConfigParser::parseApplyResponse(body);
    QVERIFY(result.ok);
    QCOMPARE(result.appliedRevision, 8);
}

void TestRestConfigParser::formatRestErrorMaps401TokenEmpty()
{
    const QString msg = RestConfigParser::formatRestError(
        401, R"({"detail":"REST not configured"})");
    QCOMPARE(msg, QStringLiteral("Device REST token empty \u2014 Scan QR on logger"));
}

void TestRestConfigParser::formatRestErrorMaps401TokenMismatch()
{
    const QString msg = RestConfigParser::formatRestError(
        401, R"({"detail":"invalid bearer"})");
    QCOMPARE(msg, QStringLiteral("Token mismatch \u2014 Scan QR again on device"));
}

void TestRestConfigParser::formatRestErrorMaps409Revision()
{
    const QString msg = RestConfigParser::formatRestError(409, "{}");
    QCOMPARE(msg, QStringLiteral("Configuration changed on device. Connect again, then save."));
}

void TestRestConfigParser::formatRestErrorMaps422Revision()
{
    const QString msg = RestConfigParser::formatRestError(
        422, R"({"detail":"revision mismatch"})");
    QCOMPARE(msg, QStringLiteral("Configuration changed on device. Connect again, then save."));
}

void TestRestConfigParser::formatRestErrorMaps422MissingFieldsNotRevision()
{
    const QByteArray body = R"({
        "detail": [
            {"type": "missing", "loc": ["body", "api_version"], "msg": "Field required",
             "input": {"config": {"station_code": "TRAM-1"}, "expected_revision": 1}},
            {"type": "missing", "loc": ["body", "request_id"], "msg": "Field required",
             "input": {"config": {"station_code": "TRAM-1"}, "expected_revision": 1}}
        ]
    })";
    const QString msg = RestConfigParser::formatRestError(422, body);
    QVERIFY(!msg.contains(QStringLiteral("Configuration changed on device")));
    QCOMPARE(msg, QStringLiteral(
        "Device rejected config request (missing fields). Update Central Logger."));
}

void TestRestConfigParser::formatRestErrorMaps422MissingApiVersion()
{
    const QByteArray body = R"({
        "detail": [{"type": "missing", "loc": ["body", "api_version"], "msg": "Field required"}]
    })";
    const QString msg = RestConfigParser::formatRestError(422, body);
    QVERIFY(msg.contains(QStringLiteral("missing fields")));
}

void TestRestConfigParser::formatRestErrorMaps404()
{
    const QString msg = RestConfigParser::formatRestError(404, "");
    QVERIFY(msg.contains(QStringLiteral("firmware")));
}

void TestRestConfigParser::formatRestErrorTransportFallback()
{
    const QString msg = RestConfigParser::formatRestError(
        0, QByteArray{}, QStringLiteral("Connection refused"));
    QVERIFY(msg.contains(QStringLiteral("Connection refused")));
}

void TestRestConfigParser::prettyJsonRoundTrip()
{
    const QString pretty = RestConfigParser::prettyJson(R"({"a":1,"b":[2,3]})");
    QVERIFY(pretty.contains("\"a\": 1"));
    QVERIFY(pretty.contains("\n"));
    // Non-JSON body falls back to UTF-8 text.
    QCOMPARE(RestConfigParser::prettyJson("plain text"), QStringLiteral("plain text"));
}

QTEST_MAIN(TestRestConfigParser)
#include "test_rest_config_parser.moc"
