#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/sensors/SensorMonitoringTableModel.h"
#include "core/sensors/SensorSnapshotCache.h"
#include "data/db/Database.h"
#include "data/repositories/SensorCatalogRepository.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QVector>

using CentralLogger::Core::DashboardController;
using CentralLogger::Core::LoggerFormController;
using CentralLogger::Core::SensorLiveRow;
using CentralLogger::Core::SensorMonitoringTableModel;
using CentralLogger::Core::SensorSnapshotCache;
using CentralLogger::Data::Database;
using CentralLogger::Data::LoggerSensor;
using CentralLogger::Data::SensorCatalogRepository;
using CentralLogger::Network::AnalogSample;
using CentralLogger::Network::PollSnapshot;

namespace {

QString uniqueConnectionName(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("sensor_table_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

/// CRUD moved to LoggerFormController; seed loggers through it while the
/// dashboard snapshot table under test stays in sync.
qint64 addSeedLogger(DashboardController &dash, Database &db,
                     const QString &code, const QString &name,
                     const QString &host, int modbusPort, int apiPort,
                     const QString &token)
{
    LoggerFormController form(nullptr);
    form.setDatabase(&db);
    form.setDashboardController(&dash);
    return form.addLogger(code, name, host, modbusPort, apiPort, token);
}

SensorLiveRow makeRow(int id,
                      const QString &name,
                      const QString &value,
                      const QString &unit,
                      const QString &status)
{
    SensorLiveRow r;
    r.edgeSensorId  = id;
    r.name          = name;
    r.sensorType    = QStringLiteral("ANALOG");
    r.unit          = unit;
    r.value         = value;
    r.displayStatus = status;
    r.timestamp     = QStringLiteral("12:00:00");
    return r;
}

PollSnapshot makeSnapshot(qint64 loggerId)
{
    PollSnapshot snap;
    snap.loggerId   = loggerId;
    snap.success    = true;
    snap.measuredAt = QDateTime::fromString(QStringLiteral("2026-05-24T03:14:15Z"),
                                            Qt::ISODate);
    snap.measuredAt.setTimeZone(QTimeZone::utc());
    return snap;
}

AnalogSample makeAnalog(int id, float value)
{
    AnalogSample s;
    s.edgeSensorId = static_cast<quint16>(id);
    s.flags        = 0x01; // valid
    s.value        = value;
    return s;
}

} // namespace

class TestSensorMonitoringTableModel : public QObject
{
    Q_OBJECT

private slots:
    void emptyByDefault();
    void columnAndRowCountReflectRows();
    void displayRoleReturnsColumnText();
    void namedRolesExposeFullRow();
    void setLoggerIdClearsRowsAndEmits();
    void cacheStoresAndReturnsRows();
    void cacheIgnoresFailedSnapshot();
    void dashboardOnSnapshotAppliedRefreshesTable();
};

void TestSensorMonitoringTableModel::emptyByDefault()
{
    SensorMonitoringTableModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), int(SensorMonitoringTableModel::ColumnCount));
    QCOMPARE(model.columnCount(), 5);
    QCOMPARE(model.loggerId(), qint64(-1));
}

void TestSensorMonitoringTableModel::columnAndRowCountReflectRows()
{
    SensorMonitoringTableModel model;
    model.setRows({
        makeRow(1, QStringLiteral("Temp"), QStringLiteral("25.50"),
                QStringLiteral("C"), QStringLiteral("OK")),
        makeRow(2, QStringLiteral("Hum"),  QStringLiteral("60.00"),
                QStringLiteral("%"), QStringLiteral("OK")),
        makeRow(3, QStringLiteral("Door"), QStringLiteral("ON"),
                QString(), QStringLiteral("OK")),
    });
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.columnCount(), 5);
}

void TestSensorMonitoringTableModel::displayRoleReturnsColumnText()
{
    SensorMonitoringTableModel model;
    model.setRows({
        makeRow(7, QStringLiteral("Temp"), QStringLiteral("25.50"),
                QStringLiteral("C"), QStringLiteral("OK")),
    });

    using M = SensorMonitoringTableModel;
    QCOMPARE(model.data(model.index(0, M::SensorIdColumn)).toInt(), 7);
    QCOMPARE(model.data(model.index(0, M::NameColumn)).toString(),
             QStringLiteral("Temp"));
    QCOMPARE(model.data(model.index(0, M::ValueColumn)).toString(),
             QStringLiteral("25.50"));
    QCOMPARE(model.data(model.index(0, M::UnitColumn)).toString(),
             QStringLiteral("C"));
    QCOMPARE(model.data(model.index(0, M::DisplayStatusColumn)).toString(),
             QStringLiteral("OK"));

    QCOMPARE(model.headerData(M::NameColumn, Qt::Horizontal).toString(),
             QStringLiteral("Name"));
}

void TestSensorMonitoringTableModel::namedRolesExposeFullRow()
{
    SensorMonitoringTableModel model;
    SensorLiveRow row = makeRow(11, QStringLiteral("Door"),
                                QStringLiteral("ON"), QString(),
                                QStringLiteral("OK"));
    row.sensorType = QStringLiteral("DI");
    row.valid = true;
    row.alarm = false;
    row.stale = false;
    model.setRows({ row });

    using M = SensorMonitoringTableModel;
    const auto idx = model.index(0, 0);
    QCOMPARE(model.data(idx, M::SensorIdRole).toInt(), 11);
    QCOMPARE(model.data(idx, M::NameRole).toString(), QStringLiteral("Door"));
    QCOMPARE(model.data(idx, M::ValueRole).toString(), QStringLiteral("ON"));
    QCOMPARE(model.data(idx, M::UnitRole).toString(), QString());
    QCOMPARE(model.data(idx, M::DisplayStatusRole).toString(), QStringLiteral("OK"));
    QCOMPARE(model.data(idx, M::SensorTypeRole).toString(), QStringLiteral("DI"));
    QCOMPARE(model.data(idx, M::ValidRole).toBool(), true);
    QCOMPARE(model.data(idx, M::AlarmRole).toBool(), false);
    QCOMPARE(model.data(idx, M::StaleRole).toBool(), false);
    QCOMPARE(model.data(idx, M::TimestampRole).toString(),
             QStringLiteral("12:00:00"));

    const auto roleNames = model.roleNames();
    QVERIFY(roleNames.value(M::SensorIdRole) == "sensorId");
    QVERIFY(roleNames.value(M::DisplayStatusRole) == "displayStatus");
}

void TestSensorMonitoringTableModel::setLoggerIdClearsRowsAndEmits()
{
    SensorMonitoringTableModel model;
    model.setRows({ makeRow(1, QStringLiteral("a"), QStringLiteral("1.00"),
                            QString(), QStringLiteral("OK")) });
    QCOMPARE(model.rowCount(), 1);

    QSignalSpy spy(&model, &SensorMonitoringTableModel::loggerIdChanged);
    model.setLoggerId(42);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.loggerId(), qint64(42));
    QCOMPARE(model.rowCount(), 0);

    model.setLoggerId(42);
    QCOMPARE(spy.count(), 1);
}

void TestSensorMonitoringTableModel::cacheStoresAndReturnsRows()
{
    SensorSnapshotCache cache;
    QVector<LoggerSensor> catalog;
    LoggerSensor s;
    s.loggerId     = 1;
    s.edgeSensorId = 5;
    s.sensorType   = QStringLiteral("ANALOG");
    s.name         = QStringLiteral("Temp");
    s.unit         = QStringLiteral("C");
    catalog.append(s);

    auto snap = makeSnapshot(1);
    snap.analogs = { makeAnalog(5, 19.5f) };

    cache.apply(snap, catalog);
    QVERIFY(cache.has(1));

    const auto rows = cache.rowsFor(1);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().edgeSensorId, 5);
    QCOMPARE(rows.first().name, QStringLiteral("Temp"));
    QCOMPARE(rows.first().value, QStringLiteral("19.5000"));

    cache.remove(1);
    QVERIFY(!cache.has(1));
    QVERIFY(cache.rowsFor(1).isEmpty());
}

void TestSensorMonitoringTableModel::cacheIgnoresFailedSnapshot()
{
    SensorSnapshotCache cache;

    PollSnapshot ok = makeSnapshot(2);
    ok.analogs = { makeAnalog(1, 10.0f) };
    cache.apply(ok, {});
    QCOMPARE(cache.rowsFor(2).size(), 1);

    PollSnapshot bad = makeSnapshot(2);
    bad.success = false;
    cache.apply(bad, {});
    QCOMPARE(cache.rowsFor(2).size(), 1);
}

void TestSensorMonitoringTableModel::dashboardOnSnapshotAppliedRefreshesTable()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("hook"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 loggerId = addSeedLogger(ctrl, db, QStringLiteral("TRAM-T7"),
                                           QStringLiteral("Test"),
                                           QStringLiteral("h"),
                                           5020, 8080, {});
    QVERIFY(loggerId > 0);

    SensorCatalogRepository catalog(db.connection());
    LoggerSensor s;
    s.loggerId     = loggerId;
    s.edgeSensorId = 1;
    s.sensorType   = QStringLiteral("ANALOG");
    s.name         = QStringLiteral("Temp");
    s.unit         = QStringLiteral("C");
    QVERIFY(catalog.upsert(s));

    auto *table = ctrl.sensorTable();
    QVERIFY(table);
    table->setLoggerId(loggerId);
    QCOMPARE(table->rowCount(), 0);

    QSignalSpy spy(&ctrl, &DashboardController::loggerSnapshotUpdated);

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(1, 22.75f) };
    ctrl.onSnapshotApplied(snap, /*sensorCount*/ 1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().value(0).toLongLong(), loggerId);

    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->data(table->index(0, SensorMonitoringTableModel::ValueColumn))
                 .toString(),
             QStringLiteral("22.7500"));

    // Switching to a different logger clears the rows even though the
    // cache still keeps the old logger's data.
    table->setLoggerId(loggerId + 999);
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(ctrl.sensorCache()->rowsFor(loggerId).size(), 1);
}

QTEST_MAIN(TestSensorMonitoringTableModel)
#include "test_sensor_monitoring_table_model.moc"
