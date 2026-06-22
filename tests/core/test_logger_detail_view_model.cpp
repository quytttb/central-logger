#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/LoggerDetailViewModel.h"
#include "core/LoggerListModel.h"
#include "core/sensors/SensorMonitoringTableModel.h"
#include "core/sensors/SensorSnapshotCache.h"
#include "data/db/Database.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/modbus/ModbusTypes.h"

#include <QSignalSpy>

#include <QDateTime>
#include <QTest>
#include <QTimeZone>
#include <QVector>

using CentralLogger::Core::DashboardController;
using CentralLogger::Core::LoggerDetailViewModel;
using CentralLogger::Core::LoggerFormController;
using CentralLogger::Core::LoggerListModel;
using CentralLogger::Core::SensorMonitoringTableModel;
using CentralLogger::Data::Database;
using CentralLogger::Data::LoggerSensor;
using CentralLogger::Data::SensorCatalogRepository;
using CentralLogger::Network::AnalogSample;
using CentralLogger::Network::PollSnapshot;

namespace {

QString uniqueConnectionName(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("vm_detail_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

/// CRUD moved to LoggerFormController; seed loggers through it while the
/// dashboard models under test stay in sync.
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

class TestLoggerDetailViewModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesSensorTable();
    void setLoggerIdLoadsCacheBaseline();
    void snapshotForActiveLoggerRefreshesTable();
    void snapshotForOtherLoggerIgnored();
    void liveStateMirrorsListModelFlags();
    void listModelExposesRtuConnectedRole();
};

void TestLoggerDetailViewModel::exposesSensorTable()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("expose"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    LoggerDetailViewModel vm;
    vm.setServices(&db, /*rest*/ nullptr, /*appState*/ nullptr, &ctrl);

    // M-13 fix: each LoggerDetailViewModel owns its table — must NOT share the
    // DashboardController instance so Dashboard and Detail can't clobber state.
    QVERIFY(vm.sensorTable() != nullptr);
    QVERIFY2(vm.sensorTable() != ctrl.sensorTable(),
             "LoggerDetailViewModel must own a separate SensorMonitoringTableModel");
}

void TestLoggerDetailViewModel::setLoggerIdLoadsCacheBaseline()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("baseline"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 loggerId = addSeedLogger(ctrl, db, QStringLiteral("TRAM-T8"),
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

    // Snapshot arrives BEFORE detail page opens: cache is primed but the
    // controller's table is on no logger yet, so the cache holds the rows
    // while the table remains empty.
    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(1, 21.0f) };
    ctrl.onSnapshotApplied(snap, /*sensorCount*/ 1);
    QCOMPARE(ctrl.sensorCache()->rowsFor(loggerId).size(), 1);
    QCOMPARE(ctrl.sensorTable()->rowCount(), 0);

    // Now the user opens the detail page. setLoggerId must hydrate the
    // table from the cache without needing another poll cycle.
    LoggerDetailViewModel vm;
    vm.setServices(&db, nullptr, nullptr, &ctrl);
    vm.setLoggerId(loggerId);

    QCOMPARE(vm.sensorTable()->loggerId(), loggerId);
    QCOMPARE(vm.sensorTable()->rowCount(), 1);
    using M = SensorMonitoringTableModel;
    QCOMPARE(vm.sensorTable()
                 ->data(vm.sensorTable()->index(0, M::ValueColumn))
                 .toString(),
             QStringLiteral("21.0000"));
}

void TestLoggerDetailViewModel::snapshotForActiveLoggerRefreshesTable()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("active"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 loggerId = addSeedLogger(ctrl, db, QStringLiteral("TRAM-T8B"),
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

    LoggerDetailViewModel vm;
    vm.setServices(&db, nullptr, nullptr, &ctrl);
    vm.setLoggerId(loggerId);
    // Catalog placeholders from DB until the first Modbus snapshot arrives.
    QCOMPARE(vm.sensorTable()->rowCount(), 1);

    auto snap = makeSnapshot(loggerId);
    snap.analogs = { makeAnalog(1, 33.5f) };
    ctrl.onSnapshotApplied(snap, 1);

    QCOMPARE(vm.sensorTable()->rowCount(), 1);
    using M = SensorMonitoringTableModel;
    QCOMPARE(vm.sensorTable()
                 ->data(vm.sensorTable()->index(0, M::ValueColumn))
                 .toString(),
             QStringLiteral("33.5000"));
}

void TestLoggerDetailViewModel::snapshotForOtherLoggerIgnored()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("other"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 a = addSeedLogger(ctrl, db, QStringLiteral("TRAM-A"), QStringLiteral("A"),
                                    QStringLiteral("h"), 5020, 8080, {});
    const qint64 b = addSeedLogger(ctrl, db, QStringLiteral("TRAM-B"), QStringLiteral("B"),
                                    QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(a > 0 && b > 0);

    SensorCatalogRepository catalog(db.connection());
    LoggerSensor s;
    s.loggerId     = b;
    s.edgeSensorId = 1;
    s.sensorType   = QStringLiteral("ANALOG");
    s.name         = QStringLiteral("Temp");
    s.unit         = QStringLiteral("C");
    QVERIFY(catalog.upsert(s));

    LoggerDetailViewModel vm;
    vm.setServices(&db, nullptr, nullptr, &ctrl);
    vm.setLoggerId(a);
    QCOMPARE(vm.sensorTable()->rowCount(), 0);

    // Snapshot for logger B should NOT affect the active table (logger A).
    auto snap = makeSnapshot(b);
    snap.analogs = { makeAnalog(1, 99.0f) };
    ctrl.onSnapshotApplied(snap, 1);

    QCOMPARE(vm.sensorTable()->loggerId(), a);
    QCOMPARE(vm.sensorTable()->rowCount(), 0);
    // Cache still records B's rows.
    QCOMPARE(ctrl.sensorCache()->rowsFor(b).size(), 1);
}

void TestLoggerDetailViewModel::liveStateMirrorsListModelFlags()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("live"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 loggerId = addSeedLogger(ctrl, db, QStringLiteral("TRAM-LIVE"),
                                           QStringLiteral("Live"),
                                           QStringLiteral("h"),
                                           5020, 8080, {});
    QVERIFY(loggerId > 0);

    LoggerDetailViewModel vm;
    vm.setServices(&db, nullptr, nullptr, &ctrl);
    vm.setLoggerId(loggerId);

    // Before any snapshot: all flags false.
    QVERIFY(!vm.online());
    QVERIFY(!vm.polling());
    QVERIFY(!vm.anyAlarm());
    QVERIFY(!vm.rtuConnected());

    QSignalSpy spy(&vm, &LoggerDetailViewModel::liveStateChanged);

    PollSnapshot snap;
    snap.loggerId = loggerId;
    snap.success  = true;
    // bit0=polling, bit1=RTU connected, bit2=any alarm
    snap.header.mapVersion  = 1;
    snap.header.statusFlags = 0x07;
    ctrl.onSnapshotApplied(snap, /*sensorCount*/ 0);

    QVERIFY(spy.count() >= 1);
    QVERIFY(vm.online());
    QVERIFY(vm.polling());
    QVERIFY(vm.rtuConnected());
    QVERIFY(vm.anyAlarm());

    // Drop to offline.
    PollSnapshot bad;
    bad.loggerId = loggerId;
    bad.success  = false;
    ctrl.onSnapshotApplied(bad, 0);

    QVERIFY(!vm.online());
    QVERIFY(!vm.polling());
    QVERIFY(!vm.anyAlarm());
    QVERIFY(!vm.rtuConnected());
}

void TestLoggerDetailViewModel::listModelExposesRtuConnectedRole()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("role"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const qint64 loggerId = addSeedLogger(ctrl, db, QStringLiteral("TRAM-RTU"),
                                           QStringLiteral("RTU"),
                                           QStringLiteral("h"),
                                           5020, 8080, {});
    QVERIFY(loggerId > 0);

    PollSnapshot snap;
    snap.loggerId = loggerId;
    snap.success  = true;
    snap.header.mapVersion  = 1;
    snap.header.statusFlags = 0x02; // RTU connected only
    ctrl.onSnapshotApplied(snap, /*sensorCount*/ 0);

    auto *model = ctrl.loggers();
    const int row = model->indexOfLogger(loggerId);
    QVERIFY(row >= 0);
    const QModelIndex idx = model->index(row, 0);
    QCOMPARE(model->data(idx, LoggerListModel::RtuConnectedRole).toBool(), true);
    QCOMPARE(model->data(idx, LoggerListModel::PollingRole).toBool(), false);
    QCOMPARE(model->data(idx, LoggerListModel::AnyAlarmRole).toBool(), false);

    const auto names = model->roleNames();
    QVERIFY(names.value(LoggerListModel::RtuConnectedRole) == "rtuConnected");
}

QTEST_MAIN(TestLoggerDetailViewModel)
#include "test_logger_detail_view_model.moc"
