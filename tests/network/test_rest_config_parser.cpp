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
    void parsesDecimalsWithDefaultAndClamp();
    void parsesSensorsNestedUnderConfig();
    void parsesModbusTcpUnitIdFromConfig();
    void skipsSensorsMissingId();
    void invalidJsonReturnsValidFalse();
    void formatRestErrorMaps401TokenEmpty();
    void formatRestErrorMaps401TokenMismatch();
    void formatRestErrorMaps404();
    void formatRestErrorPreservesServerStatus();
    void formatRestErrorTransportFallback();
    void prettyJsonRoundTrip();
    void parsesNullVsOmittedWithoutUsingWriteCapability();
    void parsesGetRevisionFallback();
    void formatRestErrorIncludesServerDetail();
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
    QCOMPARE(parsed.stationCode, QStringLiteral("STN01"));
    QCOMPARE(parsed.stationName, QStringLiteral("Hanoi"));
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
            {"id": 11, "sensor_type": "DI", "parent_id": 10, "di_type": "02", "name": "FaultDI", "register_address": 3},
            {"id": 12, "sensor_type": "DI", "parent_id": 10, "di_type": "03", "name": "MaintDI"}
        ]
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.sensors.size(), 3);
    QVERIFY(!parsed.sensors[1].parentEdgeSensorId.has_value()
            || parsed.sensors[1].parentEdgeSensorId.value() == 10);
    QCOMPARE(parsed.sensors[1].diType, QStringLiteral("02"));
    QCOMPARE(parsed.sensors[1].edgeSensorId, 3); // register_address should override id for DI/DO
    QCOMPARE(parsed.sensors[2].edgeSensorId, 12); // fallback to id when register_address is absent
}

void TestRestConfigParser::parsesDecimalsWithDefaultAndClamp()
{
    const QByteArray body = R"({
        "revision": 1,
        "sensors": [
            {"sensor_id": 1, "sensor_type": "ANALOG", "name": "A", "decimals": 2},
            {"sensor_id": 2, "sensor_type": "ANALOG", "name": "B"},
            {"sensor_id": 3, "sensor_type": "ANALOG", "name": "C", "decimals": 9},
            {"sensor_id": 4, "sensor_type": "ANALOG", "name": "D", "decimals": -1}
        ]
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, body);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.sensors.size(), 4);
    QCOMPARE(parsed.sensors[0].decimals, 2); // explicit
    QCOMPARE(parsed.sensors[1].decimals, 4); // default
    QCOMPARE(parsed.sensors[2].decimals, 6); // clamped high
    QCOMPARE(parsed.sensors[3].decimals, 0); // clamped low
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

void TestRestConfigParser::formatRestErrorMaps404()
{
    const QString msg = RestConfigParser::formatRestError(404, "");
    QVERIFY(msg.contains(QStringLiteral("firmware")));
}

void TestRestConfigParser::formatRestErrorPreservesServerStatus()
{
    const QString msg = RestConfigParser::formatRestError(500, QByteArray{});
    QVERIFY(msg.contains(QStringLiteral("500")));
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

void TestRestConfigParser::parsesNullVsOmittedWithoutUsingWriteCapability()
{
    // New data-logger firmware: thresholds_writable=true, explicit null vs number, flat config keys
    const QByteArray writableBody = R"({
        "revision": 11,
        "thresholds_writable": true,
        "station_code": "DL-01",
        "poll_interval": 5,
        "sensors": [
            {"sensor_id": 1, "sensor_type": "ANALOG", "name": "Temp", "min_threshold": -10.5, "max_threshold": null},
            {"sensor_id": 2, "sensor_type": "ANALOG", "name": "OmittedLimits"}
        ]
    })";
    const auto parsed = RestConfigParser::parseConfigResponse(1, writableBody);
    QVERIFY(parsed.valid);
    QCOMPARE(parsed.revision, 11);
    QCOMPARE(parsed.stationCode, QStringLiteral("DL-01"));
    QCOMPARE(parsed.sensors.size(), 2);

    QVERIFY(parsed.sensors[0].minThresholdSpecified);
    QVERIFY(parsed.sensors[0].maxThresholdSpecified);
    QCOMPARE(parsed.sensors[0].minThreshold, std::optional<double>(-10.5));
    QVERIFY(!parsed.sensors[0].maxThreshold.has_value());

    QVERIFY(!parsed.sensors[1].minThresholdSpecified);
    QVERIFY(!parsed.sensors[1].maxThresholdSpecified);
    QVERIFY(!parsed.sensors[1].minThreshold.has_value());
    QVERIFY(!parsed.sensors[1].maxThreshold.has_value());

    // Legacy firmware / read-only adapter: no capability or thresholds.
    const QByteArray legacyBody = R"({"revision": 2, "sensors": [{"sensor_id": 1, "sensor_type": "ANALOG"}]})";
    const auto legacy = RestConfigParser::parseConfigResponse(1, legacyBody);
    QVERIFY(legacy.valid);
    QCOMPARE(legacy.sensors.size(), 1);
    QVERIFY(!legacy.sensors[0].minThresholdSpecified);
}

void TestRestConfigParser::parsesGetRevisionFallback()
{
    const auto fallback = RestConfigParser::parseConfigResponse(1, R"({"applied_revision":9,"sensors":[]})");
    QVERIFY(fallback.valid);
    QCOMPARE(fallback.revision, 9);
    QCOMPARE(RestConfigParser::parseConfigResponse(1, R"({"revision":1.2})").revision, -1);
}

void TestRestConfigParser::formatRestErrorIncludesServerDetail()
{
    QCOMPARE(RestConfigParser::formatRestError(503, R"({"detail":"Adapter not ready"})"),
             QStringLiteral("HTTP 503: Adapter not ready"));
}

QTEST_MAIN(TestRestConfigParser)
#include "test_rest_config_parser.moc"
