// Tests for the Thresholds screen backend:
//   - ThresholdTableModel: columns, filter, dirty tracking, discard/save reset
//   - ThresholdController: catalog load (active ANALOG only), validation and
//     persistence of min/max thresholds in `logger_sensor`.
//
// Thresholds are applied centrally: SensorMerger raises ALARM when a live
// value is <= min or >= max, so these tests only cover the editing path.

#include "core/threshold/ThresholdController.h"
#include "core/threshold/ThresholdTableModel.h"
#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "utils/SensorConstants.h"

#include <QSignalSpy>
#include <QTest>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;

namespace {

QString uniqueConn(const char *tag)
{
    static int s = 0;
    return QStringLiteral("threshold_%1_%2").arg(tag).arg(++s);
}

ThresholdRow makeRow(qint64 sensorId, int edgeSensorId, const QString &name,
                     const QString &unit, const QString &min,
                     const QString &max)
{
    ThresholdRow row;
    row.sensorId = sensorId;
    row.edgeSensorId = edgeSensorId;
    row.name = name;
    row.unit = unit;
    row.minText = min;
    row.maxText = max;
    row.originalMinText = min;
    row.originalMaxText = max;
    return row;
}

LoggerSensor makeSensor(qint64 loggerId, int edgeSensorId, const QString &type,
                        const QString &name, const QString &unit,
                        std::optional<double> min, std::optional<double> max,
                        bool active = true)
{
    LoggerSensor sensor;
    sensor.loggerId = loggerId;
    sensor.edgeSensorId = edgeSensorId;
    sensor.sensorType = type;
    sensor.name = name;
    sensor.unit = unit;
    sensor.minThreshold = min;
    sensor.maxThreshold = max;
    sensor.active = active;
    return sensor;
}

struct Harness {
    Database db;
    ThresholdController controller{nullptr};
    qint64 loggerId = 0;

    explicit Harness(const char *tag)
    {
        if (!db.open(uniqueConn(tag), QStringLiteral(":memory:"))) {
            qFatal("Harness: Database::open() failed");
        }
        controller.setDatabase(&db);

        LoggerRepository loggerRepo(db.connection());
        LoggerInfo info;
        info.stationCode = QStringLiteral("SIM-001");
        info.name = QStringLiteral("Tram 1");
        info.host = QStringLiteral("127.0.0.1");
        QString error;
        if (!loggerRepo.insert(info, &error)) {
            qFatal("Harness: logger insert failed: %s", qPrintable(error));
        }
        loggerId = info.id;

        SensorCatalogRepository catalog(db.connection());
        auto analog = makeSensor(loggerId, 1, CentralLogger::Sensor::kTypeAnalog,
                                 QStringLiteral("pH"), QStringLiteral("pH"),
                                 6.0, 9.0);
        auto analogInactive =
            makeSensor(loggerId, 2, CentralLogger::Sensor::kTypeAnalog,
                       QStringLiteral("TSS"), QStringLiteral("mg/L"),
                       0.0, 100.0, false);
        auto digital = makeSensor(loggerId, 0, CentralLogger::Sensor::kTypeDi,
                                  QStringLiteral("Bom"), QStringLiteral(""),
                                  std::nullopt, std::nullopt);
        for (auto *sensor : {&analog, &analogInactive, &digital}) {
            if (!catalog.upsert(*sensor, &error)) {
                qFatal("Harness: sensor insert failed: %s", qPrintable(error));
            }
        }
    }
};

} // namespace

class TestThresholdController : public QObject
{
    Q_OBJECT

private slots:
    // --- ThresholdTableModel -------------------------------------------
    void model_exposesThreeColumns();
    void model_filterMatchesNameAndUnit();
    void model_dirtyTrackingAndReset();
    void model_markSavedClearsDirty();

    // --- ThresholdController -------------------------------------------
    void controller_refresh_loadsOnlyActiveAnalog();
    void controller_refresh_picksFirstLogger();
    void controller_save_rejectsInvalidNumber();
    void controller_save_rejectsMinNotLessThanMax();
    void controller_save_persistsAndClearsDirty();
    void controller_discardChanges_restoresDbValues();
};

// ---------------------------------------------------------------------------
// ThresholdTableModel
// ---------------------------------------------------------------------------

void TestThresholdController::model_exposesThreeColumns()
{
    ThresholdTableModel model;
    model.setRows({ makeRow(1, 1, QStringLiteral("pH"), QStringLiteral("pH"),
                            QStringLiteral("6"), QStringLiteral("9")) });

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), 3);

    const QModelIndex nameIndex = model.index(0, 0);
    QCOMPARE(nameIndex.data(Qt::DisplayRole).toString(), QStringLiteral("pH"));
    QCOMPARE(nameIndex.data(ThresholdTableModel::IsNameColumnRole).toBool(),
             true);
    QCOMPARE(model.index(0, 1).data(ThresholdTableModel::MinTextRole).toString(),
             QStringLiteral("6"));
    QCOMPARE(model.index(0, 2).data(ThresholdTableModel::MaxTextRole).toString(),
             QStringLiteral("9"));
    QCOMPARE(model.index(0, 1).data(ThresholdTableModel::IsNameColumnRole).toBool(),
             false);
}

void TestThresholdController::model_filterMatchesNameAndUnit()
{
    ThresholdTableModel model;
    model.setRows({
        makeRow(1, 1, QStringLiteral("pH"), QStringLiteral("pH"), {}, {}),
        makeRow(2, 2, QStringLiteral("TSS"), QStringLiteral("mg/L"), {}, {}),
        makeRow(3, 3, QStringLiteral("Nhiệt độ"), QStringLiteral("°C"), {}, {}),
    });
    QCOMPARE(model.rowsSize(), 3);

    model.setFilterText(QStringLiteral("tss"));
    QCOMPARE(model.rowsSize(), 1);
    QCOMPARE(model.index(0, 0).data(ThresholdTableModel::NameRole).toString(),
             QStringLiteral("TSS"));

    // Unit-only match.
    model.setFilterText(QStringLiteral("°C"));
    QCOMPARE(model.rowsSize(), 1);
    QCOMPARE(model.index(0, 0).data(ThresholdTableModel::NameRole).toString(),
             QStringLiteral("Nhiệt độ"));

    model.setFilterText(QStringLiteral("  "));
    QCOMPARE(model.rowsSize(), 3);
}

void TestThresholdController::model_dirtyTrackingAndReset()
{
    ThresholdTableModel model;
    model.setRows({ makeRow(1, 1, QStringLiteral("pH"), QStringLiteral("pH"),
                            QStringLiteral("6"), QStringLiteral("9")) });
    QCOMPARE(model.dirty(), false);

    QSignalSpy dirtySpy(&model, &ThresholdTableModel::dirtyChanged);
    model.setCellText(0, 2, QStringLiteral("10"));
    QCOMPARE(model.dirty(), true);
    QCOMPARE(model.dirtyCount(), 1);
    QCOMPARE(dirtySpy.count(), 1);
    QCOMPARE(model.index(0, 2).data(ThresholdTableModel::RowDirtyRole).toBool(),
             true);

    // Editing back to the DB value clears the dirty flag.
    model.setCellText(0, 2, QStringLiteral("9"));
    QCOMPARE(model.dirty(), false);

    // Reset restores every cell.
    model.setCellText(0, 1, QStringLiteral("5"));
    model.setCellText(0, 2, QStringLiteral("12"));
    QCOMPARE(model.dirtyCount(), 1);
    model.resetEdits();
    QCOMPARE(model.dirty(), false);
    QCOMPARE(model.index(0, 1).data(ThresholdTableModel::MinTextRole).toString(),
             QStringLiteral("6"));
    QCOMPARE(model.index(0, 2).data(ThresholdTableModel::MaxTextRole).toString(),
             QStringLiteral("9"));
}

void TestThresholdController::model_markSavedClearsDirty()
{
    ThresholdTableModel model;
    model.setRows({ makeRow(1, 1, QStringLiteral("pH"), QStringLiteral("pH"),
                            QStringLiteral("6"), QStringLiteral("9")) });

    model.setCellText(0, 1, QStringLiteral("5.5"));
    QVERIFY(model.dirty());
    model.markSaved();
    QVERIFY(!model.dirty());

    // The saved value becomes the new baseline for dirty tracking.
    model.setCellText(0, 1, QStringLiteral("5.5"));
    QVERIFY(!model.dirty());
    model.setCellText(0, 1, QStringLiteral("4"));
    QVERIFY(model.dirty());
}

// ---------------------------------------------------------------------------
// ThresholdController
// ---------------------------------------------------------------------------

void TestThresholdController::controller_refresh_loadsOnlyActiveAnalog()
{
    Harness h("refresh");
    h.controller.refresh();

    QCOMPARE(h.controller.loggerItems().size(), 1);
    QCOMPARE(h.controller.selectedLoggerId(), h.loggerId);
    QCOMPARE(h.controller.selectedLoggerName(), QStringLiteral("Tram 1"));

    ThresholdTableModel *table = h.controller.table();
    QCOMPARE(table->rowsSize(), 1); // inactive ANALOG + DI filtered out
    QCOMPARE(table->index(0, 0).data(ThresholdTableModel::NameRole).toString(),
             QStringLiteral("pH"));
    QCOMPARE(table->index(0, 0).data(ThresholdTableModel::UnitRole).toString(),
             QStringLiteral("pH"));
    QCOMPARE(table->index(0, 1).data(ThresholdTableModel::MinTextRole).toString(),
             QStringLiteral("6"));
    QCOMPARE(table->index(0, 2).data(ThresholdTableModel::MaxTextRole).toString(),
             QStringLiteral("9"));
    QVERIFY(!table->dirty());
    QVERIFY(h.controller.lastError().isEmpty());
}

void TestThresholdController::controller_refresh_picksFirstLogger()
{
    Harness h("first");

    LoggerRepository repo(h.db.connection());
    LoggerInfo second;
    second.stationCode = QStringLiteral("SIM-002");
    second.name = QStringLiteral("Tram 2");
    second.host = QStringLiteral("127.0.0.2");
    QString error;
    QVERIFY2(repo.insert(second, &error), qPrintable(error));

    h.controller.refresh();
    QCOMPARE(h.controller.loggerItems().size(), 2);
    QCOMPARE(h.controller.selectedLoggerId(), h.loggerId);

    // Switching loggers reloads the rows for that logger.
    h.controller.setSelectedLoggerId(second.id);
    QCOMPARE(h.controller.selectedLoggerId(), second.id);
    QCOMPARE(h.controller.selectedLoggerName(), QStringLiteral("Tram 2"));
    QCOMPARE(h.controller.table()->rowsSize(), 0);
}

void TestThresholdController::controller_save_rejectsInvalidNumber()
{
    Harness h("invalid");
    h.controller.refresh();

    QSignalSpy finished(&h.controller,
                        &ThresholdController::saveFinished);
    h.controller.table()->setCellText(0, 1, QStringLiteral("abc"));
    h.controller.save();

    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.at(0).at(0).toBool());
    QVERIFY(h.controller.lastError().contains(QStringLiteral("not a valid number")));

    // Nothing written: the DB still has the original value.
    SensorCatalogRepository repo(h.db.connection());
    const auto sensor = repo.findByLoggerAndEdgeId(
        h.loggerId, 1, CentralLogger::Sensor::kTypeAnalog);
    QVERIFY(sensor.has_value());
    QCOMPARE(sensor->minThreshold.value_or(0.0), 6.0);
}

void TestThresholdController::controller_save_rejectsMinNotLessThanMax()
{
    Harness h("minmax");
    h.controller.refresh();

    QSignalSpy finished(&h.controller,
                        &ThresholdController::saveFinished);
    h.controller.table()->setCellText(0, 1, QStringLiteral("10"));
    h.controller.table()->setCellText(0, 2, QStringLiteral("10"));
    h.controller.save();

    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.at(0).at(0).toBool());
    QVERIFY(h.controller.lastError().contains(QStringLiteral("lower than Max")));

    SensorCatalogRepository repo(h.db.connection());
    const auto sensor = repo.findByLoggerAndEdgeId(
        h.loggerId, 1, CentralLogger::Sensor::kTypeAnalog);
    QVERIFY(sensor.has_value());
    QCOMPARE(sensor->minThreshold.value_or(0.0), 6.0);
    QCOMPARE(sensor->maxThreshold.value_or(0.0), 9.0);
}

void TestThresholdController::controller_save_persistsAndClearsDirty()
{
    Harness h("persist");
    h.controller.refresh();

    QSignalSpy finished(&h.controller,
                        &ThresholdController::saveFinished);
    h.controller.table()->setCellText(0, 1, QStringLiteral("5.5"));
    h.controller.table()->setCellText(0, 2, QStringLiteral("8.5"));
    h.controller.save();

    QCOMPARE(finished.count(), 1);
    QVERIFY2(finished.at(0).at(0).toBool(),
             qPrintable(h.controller.lastError()));
    QVERIFY(!h.controller.table()->dirty());

    SensorCatalogRepository repo(h.db.connection());
    const auto sensor = repo.findByLoggerAndEdgeId(
        h.loggerId, 1, CentralLogger::Sensor::kTypeAnalog);
    QVERIFY(sensor.has_value());
    QCOMPARE(sensor->minThreshold.value_or(0.0), 5.5);
    QCOMPARE(sensor->maxThreshold.value_or(0.0), 8.5);

    // An empty field clears the threshold (nullable column).
    h.controller.table()->setCellText(0, 1, QString{});
    h.controller.save();
    QVERIFY2(finished.at(1).at(0).toBool(),
             qPrintable(h.controller.lastError()));
    const auto cleared = repo.findByLoggerAndEdgeId(
        h.loggerId, 1, CentralLogger::Sensor::kTypeAnalog);
    QVERIFY(cleared.has_value());
    QVERIFY(!cleared->minThreshold.has_value());
    QCOMPARE(cleared->maxThreshold.value_or(0.0), 8.5);
}

void TestThresholdController::controller_discardChanges_restoresDbValues()
{
    Harness h("discard");
    h.controller.refresh();

    h.controller.table()->setCellText(0, 1, QStringLiteral("1"));
    h.controller.table()->setCellText(0, 2, QStringLiteral("2"));
    QVERIFY(h.controller.table()->dirty());

    h.controller.discardChanges();

    QVERIFY(!h.controller.table()->dirty());
    QCOMPARE(h.controller.table()
                 ->index(0, 1)
                 .data(ThresholdTableModel::MinTextRole)
                 .toString(),
             QStringLiteral("6"));
    QCOMPARE(h.controller.table()
                 ->index(0, 2)
                 .data(ThresholdTableModel::MaxTextRole)
                 .toString(),
             QStringLiteral("9"));

    SensorCatalogRepository repo(h.db.connection());
    const auto sensor = repo.findByLoggerAndEdgeId(
        h.loggerId, 1, CentralLogger::Sensor::kTypeAnalog);
    QVERIFY(sensor.has_value());
    QCOMPARE(sensor->minThreshold.value_or(0.0), 6.0);
    QCOMPARE(sensor->maxThreshold.value_or(0.0), 9.0);
}

QTEST_MAIN(TestThresholdController)
#include "test_threshold_controller.moc"
