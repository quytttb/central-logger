#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "network/modbus/ModbusBridge.h"
#include "network/modbus/ModbusTypes.h"

#include <QSqlDatabase>
#include <QtTest>

using CentralLogger::Data::Database;
using CentralLogger::Data::LoggerRepository;
using CentralLogger::Data::SensorCatalogRepository;
using CentralLogger::Data::SensorReadingRepository;
using CentralLogger::Network::AnalogSample;
using CentralLogger::Network::ModbusBridge;
using CentralLogger::Network::PollSnapshot;

class TestModbusBridgeBatch : public QObject
{
    Q_OBJECT

private slots:
    void applyBatch_insertsMultipleSnapshotsInOneTransaction();
};

void TestModbusBridgeBatch::applyBatch_insertsMultipleSnapshotsInOneTransaction()
{
    Database db;
    QString err;
    QVERIFY(db.open(QStringLiteral("test_bridge_batch"), Database::memoryPath(), &err));

    LoggerRepository loggers(db.connection());
    CentralLogger::Data::LoggerInfo info;
    info.stationCode = QStringLiteral("ST01");
    info.name        = QStringLiteral("Station 1");
    info.host        = QStringLiteral("192.168.1.1");
    QVERIFY(loggers.insert(info, &err));
    const qint64 loggerId = info.id;

    ModbusBridge bridge;
    bridge.setDatabase(&db);

    PollSnapshot snap1;
    snap1.loggerId   = loggerId;
    snap1.success    = true;
    snap1.measuredAt = QDateTime::currentDateTimeUtc();
    snap1.header.na  = 1;
    AnalogSample a1;
    a1.edgeSensorId = 1;
    a1.value        = 12.5f;
    a1.flags        = 0x01;
    snap1.analogs.append(a1);

    PollSnapshot snap2 = snap1;
    snap2.measuredAt = snap1.measuredAt.addSecs(1);
    snap2.analogs[0].value = 13.0f;

    bridge.applyBatch({ snap1, snap2 });

    SensorCatalogRepository catalog(db.connection());
    const auto sensors = catalog.listByLoggerId(loggerId);
    QCOMPARE(sensors.size(), 1);

    SensorReadingRepository readings(db.connection());
    QCOMPARE(readings.countForSensor(sensors.first().id), 2);
}

QTEST_MAIN(TestModbusBridgeBatch)
#include "test_modbus_bridge_batch.moc"
