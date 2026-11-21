#include "core/threshold/ThresholdController.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"

#include <QSignalSpy>
#include <QTest>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;

class TestThresholdController : public QObject
{
    Q_OBJECT

private slots:
    void tableDisplaysAndFiltersWithoutEditing();
    void loadsMirrorForDisabledOrOfflineLogger();
};

void TestThresholdController::tableDisplaysAndFiltersWithoutEditing()
{
    ThresholdTableModel table;
    table.setRows({
        {QStringLiteral("Temperature"), QStringLiteral("°C"), QStringLiteral("-5"), QStringLiteral("25")},
        {QStringLiteral("Flow"), QStringLiteral("m3/h"), {}, QStringLiteral("60")},
    });
    QCOMPARE(table.columnCount(), 3);
    QCOMPARE(table.rowsSize(), 2);
    QCOMPARE(table.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Min"));
    QCOMPARE(table.data(table.index(0, 0)).toString(), QStringLiteral("Temperature"));
    QCOMPARE(table.data(table.index(0, 0), ThresholdTableModel::UnitRole).toString(),
             QStringLiteral("°C"));
    QCOMPARE(table.data(table.index(0, 1)).toString(), QStringLiteral("-5"));
    QCOMPARE(table.data(table.index(0, 2)).toString(), QStringLiteral("25"));
    QVERIFY(table.data(table.index(1, 1)).toString().isEmpty());
    QVERIFY(!table.flags(table.index(0, 1)).testFlag(Qt::ItemIsEditable));
    QVERIFY(!table.setData(table.index(0, 1), QStringLiteral("999"), Qt::EditRole));

    table.setFilterText(QStringLiteral(" M3/H "));
    QCOMPARE(table.rowsSize(), 1);
    QCOMPARE(table.data(table.index(0, 0)).toString(), QStringLiteral("Flow"));
    table.setFilterText(QStringLiteral("temp"));
    QCOMPARE(table.rowsSize(), 1);
    QCOMPARE(table.data(table.index(0, 0)).toString(), QStringLiteral("Temperature"));
    table.setFilterText(QStringLiteral("no match"));
    QCOMPARE(table.rowsSize(), 0);
    table.setFilterText({});
    QCOMPARE(table.rowsSize(), 2);
}

void TestThresholdController::loadsMirrorForDisabledOrOfflineLogger()
{
    Database db;
    QVERIFY(db.open(QStringLiteral("threshold_mirror"), QStringLiteral(":memory:")));
    LoggerRepository loggers(db.connection());
    LoggerInfo logger;
    logger.stationCode = QStringLiteral("TH-01");
    logger.name = QStringLiteral("Local logger");
    logger.host = QStringLiteral("127.0.0.1");
    logger.enabled = false;
    QVERIFY(loggers.insert(logger));

    SensorCatalogRepository catalog(db.connection());
    LoggerSensor active;
    active.loggerId = logger.id;
    active.edgeSensorId = 1;
    active.sensorType = QStringLiteral("ANALOG");
    active.name = QStringLiteral("Level");
    active.minThreshold = 0.0;
    active.maxThreshold = 15.25;
    QVERIFY(catalog.upsert(active));
    auto inactive = active;
    inactive.edgeSensorId = 2;
    inactive.active = false;
    QVERIFY(catalog.upsert(inactive));
    auto digital = active;
    digital.edgeSensorId = 3;
    digital.sensorType = QStringLiteral("DI");
    QVERIFY(catalog.upsert(digital));

    ThresholdController controller(nullptr);
    controller.setDatabase(&db);
    auto *table = controller.table();
    QCOMPARE(table->rowsSize(), 0);
    controller.setSelectedLoggerId(logger.id);
    QVERIFY(controller.lastError().isEmpty());
    QCOMPARE(table->rowsSize(), 1);
    QCOMPARE(table->data(table->index(0, 0)).toString(), active.name);
    QCOMPARE(table->data(table->index(0, 1)).toString(), QStringLiteral("0"));
    QCOMPARE(table->data(table->index(0, 2)).toString(), QStringLiteral("15.25"));

    // A later GET can change the mirror without creating an edit surface.
    QSignalSpy updated(table, &QAbstractItemModel::modelReset);
    active.maxThreshold.reset();
    QVERIFY(catalog.upsert(active));
    controller.refresh();
    QCOMPARE(updated.size(), 1);
    QVERIFY(table->data(table->index(0, 2)).toString().isEmpty());
    controller.setSelectedLoggerId(0);
    QCOMPARE(table->rowsSize(), 0);
}

QTEST_GUILESS_MAIN(TestThresholdController)
#include "test_threshold_controller.moc"
