#include "../fixtures/ReadOnlyEdgeHttpServer.h"
#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/modbus/ModbusService.h"
#include "network/rest/RestConfigService.h"

#include <QModbusTcpServer>
#include <QSignalSpy>
#include <QTest>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;
using namespace CentralLogger::Network;

namespace {

const QString kHost = QStringLiteral("127.0.0.1");
const QString kToken = QStringLiteral("test-token");
const QByteArray kGetConfig = "GET /api/v1/config HTTP/1.1";

struct Harness
{
    Database db;
    DashboardController dashboard{nullptr};
    RestConfigService rest;
    LoggerFormController form{nullptr};
    ReadOnlyEdgeHttpServer edge;

    Harness()
    {
        if (!db.open(QStringLiteral("logger_form_save"), QStringLiteral(":memory:")))
            qFatal("Could not open test database");
        dashboard.setDatabase(&db);
        rest.setDatabase(&db);
        form.setDatabase(&db);
        form.setRestConfigService(&rest);
        form.setDashboardController(&dashboard);
    }

    void probe()
    {
        form.probeConfig(kHost, edge.port(), kToken);
    }

    void add(int modbusPort = 5020, int unitId = 1, const QString &token = kToken)
    {
        form.saveLoggerFromForm(true, -1, {}, QStringLiteral("Central alias"),
                                kHost, modbusPort, edge.port(), token, unitId, 60,
                                2, true, QStringLiteral("Local note"));
    }
};

class RecordingModbusServer : public QModbusTcpServer
{
public:
    bool start(bool native, int unitId)
    {
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost))
            return false;
        port = reservation.serverPort();
        reservation.close();

        setConnectionParameter(QModbusDevice::NetworkAddressParameter, kHost);
        setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        setServerAddress(unitId);

        using Unit = QModbusDataUnit;
        const quint16 digital = native ? 1 : 0;
        const QList<quint16> registers{
            2, 1, 0, 0, 1, digital, digital, 0, quint16(native ? 0 : 1), 0,
            quint16(native ? 1 : 164), 1, 0x40e0, 0, 0, 0, 0, 0, // valid, value 7.0, status 00
        };
        QModbusDataUnitMap map{
            {Unit::HoldingRegisters, Unit(Unit::HoldingRegisters, 0, registers)},
        };
        if (native) {
            map.insert(Unit::DiscreteInputs, Unit(Unit::DiscreteInputs, 0, QList<quint16>{1}));
            map.insert(Unit::Coils, Unit(Unit::Coils, 0, QList<quint16>{1}));
        }
        return setMap(map) && connectDevice();
    }

    quint16 port = 0;
    QList<QModbusPdu::FunctionCode> requests;

protected:
    QModbusResponse processRequest(const QModbusPdu &request) override
    {
        requests.append(request.functionCode());
        return QModbusTcpServer::processRequest(request);
    }
};

} // namespace

class TestLoggerFormSave : public QObject
{
    Q_OBJECT

private slots:
    void addEditReloadUseOnlyReadRequests_data();
    void addEditReloadUseOnlyReadRequests();
    void addRequiresSnapshotFromCurrentConnection();
    void addValidationAndDuplicateStationCode();
    void editOfflineKeepsCatalogAndRevision();
    void reloadFailureKeepsMirrorAndDisabledState();
};

void TestLoggerFormSave::addEditReloadUseOnlyReadRequests_data()
{
    QTest::addColumn<bool>("native");
    QTest::newRow("native-advertises-write") << true;
    QTest::newRow("osg-read-only-adapter") << false;
}

void TestLoggerFormSave::addEditReloadUseOnlyReadRequests()
{
    QFETCH(bool, native);
    RecordingModbusServer modbus;
    ModbusService service;
    Harness h;
    QVERIFY(h.edge.port() != 0);
    const int unitId = native ? 3 : 1;
    const int sensorId = native ? 1 : 164;
    QVERIFY(modbus.start(native, unitId));
    h.dashboard.setModbusService(&service);
    if (!native) {
        // OSG's virtual station has no mapped thresholds or digital channels.
        h.edge.respondWith(R"({"revision":5,
            "config":{"station_code":"OSG-01","station_name":"OSG station",
                      "poll_interval":10,"modbus_tcp_unit_id":1,
                      "sensors":[{"sensor_id":164,"sensor_type":"ANALOG","name":"O2","unit":"%V"}]}})");
    }

    QSignalSpy probe(&h.form, &LoggerFormController::probeConfigResult);
    h.probe();
    QTRY_COMPARE(probe.size(), 1);
    QVERIFY(probe.takeFirst().at(0).toBool());
    QCOMPARE(h.form.probedModbusUnitId(), unitId);

    QSignalSpy polls(&service, &ModbusService::pollFinished);
    QSignalSpy saved(&h.form, &LoggerFormController::formSaveFinished);
    h.add(modbus.port, unitId); // Name and cadence deliberately differ from the edge.
    QCOMPARE(saved.size(), 1);
    const auto addResult = saved.takeFirst();
    QVERIFY2(addResult.at(0).toBool(), qPrintable(addResult.at(2).toString()));
    const qint64 id = addResult.at(1).toLongLong();
    QVERIFY(id > 0);
    QTRY_COMPARE(polls.size(), 1);
    const auto snapshot = polls.first().first().value<PollSnapshot>();
    QVERIFY2(snapshot.success, qPrintable(snapshot.errorMessage));
    QCOMPARE(snapshot.analogs.size(), 1);
    QCOMPARE(snapshot.analogs.first().edgeSensorId, sensorId);
    QCOMPARE(snapshot.diBits.size(), native ? 1 : 0);
    QCOMPARE(snapshot.doBits.size(), native ? 1 : 0);
    QList<QModbusPdu::FunctionCode> cycle{QModbusPdu::ReadHoldingRegisters, QModbusPdu::ReadHoldingRegisters};
    if (native) {
        cycle.append(QModbusPdu::ReadDiscreteInputs);
        cycle.append(QModbusPdu::ReadCoils);
    }
    QCOMPARE(modbus.requests, cycle);
    QCOMPARE(h.edge.requests, QList<QByteArray>{kGetConfig});

    LoggerRepository repo(h.db.connection());
    SensorCatalogRepository catalog(h.db.connection());
    const auto added = repo.findById(id);
    QVERIFY(added);
    QCOMPARE(added->name, QStringLiteral("Central alias"));
    QCOMPARE(added->centralPollIntervalS, 60);
    QCOMPARE(added->lastRevision, 5);
    const auto first = catalog.findByLoggerAndEdgeId(id, sensorId, QStringLiteral("ANALOG"));
    QVERIFY(first);
    QCOMPARE(first->minThreshold, native ? std::optional<double>(6.0) : std::nullopt);
    QCOMPARE(first->maxThreshold, native ? std::optional<double>(9.0) : std::nullopt);

    h.form.saveLoggerFromForm(false, id, QStringLiteral("ignored"), QStringLiteral("Local rename"),
                             kHost, modbus.port, h.edge.port(), kToken, unitId, 61, 3, true,
                             QStringLiteral("Edited note"));
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.takeFirst().at(0).toBool());
    const auto edited = repo.findById(id);
    QVERIFY(edited);
    QCOMPARE(edited->name, QStringLiteral("Local rename"));
    QCOMPARE(edited->centralPollIntervalS, 61);
    QCOMPARE(edited->stationCode, added->stationCode);
    QCOMPARE(edited->lastRevision, added->lastRevision);

    if (native) {
        h.edge.respondWith(R"({"revision":6,
            "config":{"thresholds_writable":true,"station_name":"Edge changed",
                      "poll_interval":17,"modbus_tcp_unit_id":9},
            "sensors":[{"sensor_id":1,"sensor_type":"ANALOG","name":"Updated","max_threshold":null}]})");
    } else {
        h.edge.respondWith(R"({"revision":6,
            "config":{"station_name":"OSG changed","poll_interval":12,"modbus_tcp_unit_id":9,
                      "sensors":[{"sensor_id":164,"sensor_type":"ANALOG","name":"Updated","unit":"%V"}]}})");
    }

    QSignalSpy reloaded(&h.form, &LoggerFormController::configReloadFinished);
    h.form.reloadLoggerConfig(id);
    QCOMPARE(h.form.reloadingLoggerId(), id);
    h.form.reloadLoggerConfig(id);
    QTRY_COMPARE(reloaded.size(), 1);
    QCOMPARE(h.form.reloadingLoggerId(), 0);
    const auto reloadResult = reloaded.takeFirst();
    QCOMPARE(reloadResult.at(0).toLongLong(), id);
    QVERIFY2(reloadResult.at(1).toBool(), qPrintable(reloadResult.at(2).toString()));
    QTRY_COMPARE(polls.size(), 2);
    QVERIFY(polls.last().first().value<PollSnapshot>().success);
    QCOMPARE(modbus.requests, cycle + cycle);
    const QList<QByteArray> expectedHttp{kGetConfig, kGetConfig};
    QCOMPARE(h.edge.requests, expectedHttp);

    const auto fresh = repo.findById(id);
    QVERIFY(fresh);
    QCOMPARE(fresh->lastRevision, 6);
    QCOMPARE(fresh->name, edited->name);
    QCOMPARE(fresh->centralPollIntervalS, edited->centralPollIntervalS);
    QCOMPARE(fresh->modbusUnitId, edited->modbusUnitId);
    QCOMPARE(fresh->note, edited->note);
    const auto updated = catalog.findByLoggerAndEdgeId(id, sensorId, QStringLiteral("ANALOG"));
    QVERIFY(updated);
    QCOMPARE(updated->id, first->id);
    QCOMPARE(updated->name, QStringLiteral("Updated"));
    QCOMPARE(updated->minThreshold, first->minThreshold); // Omitted: preserve the existing mirror.
    QVERIFY(!updated->maxThreshold.has_value()); // Native: explicit null, OSG: never supplied.
}

void TestLoggerFormSave::addRequiresSnapshotFromCurrentConnection()
{
    Harness h;
    LoggerRepository repo(h.db.connection());
    QSignalSpy saved(&h.form, &LoggerFormController::formSaveFinished);
    h.add();
    QCOMPARE(saved.size(), 1);
    QVERIFY(!saved.takeFirst().at(0).toBool());
    QCOMPARE(repo.countTotal(), 0);

    h.probe();
    QTRY_VERIFY(h.form.hasProbedConfig());
    h.add(5020, 1, QStringLiteral("changed-token"));
    QCOMPARE(saved.size(), 1);
    QVERIFY(!saved.takeFirst().at(0).toBool());
    QCOMPARE(repo.countTotal(), 0);
    QCOMPARE(h.edge.requests, QList<QByteArray>{kGetConfig});
}

void TestLoggerFormSave::addValidationAndDuplicateStationCode()
{
    Harness h;
    h.probe();
    QTRY_VERIFY(h.form.hasProbedConfig());
    QSignalSpy saved(&h.form, &LoggerFormController::formSaveFinished);
    h.add(0); // Invalid Modbus port.
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.takeFirst().at(2).toString().contains(QStringLiteral("Modbus port")));

    h.add();
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.takeFirst().at(0).toBool());
    QVERIFY(!h.form.hasProbedConfig());
    h.probe();
    QTRY_VERIFY(h.form.hasProbedConfig());
    h.add(); // Same station code, without replacing the existing row.
    QCOMPARE(saved.size(), 1);
    const auto duplicate = saved.takeFirst();
    QVERIFY(!duplicate.at(0).toBool());
    QVERIFY(duplicate.at(2).toString().contains(h.form.probedStationCode()));
    QCOMPARE(LoggerRepository(h.db.connection()).countTotal(), 1);
}

void TestLoggerFormSave::editOfflineKeepsCatalogAndRevision()
{
    Harness h;
    h.probe();
    QTRY_VERIFY(h.form.hasProbedConfig());
    QSignalSpy saved(&h.form, &LoggerFormController::formSaveFinished);
    h.add();
    QCOMPARE(saved.size(), 1);
    const qint64 id = saved.takeFirst().at(1).toLongLong();
    QVERIFY(id > 0);

    h.form.setRestConfigService(nullptr);
    h.form.saveLoggerFromForm(false, id, QStringLiteral("ignored"), QStringLiteral("Offline alias"),
                             QStringLiteral("127.0.0.2"), 15020, 18080, QStringLiteral("new-token"),
                             2, 8, 4, false, QStringLiteral("Offline note"));
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.takeFirst().at(0).toBool());
    const auto stored = LoggerRepository(h.db.connection()).findById(id);
    QVERIFY(stored);
    QCOMPARE(stored->stationCode, QStringLiteral("ST-01"));
    QCOMPARE(stored->name, QStringLiteral("Offline alias"));
    QCOMPARE(stored->host, QStringLiteral("127.0.0.2"));
    QCOMPARE(stored->modbusPort, 15020);
    QCOMPARE(stored->modbusUnitId, 2);
    QCOMPARE(stored->apiPort, 18080);
    QCOMPARE(stored->apiToken, QStringLiteral("new-token"));
    QCOMPARE(stored->centralPollIntervalS, 8);
    QCOMPARE(stored->timeoutS, 4.0);
    QCOMPARE(stored->enabled, false);
    QCOMPARE(stored->note, QStringLiteral("Offline note"));
    QCOMPARE(stored->lastRevision, 5);
    const auto sensor = SensorCatalogRepository(h.db.connection()).listByLoggerId(id);
    QCOMPARE(sensor.size(), 1);
    QCOMPARE(sensor.first().maxThreshold, std::optional<double>(9.0));
}

void TestLoggerFormSave::reloadFailureKeepsMirrorAndDisabledState()
{
    ModbusService service;
    QSignalSpy polls(&service, &ModbusService::pollFinished);
    Harness h;
    h.dashboard.setModbusService(&service);
    LoggerRepository repo(h.db.connection());
    LoggerInfo logger;
    logger.stationCode = QStringLiteral("DISABLED");
    logger.name = QStringLiteral("Central name");
    logger.host = kHost;
    logger.apiPort = h.edge.port();
    logger.enabled = false;
    logger.lastRevision = 4;
    QVERIFY(repo.insert(logger));
    SensorCatalogRepository catalog(h.db.connection());
    LoggerSensor sensor;
    sensor.loggerId = logger.id;
    sensor.edgeSensorId = 1;
    sensor.sensorType = QStringLiteral("ANALOG");
    sensor.maxThreshold = 9.0;
    QVERIFY(catalog.upsert(sensor));

    QSignalSpy reloaded(&h.form, &LoggerFormController::configReloadFinished);
    h.edge.respondWith(R"({"detail":"Adapter not ready"})", "503 Service Unavailable");
    h.form.reloadLoggerConfig(logger.id);
    QTRY_COMPARE(reloaded.size(), 1);
    const auto failure = reloaded.takeFirst();
    QVERIFY(!failure.at(1).toBool());
    QVERIFY(failure.at(2).toString().contains(QStringLiteral("503")));

    h.edge.respondWith(R"({"sensors":[]})"); // Missing revision: invalid snapshot.
    h.form.reloadLoggerConfig(logger.id);
    QTRY_COMPARE(reloaded.size(), 1);
    QVERIFY(!reloaded.takeFirst().at(1).toBool());
    QCOMPARE(repo.findById(logger.id)->lastRevision, 4);
    QCOMPARE(catalog.listByLoggerId(logger.id).first().maxThreshold, std::optional<double>(9.0));

    h.edge.respondWith(R"({"revision":5,"sensors":[
        {"sensor_id":1,"sensor_type":"ANALOG","max_threshold":null}]})");
    h.form.reloadLoggerConfig(logger.id);
    QTRY_COMPARE(reloaded.size(), 1);
    QVERIFY(reloaded.takeFirst().at(1).toBool());
    QCOMPARE(repo.findById(logger.id)->lastRevision, 5);
    QVERIFY(!repo.findById(logger.id)->enabled);
    QVERIFY(!catalog.listByLoggerId(logger.id).first().maxThreshold.has_value());
    QCOMPARE(polls.size(), 0);
}

QTEST_GUILESS_MAIN(TestLoggerFormSave)
#include "test_logger_form_save.moc"
