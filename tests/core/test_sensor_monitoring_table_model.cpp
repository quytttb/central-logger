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
    s.statusCode   = QLatin1String("00"); // valid
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
    void dashboardOverviewSeparatesSameIdWithDifferentMetadata();
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
    row.statusCode = QLatin1String("00");
    model.setRows({ row });

    using M = SensorMonitoringTableModel;
    const auto idx = model.index(0, 0);
    QCOMPARE(model.data(idx, M::SensorIdRole).toInt(), 11);
    QCOMPARE(model.data(idx, M::NameRole).toString(), QStringLiteral("Door"));
    QCOMPARE(model.data(idx, M::ValueRole).toString(), QStringLiteral("ON"));
    QCOMPARE(model.data(idx, M::UnitRole).toString(), QString());
    QCOMPARE(model.data(idx, M::DisplayStatusRole).toString(), QStringLiteral("OK"));
    QCOMPARE(model.data(idx, M::SensorTypeRole).toString(), QStringLiteral("DI"));
    QCOMPARE(model.data(idx, M::StatusCodeRole).toString(), QStringLiteral("00"));
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
    ctrl.onSnapshotApplied(snap, /*sensorCount*/ 1,
                           catalog.listByLoggerId(loggerId));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().value(0).toLongLong(), loggerId);

    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->data(table->index(0, SensorMonitoringTableModel::ValueColumn))
                 .toString(),
             QStringLiteral("22.7500"));

    auto failed = snap;
    failed.success = false;
    ctrl.onSnapshotApplied(failed, 1, {});
    QCOMPARE(table->data(table->index(0, SensorMonitoringTableModel::ValueColumn))
                 .toString(), QStringLiteral("—"));
    QCOMPARE(table->data(table->index(0, SensorMonitoringTableModel::DisplayStatusColumn))
                 .toString(), QStringLiteral("STALE"));
    QVERIFY(ctrl.sensorCache()->rowsFor(loggerId).first().statusCode.isEmpty());

    ctrl.onSnapshotApplied(snap, 1, catalog.listByLoggerId(loggerId));
    QCOMPARE(table->data(table->index(0, SensorMonitoringTableModel::ValueColumn))
                 .toString(), QStringLiteral("22.7500"));

    // Switching to a different logger clears the rows even though the
    // cache still keeps the old logger's data.
    table->setLoggerId(loggerId + 999);
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(ctrl.sensorCache()->rowsFor(loggerId).size(), 1);
}

void TestSensorMonitoringTableModel::dashboardOverviewSeparatesSameIdWithDifferentMetadata()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("overview_identity"), QStringLiteral(":memory:")));
    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const auto anchor = addSeedLogger(ctrl, db, QStringLiteral("ANCHOR"),
                                      QStringLiteral("Anchor"), QStringLiteral("h"), 5020, 8080, {});
    const auto first = addSeedLogger(ctrl, db, QStringLiteral("OSG-A"),
                                     QStringLiteral("OSG A"), QStringLiteral("h"), 5020, 8080, {});
    const auto second = addSeedLogger(ctrl, db, QStringLiteral("OSG-B"),
                                      QStringLiteral("OSG B"), QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(anchor > 0 && first > 0 && second > 0);
    ctrl.setGraphLoggerId(anchor);

    LoggerSensor oxygen;
    oxygen.loggerId = first;
    oxygen.edgeSensorId = 164;
    oxygen.sensorType = QStringLiteral("ANALOG");
    oxygen.name = QStringLiteral("O2");
    oxygen.unit = QStringLiteral("%V");
    auto firstSnap = makeSnapshot(first);
    firstSnap.analogs = {makeAnalog(164, 11.42f)};
    ctrl.onSnapshotApplied(firstSnap, 1, {oxygen});

    LoggerSensor nitrogen;
    nitrogen.loggerId = second;
    nitrogen.edgeSensorId = 164;
    nitrogen.sensorType = QStringLiteral("ANALOG");
    nitrogen.name = QStringLiteral("NO2");
    nitrogen.unit = QStringLiteral("mg/Nm³");
    auto secondSnap = makeSnapshot(second);
    secondSnap.analogs = {makeAnalog(164, 2.0f)};
    ctrl.onSnapshotApplied(secondSnap, 1, {nitrogen});

    auto *overview = ctrl.overviewTable();
    QCOMPARE(overview->rowCount(), 2);
    QCOMPARE(overview->columnCount(), 3);
    int oxygenColumn = -1;
    int nitrogenColumn = -1;
    for (int col = 1; col < overview->columnCount(); ++col) {
        const auto header = overview->headerData(col, Qt::Horizontal).toString();
        if (header == QStringLiteral("O2 (%V)")) oxygenColumn = col;
        if (header == QStringLiteral("NO2 (mg/Nm³)")) nitrogenColumn = col;
    }
    QVERIFY(oxygenColumn > 0 && nitrogenColumn > 0);
    for (int row = 0; row < overview->rowCount(); ++row) {
        const auto id = overview->data(overview->index(row, 0),
                                       CentralLogger::Core::LoggerOverviewTableModel::LoggerIdRole).toLongLong();
        if (id == first) {
            QCOMPARE(overview->data(overview->index(row, oxygenColumn)).toString(), QStringLiteral("11.4200"));
            QVERIFY(overview->data(overview->index(row, nitrogenColumn)).toString().isEmpty());
        } else {
            QCOMPARE(id, second);
            QCOMPARE(overview->data(overview->index(row, nitrogenColumn)).toString(), QStringLiteral("2.0000"));
            QVERIFY(overview->data(overview->index(row, oxygenColumn)).toString().isEmpty());
        }
    }
}

QTEST_MAIN(TestSensorMonitoringTableModel)
#include "test_sensor_monitoring_table_model.moc"
