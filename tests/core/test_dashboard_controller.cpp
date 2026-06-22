#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
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

/// Wires a DashboardController + LoggerFormController over one in-memory DB,
/// matching the production composition in main.cpp. CRUD lives on the form
/// controller; the dashboard owns the loggers model it refreshes.
struct Harness {
    Database db;
    DashboardController dash{nullptr};
    LoggerFormController form{nullptr};

    explicit Harness(const char *suffix)
    {
        const bool opened = db.open(uniqueConnectionName(suffix),
                                    QStringLiteral(":memory:"));
        Q_ASSERT(opened);
        Q_UNUSED(opened);
        dash.setDatabase(&db);
        form.setDatabase(&db);
        form.setDashboardController(&dash);
        dash.reloadLoggers();
    }
};

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
    void buildEditPatchOmitsUnchangedStationCode();
    void buildEditPatchNeverPostsStationCode();
};

void TestDashboardController::addLoggerInsertsAndUpdatesModel()
{
    Harness h("add");

    QCOMPARE(h.dash.loggers()->rowCount(), 0);

    const qint64 id = h.form.addLogger(QStringLiteral("TRAM-001"),
                                       QStringLiteral("Trạm test"),
                                       QStringLiteral("192.168.1.10"),
                                       5020, 8080,
                                       QStringLiteral("token"));
    QVERIFY2(id > 0, qPrintable(h.form.lastError()));
    QCOMPARE(h.dash.loggers()->rowCount(), 1);
    QVERIFY(h.form.lastError().isEmpty());

    // Event row recorded.
    EventRepository events(h.db.connection());
    const auto recent = events.listRecent();
    QVERIFY(!recent.isEmpty());
    QCOMPARE(recent.first().eventType, QStringLiteral("Info"));
}

void TestDashboardController::addLoggerRejectsDuplicateStationCode()
{
    Harness h("dupe");

    QVERIFY(h.form.addLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("A"),
                             QStringLiteral("h"), 5020, 8080, {}) > 0);

    const qint64 second = h.form.addLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("B"),
                                           QStringLiteral("h"), 5020, 8080, {});
    QCOMPARE(second, qint64(-1));
    QVERIFY(h.form.lastError().contains(QStringLiteral("TRAM-DUP")));
    QCOMPARE(h.dash.loggers()->rowCount(), 1);
}

void TestDashboardController::addLoggerValidation()
{
    Harness h("valid");

    QCOMPARE(h.form.addLogger(QStringLiteral("  "), QStringLiteral("A"),
                              QStringLiteral("h"), 5020, 8080, {}), qint64(-1));
    QVERIFY(!h.form.lastError().isEmpty());

    QCOMPARE(h.form.addLogger(QStringLiteral("X"), QStringLiteral("A"),
                              QStringLiteral("h"), 0, 8080, {}), qint64(-1));
    QVERIFY(h.form.lastError().contains(QStringLiteral("Modbus port")));

    QCOMPARE(h.form.addLogger(QStringLiteral("X"), QStringLiteral("A"),
                              QStringLiteral(""), 5020, 8080, {}), qint64(-1));
}

void TestDashboardController::addLoggerRejectsInvalidHost()
{
    Harness h("host");

    QCOMPARE(h.form.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                              QStringLiteral("not a valid host!!!"), 5020, 8080, {}),
             qint64(-1));
    QVERIFY(h.form.lastError().contains(QStringLiteral("IPv4")));

    QCOMPARE(h.form.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                              QStringLiteral("192.168.1"), 5020, 8080, {}),
             qint64(-1));

    QVERIFY(h.form.addLogger(QStringLiteral("TRAM-H"), QStringLiteral("A"),
                             QStringLiteral("logger.local"), 5020, 8080, {}) > 0);
}

void TestDashboardController::updateLoggerRoundTrip()
{
    Harness h("upd");

    const qint64 id = h.form.addLogger(QStringLiteral("TRAM-U"), QStringLiteral("Old"),
                                       QStringLiteral("h1"), 5020, 8080,
                                       QStringLiteral("t1"));
    QVERIFY(id > 0);

    QVERIFY(h.form.updateLogger(id,
                                QStringLiteral("TRAM-U"),
                                QStringLiteral("New"),
                                QStringLiteral("h2"),
                                5030, 9090,
                                QStringLiteral("t2"),
                                QStringLiteral("note edited")));

    const auto data = h.form.getLoggerFormData(id);
    QCOMPARE(data.value(QStringLiteral("name")).toString(),   QStringLiteral("New"));
    QCOMPARE(data.value(QStringLiteral("host")).toString(),   QStringLiteral("h2"));
    QCOMPARE(data.value(QStringLiteral("modbusPort")).toInt(), 5030);
    QCOMPARE(data.value(QStringLiteral("apiPort")).toInt(),    9090);
    QCOMPARE(data.value(QStringLiteral("apiToken")).toString(), QStringLiteral("t2"));
    QCOMPARE(data.value(QStringLiteral("note")).toString(),    QStringLiteral("note edited"));
}

void TestDashboardController::removeLoggerCascadesCatalog()
{
    Harness h("rm");

    const qint64 id = h.form.addLogger(QStringLiteral("TRAM-RM"), QStringLiteral("A"),
                                       QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(id > 0);

    SensorCatalogRepository catalog(h.db.connection());
    QVERIFY(catalog.ensureExists(id, 1, QStringLiteral("ANALOG")) > 0);
    QCOMPARE(catalog.listByLoggerId(id).size(), 1);

    QVERIFY(h.form.removeLogger(id));
    QCOMPARE(h.dash.loggers()->rowCount(), 0);
    QCOMPARE(catalog.listByLoggerId(id).size(), 0);
}

void TestDashboardController::getLoggerFormDataMatchesInsert()
{
    Harness h("form");

    const qint64 id = h.form.addLogger(QStringLiteral("TRAM-F"), QStringLiteral("Form"),
                                       QStringLiteral("10.0.0.1"), 5021, 8081,
                                       QStringLiteral("secret"),
                                       QStringLiteral("hello"));
    QVERIFY(id > 0);

    const auto data = h.form.getLoggerFormData(id);
    QCOMPARE(data.value(QStringLiteral("id")).toLongLong(),    id);
    QCOMPARE(data.value(QStringLiteral("stationCode")).toString(), QStringLiteral("TRAM-F"));
    QCOMPARE(data.value(QStringLiteral("host")).toString(),    QStringLiteral("10.0.0.1"));
    QCOMPARE(data.value(QStringLiteral("modbusPort")).toInt(), 5021);
    QCOMPARE(data.value(QStringLiteral("note")).toString(),    QStringLiteral("hello"));
    QVERIFY(!data.contains(QStringLiteral("enabled")));

    QVERIFY(h.form.getLoggerFormData(9999).isEmpty());
}

void TestDashboardController::findAllWithSensorCountsCountsRows()
{
    Harness h("count");

    const qint64 a = h.form.addLogger(QStringLiteral("CNT-A"), QStringLiteral("A"),
                                      QStringLiteral("h"), 5020, 8080, {});
    const qint64 b = h.form.addLogger(QStringLiteral("CNT-B"), QStringLiteral("B"),
                                      QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(a > 0 && b > 0);

    SensorCatalogRepository catalog(h.db.connection());
    QVERIFY(catalog.ensureExists(a, 1, QStringLiteral("ANALOG")) > 0);
    QVERIFY(catalog.ensureExists(a, 2, QStringLiteral("ANALOG")) > 0);
    QVERIFY(catalog.ensureExists(b, 1, QStringLiteral("ANALOG")) > 0);

    h.dash.reloadLoggers();
    auto *model = h.dash.loggers();
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

    QVariantMap patch = LoggerFormController::buildEditPatch(original, edited);
    QCOMPARE(patch.size(), 1);
    QCOMPARE(patch.value(QStringLiteral("station_name")).toString(), QStringLiteral("Saigon"));
    QVERIFY(!patch.contains(QStringLiteral("poll_interval")));

    edited.insert(QStringLiteral("poll_interval"), 10);
    patch = LoggerFormController::buildEditPatch(original, edited);
    QCOMPARE(patch.size(), 2);
    QCOMPARE(patch.value(QStringLiteral("poll_interval")).toInt(), 10);
}

void TestDashboardController::buildEditPatchOmitsUnchangedStationCode()
{
    QVariantMap original;
    original.insert(QStringLiteral("station_code"), QStringLiteral("EDGE-DEVICE-01"));
    original.insert(QStringLiteral("station_name"), QStringLiteral("Plant A"));
    original.insert(QStringLiteral("poll_interval"), 5);

    QVariantMap edited = original;
    edited.insert(QStringLiteral("station_name"), QStringLiteral("Plant A"));
    edited.insert(QStringLiteral("poll_interval"), 5);

    const QVariantMap patch = LoggerFormController::buildEditPatch(original, edited);
    QVERIFY(!patch.contains(QStringLiteral("station_code")));
    QVERIFY(patch.isEmpty());
}

void TestDashboardController::buildEditPatchNeverPostsStationCode()
{
    QVariantMap original;
    original.insert(QStringLiteral("station_code"), QStringLiteral("EDGE-01"));
    original.insert(QStringLiteral("station_name"), QStringLiteral("Plant A"));

    QVariantMap edited = original;
    edited.insert(QStringLiteral("station_code"), QStringLiteral("EDGE-02"));
    edited.insert(QStringLiteral("station_name"), QStringLiteral("Plant B"));

    const QVariantMap patch = LoggerFormController::buildEditPatch(original, edited);
    QVERIFY(!patch.contains(QStringLiteral("station_code")));
    QCOMPARE(patch.value(QStringLiteral("station_name")).toString(), QStringLiteral("Plant B"));
}

QTEST_MAIN(TestDashboardController)
#include "test_dashboard_controller.moc"
