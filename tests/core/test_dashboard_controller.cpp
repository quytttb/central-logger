#include "core/DashboardController.h"
#include "core/LoggerListModel.h"
#include "data/db/Database.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/EventRepository.h"
#include "data/repositories/SensorCatalogRepository.h"

#include <QString>
#include <QTest>
#include <QVariantMap>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;

namespace {

QString uniqueConnectionName(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("ctrl_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

} // namespace

class TestDashboardController : public QObject
{
    Q_OBJECT

private slots:
    void addLoggerInsertsAndUpdatesModel();
    void addLoggerRejectsDuplicateStationCode();
    void addLoggerValidation();
    void addLoggerRejectsInvalidHost();
    void updateLoggerRoundTrip();
    void removeLoggerCascadesCatalog();
    void getLoggerFormDataMatchesInsert();
    void findAllWithSensorCountsCountsRows();
    void buildEditPatchDiffsCorrectly();
};

void TestDashboardController::addLoggerInsertsAndUpdatesModel()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("add"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    ctrl.reloadLoggers();

    QCOMPARE(ctrl.loggers()->rowCount(), 0);

    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-001"),
                                     QStringLiteral("Trạm test"),
                                     QStringLiteral("192.168.1.10"),
                                     5020, 8080,
                                     QStringLiteral("token"));
    QVERIFY2(id > 0, qPrintable(ctrl.lastError()));
    QCOMPARE(ctrl.loggers()->rowCount(), 1);
    QVERIFY(ctrl.lastError().isEmpty());

    // Event row recorded.
    EventRepository events(db.connection());
    const auto recent = events.listRecent();
    QVERIFY(!recent.isEmpty());
    QCOMPARE(recent.first().eventType, QStringLiteral("Info"));
}

void TestDashboardController::addLoggerRejectsDuplicateStationCode()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("dupe"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    QVERIFY(ctrl.addLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("A"),
                           QStringLiteral("h"), 5020, 8080, {}) > 0);

    const qint64 second = ctrl.addLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("B"),
                                         QStringLiteral("h"), 5020, 8080, {});
    QCOMPARE(second, qint64(-1));
    QVERIFY(ctrl.lastError().contains(QStringLiteral("TRAM-DUP")));
    QCOMPARE(ctrl.loggers()->rowCount(), 1);
}

void TestDashboardController::addLoggerValidation()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("valid"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    QCOMPARE(ctrl.addLogger(QStringLiteral("  "), QStringLiteral("A"),
                            QStringLiteral("h"), 5020, 8080, {}), qint64(-1));
    QVERIFY(!ctrl.lastError().isEmpty());

    QCOMPARE(ctrl.addLogger(QStringLiteral("X"), QStringLiteral("A"),
                            QStringLiteral("h"), 0, 8080, {}), qint64(-1));
    QVERIFY(ctrl.lastError().contains(QStringLiteral("Modbus port")));

    QCOMPARE(ctrl.addLogger(QStringLiteral("X"), QStringLiteral("A"),
                            QStringLiteral(""), 5020, 8080, {}), qint64(-1));
}

void TestDashboardController::addLoggerRejectsInvalidHost()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("host"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    QCOMPARE(ctrl.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                            QStringLiteral("not a valid host!!!"), 5020, 8080, {}),
             qint64(-1));
    QVERIFY(ctrl.lastError().contains(QStringLiteral("IPv4")));

    QCOMPARE(ctrl.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                            QStringLiteral("192.168.1"), 5020, 8080, {}),
             qint64(-1));

    QVERIFY(ctrl.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                         QStringLiteral("logger.local"), 5020, 8080, {}) > 0);
}

void TestDashboardController::updateLoggerRoundTrip()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("upd"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-U"), QStringLiteral("Old"),
                                     QStringLiteral("h1"), 5020, 8080,
                                     QStringLiteral("t1"));
    QVERIFY(id > 0);

    QVERIFY(ctrl.updateLogger(id,
                              QStringLiteral("TRAM-U"),
                              QStringLiteral("New"),
                              QStringLiteral("h2"),
                              5030, 9090,
                              QStringLiteral("t2"),
                              QStringLiteral("note edited")));

    const auto data = ctrl.getLoggerFormData(id);
    QCOMPARE(data.value(QStringLiteral("name")).toString(),   QStringLiteral("New"));
    QCOMPARE(data.value(QStringLiteral("host")).toString(),   QStringLiteral("h2"));
    QCOMPARE(data.value(QStringLiteral("modbusPort")).toInt(), 5030);
    QCOMPARE(data.value(QStringLiteral("apiPort")).toInt(),    9090);
    QCOMPARE(data.value(QStringLiteral("apiToken")).toString(), QStringLiteral("t2"));
    QCOMPARE(data.value(QStringLiteral("note")).toString(),    QStringLiteral("note edited"));
}

void TestDashboardController::removeLoggerCascadesCatalog()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("rm"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-RM"), QStringLiteral("A"),
                                     QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(id > 0);

    SensorCatalogRepository catalog(db.connection());
    QVERIFY(catalog.ensureExists(id, 1, QStringLiteral("ANALOG")) > 0);
    QCOMPARE(catalog.listByLoggerId(id).size(), 1);

    QVERIFY(ctrl.removeLogger(id));
    QCOMPARE(ctrl.loggers()->rowCount(), 0);
    QCOMPARE(catalog.listByLoggerId(id).size(), 0);
}

void TestDashboardController::getLoggerFormDataMatchesInsert()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("form"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-F"), QStringLiteral("Form"),
                                     QStringLiteral("10.0.0.1"), 5021, 8081,
                                     QStringLiteral("secret"),
                                     QStringLiteral("hello"));
    QVERIFY(id > 0);

    const auto data = ctrl.getLoggerFormData(id);
    QCOMPARE(data.value(QStringLiteral("id")).toLongLong(),    id);
    QCOMPARE(data.value(QStringLiteral("stationCode")).toString(), QStringLiteral("TRAM-F"));
    QCOMPARE(data.value(QStringLiteral("host")).toString(),    QStringLiteral("10.0.0.1"));
    QCOMPARE(data.value(QStringLiteral("modbusPort")).toInt(), 5021);
    QCOMPARE(data.value(QStringLiteral("note")).toString(),    QStringLiteral("hello"));
    QVERIFY(!data.contains(QStringLiteral("enabled")));

    QVERIFY(ctrl.getLoggerFormData(9999).isEmpty());
}

void TestDashboardController::findAllWithSensorCountsCountsRows()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("count"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    const qint64 a = ctrl.addLogger(QStringLiteral("CNT-A"), QStringLiteral("A"),
                                    QStringLiteral("h"), 5020, 8080, {});
    const qint64 b = ctrl.addLogger(QStringLiteral("CNT-B"), QStringLiteral("B"),
                                    QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(a > 0 && b > 0);

    SensorCatalogRepository catalog(db.connection());
    QVERIFY(catalog.ensureExists(a, 1, QStringLiteral("ANALOG")) > 0);
    QVERIFY(catalog.ensureExists(a, 2, QStringLiteral("ANALOG")) > 0);
    QVERIFY(catalog.ensureExists(b, 1, QStringLiteral("ANALOG")) > 0);

    ctrl.reloadLoggers();
    auto *model = ctrl.loggers();
    QCOMPARE(model->rowCount(), 2);

    const int sensorRole = LoggerListModel::SensorCountRole;
    QCOMPARE(model->data(model->index(0, 0), sensorRole).toInt(), 2);
    QCOMPARE(model->data(model->index(1, 0), sensorRole).toInt(), 1);
}

void TestDashboardController::buildEditPatchDiffsCorrectly()
{
    QVariantMap original;
    original.insert(QStringLiteral("station_name"), QStringLiteral("Hanoi"));
    original.insert(QStringLiteral("poll_interval"), 5);
    original.insert(QStringLiteral("modbus_tcp_enabled"), true);

    QVariantMap edited = original;
    edited.insert(QStringLiteral("station_name"), QStringLiteral("Saigon"));
    edited.insert(QStringLiteral("poll_interval"), 5);

    QVariantMap patch = DashboardController::buildEditPatch(original, edited);
    QCOMPARE(patch.size(), 1);
    QCOMPARE(patch.value(QStringLiteral("station_name")).toString(), QStringLiteral("Saigon"));
    QVERIFY(!patch.contains(QStringLiteral("poll_interval")));

    edited.insert(QStringLiteral("poll_interval"), 10);
    patch = DashboardController::buildEditPatch(original, edited);
    QCOMPARE(patch.size(), 2);
    QCOMPARE(patch.value(QStringLiteral("poll_interval")).toInt(), 10);
}

QTEST_MAIN(TestDashboardController)
#include "test_dashboard_controller.moc"
