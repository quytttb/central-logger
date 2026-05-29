#include "core/sensors/SensorMerger.h"

#include <QDateTime>
#include <QTest>
#include <QVector>

using CentralLogger::Core::SensorLiveRow;
using CentralLogger::Core::SensorMerger;
using CentralLogger::Data::LoggerSensor;
using CentralLogger::Network::AnalogSample;
using CentralLogger::Network::PollSnapshot;

namespace {

constexpr quint16 kFlagValid = 0x01;
constexpr quint16 kFlagAlarm = 0x02;
constexpr quint16 kFlagStale = 0x04;

LoggerSensor makeCatalog(qint64 loggerId,
                         int edgeSensorId,
                         const char *type,
                         const QString &name = {},
                         const QString &unit = {},
                         std::optional<double> minT = std::nullopt,
                         std::optional<double> maxT = std::nullopt,
                         bool active = true)
{
    LoggerSensor s;
    s.id = edgeSensorId + 100;
    s.loggerId = loggerId;
    s.edgeSensorId = edgeSensorId;
    s.sensorType = QString::fromLatin1(type);
    s.name = name;
    s.unit = unit;
    s.minThreshold = minT;
    s.maxThreshold = maxT;
    s.active = active;
    return s;
}

PollSnapshot makeSnapshot(qint64 loggerId, quint16 statusFlags = 0x02)
{
    PollSnapshot snap;
    snap.loggerId   = loggerId;
    snap.success    = true;
    snap.header.mapVersion  = 1;
    snap.header.statusFlags = statusFlags; // bit1 = RTU connected by default
    snap.measuredAt = QDateTime::fromString(QStringLiteral("2026-05-24T03:14:15Z"),
                                            Qt::ISODate);
    snap.measuredAt.setTimeZone(QTimeZone::utc());
    return snap;
}

LoggerSensor makeChildDi(qint64 loggerId,
                         int edgeSensorId,
                         int parentEdgeId,
                         const QString &diType,
                         const QString &name = QStringLiteral("ChildDI"))
{
    LoggerSensor s;
    s.loggerId            = loggerId;
    s.edgeSensorId        = edgeSensorId;
    s.sensorType          = QStringLiteral("DI");
    s.name                = name;
    s.parentEdgeSensorId  = parentEdgeId;
    s.diType              = diType;
    s.active              = true;
    return s;
}

AnalogSample makeAnalog(int edgeSensorId, float value, quint16 flags = kFlagValid)
{
    AnalogSample s;
    s.edgeSensorId = static_cast<quint16>(edgeSensorId);
    s.flags        = flags;
    s.value        = value;
    return s;
}

const SensorLiveRow *findRow(const QVector<SensorLiveRow> &rows, int edgeSensorId)
{
    for (const auto &r : rows) {
        if (r.edgeSensorId == edgeSensorId) return &r;
    }
    return nullptr;
}

} // namespace

class TestSensorMerger : public QObject
{
    Q_OBJECT

private slots:
    void mergeAnalogDiCatalogMatch();
    void snapshotOnlyWithoutCatalog();
    void catalogOnlyYieldsWaitRow();
    void displayStatusStaleAndThreshold();
    void failedSnapshotEmpty();
    void doBitMappingFromCatalog();
    void inactiveCatalogYieldsWaitStatus();
    void wrongLoggerIdReturnsEmpty();
    void alarmFlagWithoutThresholdYieldsGenericAlarm();
    void rtuDisconnectedYieldsErr();
    void hr1ZeroDoesNotForceErrWhenValid();
    void attachDiErrorCodeShowsLabel();
    void attachDiPriority02Over03();
    void attachDiCustomCode();
};

void TestSensorMerger::mergeAnalogDiCatalogMatch()
{
    constexpr qint64 loggerId = 7;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 1, "ANALOG", QStringLiteral("Temp1"), QStringLiteral("C")),
        makeCatalog(loggerId, 2, "ANALOG", QStringLiteral("Hum"),   QStringLiteral("%")),
        makeCatalog(loggerId, 3, "DI",     QStringLiteral("Door")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = {
        makeAnalog(1, 25.5f),
        makeAnalog(2, 60.0f),
    };
    snap.diBits = { false, false, false, true };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 3);

    const auto *temp = findRow(rows, 1);
    QVERIFY(temp);
    QCOMPARE(temp->name, QStringLiteral("Temp1"));
    QCOMPARE(temp->sensorType, QStringLiteral("ANALOG"));
    QCOMPARE(temp->unit, QStringLiteral("C"));
    QCOMPARE(temp->value, QStringLiteral("25.50"));
    QCOMPARE(temp->displayStatus, QStringLiteral("OK"));
    QVERIFY(temp->valid);

    const auto *door = findRow(rows, 3);
    QVERIFY(door);
    QCOMPARE(door->sensorType, QStringLiteral("DI"));
    QCOMPARE(door->value, QStringLiteral("ON"));
    QCOMPARE(door->displayStatus, QStringLiteral("OK"));
    QVERIFY(door->unit.isEmpty());
    QCOMPARE(door->timestamp, QStringLiteral("03:14:15"));

    // Sorted ascending by edgeSensorId.
    QCOMPARE(rows[0].edgeSensorId, 1);
    QCOMPARE(rows[1].edgeSensorId, 2);
    QCOMPARE(rows[2].edgeSensorId, 3);
}

void TestSensorMerger::snapshotOnlyWithoutCatalog()
{
    constexpr qint64 loggerId = 1;
    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(42, 12.5f) };

    const auto rows = SensorMerger::buildRows(loggerId, snap, {});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].edgeSensorId, 42);
    QCOMPARE(rows[0].sensorType, QStringLiteral("UNKNOWN"));
    QCOMPARE(rows[0].name, QStringLiteral("Sensor #42"));
    QCOMPARE(rows[0].value, QStringLiteral("12.50"));
    QCOMPARE(rows[0].displayStatus, QStringLiteral("OK"));
}

void TestSensorMerger::catalogOnlyYieldsWaitRow()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 1, "ANALOG", QStringLiteral("Lonely")),
    };

    auto snap = makeSnapshot(loggerId);

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].displayStatus, QStringLiteral("WAIT"));
    QCOMPARE(rows[0].value, QStringLiteral("—"));
}

void TestSensorMerger::displayStatusStaleAndThreshold()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 1, "ANALOG", QStringLiteral("Stale"), {},
                    std::nullopt, std::nullopt),
        makeCatalog(loggerId, 2, "ANALOG", QStringLiteral("Min"), QStringLiteral("C"),
                    /*minT*/ 10.0, /*maxT*/ 50.0),
        makeCatalog(loggerId, 3, "ANALOG", QStringLiteral("Max"), QStringLiteral("C"),
                    /*minT*/ 0.0, /*maxT*/ 50.0),
        makeCatalog(loggerId, 4, "ANALOG", QStringLiteral("Bad")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = {
        makeAnalog(1, 25.0f, kFlagValid | kFlagStale),
        makeAnalog(2, 5.0f),                     // below min → ALARM_MIN
        makeAnalog(3, 99.0f),                    // above max → ALARM_MAX
        makeAnalog(4, 0.0f, /*flags*/ 0),        // !valid → ERR
    };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 4);

    QCOMPARE(findRow(rows, 1)->displayStatus, QStringLiteral("STALE"));
    QCOMPARE(findRow(rows, 2)->displayStatus, QStringLiteral("ALARM"));
    QCOMPARE(findRow(rows, 2)->alarmType, QStringLiteral("min"));
    QCOMPARE(findRow(rows, 3)->displayStatus, QStringLiteral("ALARM"));
    QCOMPARE(findRow(rows, 3)->alarmType, QStringLiteral("max"));
    QCOMPARE(findRow(rows, 4)->displayStatus, QStringLiteral("ERR"));
}

void TestSensorMerger::failedSnapshotEmpty()
{
    constexpr qint64 loggerId = 1;
    auto snap = makeSnapshot(loggerId);
    snap.success = false;
    snap.analogs = { makeAnalog(1, 1.0f) };

    const auto rows = SensorMerger::buildRows(loggerId, snap, {});
    QVERIFY(rows.isEmpty());
}

void TestSensorMerger::doBitMappingFromCatalog()
{
    constexpr qint64 loggerId = 2;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 0, "DO", QStringLiteral("Relay0")),
        makeCatalog(loggerId, 1, "DO", QStringLiteral("Relay1")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.doBits = { true, false };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(findRow(rows, 0)->value, QStringLiteral("ON"));
    QCOMPARE(findRow(rows, 1)->value, QStringLiteral("OFF"));
    QCOMPARE(findRow(rows, 0)->sensorType, QStringLiteral("DO"));
    QCOMPARE(findRow(rows, 0)->displayStatus, QStringLiteral("OK"));
}

void TestSensorMerger::inactiveCatalogYieldsWaitStatus()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 1, "ANALOG", QStringLiteral("Off"),
                    QStringLiteral("C"), std::nullopt, std::nullopt,
                    /*active*/ false),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(1, 25.0f) };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].displayStatus, QStringLiteral("WAIT"));
}

void TestSensorMerger::wrongLoggerIdReturnsEmpty()
{
    auto snap = makeSnapshot(/*loggerId*/ 1);
    snap.analogs = { makeAnalog(1, 1.0f) };

    const auto rows = SensorMerger::buildRows(/*loggerId*/ 2, snap, {});
    QVERIFY(rows.isEmpty());
}

void TestSensorMerger::alarmFlagWithoutThresholdYieldsGenericAlarm()
{
    // M-14: when the Modbus alarm bit is set but no threshold can determine
    // the direction, displayStatus must be the generic "ALARM" label instead
    // of incorrectly reporting "ALARM_MIN".
    constexpr qint64 loggerId = 5;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 1, "ANALOG", QStringLiteral("Pressure"),
                    QStringLiteral("kPa"), std::nullopt, std::nullopt),
        makeCatalog(loggerId, 2, "ANALOG", QStringLiteral("TempWithMax"),
                    QStringLiteral("C"),   std::nullopt, /*maxT*/ 80.0),
    };

    auto snap = makeSnapshot(loggerId);
    // sensor 1: alarm bit set, no thresholds → generic ALARM
    // sensor 2: alarm bit set, value < max threshold (no violation) → generic ALARM
    snap.analogs = {
        makeAnalog(1, 25.0f, kFlagValid | kFlagAlarm),
        makeAnalog(2, 50.0f, kFlagValid | kFlagAlarm),
    };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(findRow(rows, 1)->displayStatus, QStringLiteral("ALARM"));
    QCOMPARE(findRow(rows, 2)->displayStatus, QStringLiteral("ALARM"));

    // Threshold-direction checks still work when the value is clearly out of range.
    QVector<LoggerSensor> catalog2{
        makeCatalog(loggerId, 3, "ANALOG", QStringLiteral("Hot"),
                    QStringLiteral("C"), std::nullopt, /*maxT*/ 50.0),
    };
    snap.analogs = { makeAnalog(3, 99.0f, kFlagValid | kFlagAlarm) };
    const auto rows2 = SensorMerger::buildRows(loggerId, snap, catalog2);
    QCOMPARE(rows2.size(), 1);
    QCOMPARE(rows2[0].displayStatus, QStringLiteral("ALARM"));
    QCOMPARE(rows2[0].alarmType, QStringLiteral("max"));
}

void TestSensorMerger::rtuDisconnectedYieldsErr()
{
    constexpr qint64 loggerId = 1;
    // bit0=polling, bit1=RTU off — edge connection_lost while polling
    auto snap = makeSnapshot(loggerId, /*statusFlags*/ 0x01);
    snap.analogs = { makeAnalog(1, 25.0f) };

    const auto rows = SensorMerger::buildRows(loggerId, snap, {});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].displayStatus, QStringLiteral("ERR"));
}

void TestSensorMerger::hr1ZeroDoesNotForceErrWhenValid()
{
    constexpr qint64 loggerId = 1;
    auto snap = makeSnapshot(loggerId, /*statusFlags*/ 0x00);
    snap.analogs = { makeAnalog(1, 25.0f) };

    const auto rows = SensorMerger::buildRows(loggerId, snap, {});
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].displayStatus, QStringLiteral("OK"));
}

void TestSensorMerger::attachDiErrorCodeShowsLabel()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 10, "ANALOG", QStringLiteral("Tank")),
        makeChildDi(loggerId, 2, 10, QStringLiteral("02")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(10, 50.0f) };
    snap.diBits  = { false, false, true };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    const auto *tank = findRow(rows, 10);
    QVERIFY(tank);
    QCOMPARE(tank->attachDiTypeCodes, QStringList{QStringLiteral("02")});
    QCOMPARE(tank->attachDiTypeLabels, QStringList{QStringLiteral("Error")});
    QCOMPARE(tank->displayStatus, QStringLiteral("OK"));
}

void TestSensorMerger::attachDiPriority02Over03()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 10, "ANALOG"),
        makeChildDi(loggerId, 1, 10, QStringLiteral("03")),
        makeChildDi(loggerId, 2, 10, QStringLiteral("02")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(10, 1.0f) };
    snap.diBits  = { false, true, true };

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    const auto *tank = findRow(rows, 10);
    QVERIFY(tank);
    QCOMPARE(tank->attachDiTypeCodes,
             QStringList({QStringLiteral("02"), QStringLiteral("03")}));
    QCOMPARE(tank->attachDiTypeLabels,
             QStringList({QStringLiteral("Error"), QStringLiteral("Maintenance")}));
    QCOMPARE(tank->displayStatus, QStringLiteral("OK"));
}

void TestSensorMerger::attachDiCustomCode()
{
    constexpr qint64 loggerId = 1;
    QVector<LoggerSensor> catalog{
        makeCatalog(loggerId, 10, "ANALOG"),
        makeChildDi(loggerId, 5, 10, QStringLiteral("05"),
                    QStringLiteral("Leaking seal")),
    };

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(10, 1.0f) };
    snap.diBits.resize(6);
    snap.diBits[5] = true;

    const auto rows = SensorMerger::buildRows(loggerId, snap, catalog);
    const auto *tank = findRow(rows, 10);
    QVERIFY(tank);
    QCOMPARE(tank->attachDiTypeCodes, QStringList{QStringLiteral("05")});
    QCOMPARE(tank->attachDiTypeLabels, QStringList{QStringLiteral("Leaking seal")});
    QCOMPARE(tank->displayStatus, QStringLiteral("OK"));
}

QTEST_APPLESS_MAIN(TestSensorMerger)
#include "test_sensor_merger.moc"
