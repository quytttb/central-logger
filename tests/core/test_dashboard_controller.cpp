#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/LoggerListModel.h"
#include "core/SettingsController.h"
#include "core/sensors/LoggerOverviewTableModel.h"
#include "data/db/Database.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/EventRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/modbus/ModbusTypes.h"
#include "utils/UiConstants.h"

#include <QDateTime>
#include <QString>
#include <QTest>
#include <QVariantMap>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;
using namespace CentralLogger::Network;

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
    void summaryResolvesPrimaryLoggerAndBuildsOverview();
    void summaryBuildsPerSensorMiniCharts();
    void summaryGraphLoggerDropdownSwitchesChartAndOverview();
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

void TestDashboardController::summaryResolvesPrimaryLoggerAndBuildsOverview()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("summary"), QStringLiteral(":memory:")));

    DashboardController dash{nullptr};
    LoggerFormController form{nullptr};
    SettingsController settings{nullptr};

    dash.setDatabase(&db);
    form.setDatabase(&db);
    form.setDashboardController(&dash);
    settings.setDatabase(&db);
    settings.load();
    dash.setSettingsController(&settings);

    const qint64 firstId = form.addLogger(QStringLiteral("TRAM-S1"),
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("192.168.1.10"),
                                          5020, 8080, QString());
    const qint64 secondId = form.addLogger(QStringLiteral("TRAM-S2"),
                                           QStringLiteral("Beta"),
                                           QStringLiteral("192.168.1.11"),
                                           5020, 8080, QString());
    QVERIFY2(firstId > 0, qPrintable(form.lastError()));
    QVERIFY2(secondId > 0, qPrintable(form.lastError()));

    SensorCatalogRepository catalog(db.connection());
    auto addAnalog = [&catalog](qint64 loggerId) {
        LoggerSensor s;
        s.loggerId     = loggerId;
        s.edgeSensorId = 1;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = QStringLiteral("O2");
        s.unit         = QStringLiteral("%V");
        s.decimals     = 2;
        QVERIFY(catalog.upsert(s));
    };
    addAnalog(firstId);
    addAnalog(secondId);

    // Configuring the primary logger re-resolves the summary immediately.
    settings.setPrimaryLoggerId(secondId);
    QCOMPARE(dash.primaryLoggerId(), secondId);
    QCOMPARE(dash.primaryLoggerName(), QStringLiteral("Beta"));

    auto *overview = dash.overviewTable();
    QVERIFY(overview);
    QCOMPARE(overview->rowCount(), 1); // primary logger is excluded
    QCOMPARE(overview->data(overview->index(0, 0)).toString(),
             QStringLiteral("Alpha"));

    auto makeSnapshot = [](qint64 loggerId, float value) {
        PollSnapshot snap;
        snap.loggerId   = loggerId;
        snap.success    = true;
        snap.measuredAt = QDateTime::currentDateTimeUtc();
        AnalogSample analog;
        analog.edgeSensorId = 1;
        analog.statusCode   = QLatin1String("00"); // valid
        analog.value        = value;
        snap.analogs.append(analog);
        return snap;
    };

    // Primary snapshot feeds the real-time chart, not the overview table.
    dash.onSnapshotApplied(makeSnapshot(secondId, 12.5f), 1,
                           catalog.listByLoggerId(secondId));
    QVERIFY(dash.graphChartHasData());
    QCOMPARE(dash.graphChartSeries().size(), 1);
    QCOMPARE(overview->columnCount(), 1); // secondary logger has no snapshot yet

    // Secondary snapshot fills the dynamic sensor column.
    dash.onSnapshotApplied(makeSnapshot(firstId, 7.25f), 1,
                           catalog.listByLoggerId(firstId));
    QCOMPARE(overview->columnCount(), 2);
    QCOMPARE(overview->headerData(1, Qt::Horizontal).toString(),
             QStringLiteral("O2 (%V)"));
    QCOMPARE(overview->data(overview->index(0, 1),
                            LoggerOverviewTableModel::ValueRole).toString(),
             QStringLiteral("7.25"));
}

// Dashboard summary mode draws one mini chart per ANALOG sensor. This covers
// the C++ side: which sensors get a tile, the threshold flags used to tint it,
// and the data-driven axis window that keeps the trend filling the tile.
void TestDashboardController::summaryBuildsPerSensorMiniCharts()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("mini"), QStringLiteral(":memory:")));

    DashboardController dash{nullptr};
    LoggerFormController form{nullptr};
    SettingsController settings{nullptr};

    dash.setDatabase(&db);
    form.setDatabase(&db);
    form.setDashboardController(&dash);
    settings.setDatabase(&db);
    settings.load();
    dash.setSettingsController(&settings);

    const qint64 loggerId = form.addLogger(QStringLiteral("TRAM-M1"),
                                           QStringLiteral("Mini"),
                                           QStringLiteral("192.168.1.12"),
                                           5020, 8080, QString());
    QVERIFY2(loggerId > 0, qPrintable(form.lastError()));

    SensorCatalogRepository catalog(db.connection());
    auto addSensor = [&catalog, loggerId](int edgeId, const char *type,
                                          const QString &name, const QString &unit,
                                          std::optional<double> min,
                                          std::optional<double> max,
                                          bool active) {
        LoggerSensor s;
        s.loggerId     = loggerId;
        s.edgeSensorId = edgeId;
        s.sensorType   = QString::fromLatin1(type);
        s.name         = name;
        s.unit         = unit;
        s.minThreshold = min;
        s.maxThreshold = max;
        s.decimals     = 2;
        s.active       = active;
        QVERIFY(catalog.upsert(s));
    };
    addSensor(1, "ANALOG", QStringLiteral("pH"), QStringLiteral("pH"),
              6.0, 9.0, true);
    addSensor(2, "ANALOG", QStringLiteral("TSS"), QStringLiteral("mg/L"),
              0.0, 100.0, false); // inactive → no tile
    addSensor(3, "ANALOG", QStringLiteral("COD"), QStringLiteral("mg/L"),
              std::nullopt, std::nullopt, true); // no thresholds → neutral tile
    addSensor(4, "ANALOG", QStringLiteral("Độ đục"), QStringLiteral("NTU"),
              std::nullopt, 100.0, true); // distant max → off-scale marker
    addSensor(0, "DI", QStringLiteral("Bơm"), QStringLiteral(""),
              std::nullopt, std::nullopt, true); // digital → no tile

    settings.setPrimaryLoggerId(loggerId);
    QCOMPARE(dash.primaryLoggerId(), loggerId);

    PollSnapshot snap;
    snap.loggerId   = loggerId;
    snap.success    = true;
    snap.measuredAt = QDateTime::currentDateTimeUtc();
    AnalogSample outOfRange;
    outOfRange.edgeSensorId = 1;
    outOfRange.statusCode   = QLatin1String("00"); // valid
    outOfRange.value        = 9.5f; // >= max (9.0)
    snap.analogs.append(outOfRange);
    AnalogSample noThresholds;
    noThresholds.edgeSensorId = 3;
    noThresholds.statusCode   = QLatin1String("00");
    noThresholds.value        = 42.0f;
    snap.analogs.append(noThresholds);
    AnalogSample distantLimit;
    distantLimit.edgeSensorId = 4;
    distantLimit.statusCode   = QLatin1String("00");
    distantLimit.value        = 28.0f; // max 100 is far above the data
    snap.analogs.append(distantLimit);

    dash.onSnapshotApplied(snap, 1, catalog.listByLoggerId(loggerId));

    // One tile per active ANALOG sensor, ordered by edge_sensor_id.
    const QVariantList charts = dash.graphSensorCharts();
    QCOMPARE(charts.size(), 3);

    const QVariantMap chart = charts.at(0).toMap();
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartEdgeSensorId)).toInt(), 1);
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartLabel)).toString(),
             QStringLiteral("pH"));
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartUnit)).toString(),
             QStringLiteral("pH"));
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartLatestText)).toString(),
             QStringLiteral("9.50"));
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartHasThresholds)).toBool(),
             true);
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartHasMaxThreshold)).toBool(),
             true);
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartThresholdMax)).toDouble(),
             9.0);
    QCOMPARE(chart.value(QLatin1String(CentralLogger::Ui::kChartOutOfRange)).toBool(),
             true);
    // The window is data-driven so the trend fills the tile. The only sample
    // is 9.5, far above the 9.0 limit, so the limit is off-scale (the tile
    // shows a marker) instead of stretching the axis down to it.
    QVERIFY(chart.value(QLatin1String(CentralLogger::Ui::kChartAxisYMax)).toDouble() > 9.5);
    QVERIFY(chart.value(QLatin1String(CentralLogger::Ui::kChartAxisYMin)).toDouble() < 9.5);
    QVERIFY(chart.value(QLatin1String(CentralLogger::Ui::kChartAxisYMin)).toDouble() > 9.0);
    QVERIFY(!chart.value(QLatin1String(CentralLogger::Ui::kChartPoints))
                 .toList()
                 .isEmpty());

    // A sensor without thresholds stays neutral (no tint, no yellow line).
    const QVariantMap neutral = charts.at(1).toMap();
    QCOMPARE(neutral.value(QLatin1String(CentralLogger::Ui::kChartEdgeSensorId)).toInt(), 3);
    QCOMPARE(neutral.value(QLatin1String(CentralLogger::Ui::kChartHasThresholds)).toBool(),
             false);
    QCOMPARE(neutral.value(QLatin1String(CentralLogger::Ui::kChartHasMaxThreshold)).toBool(),
             false);
    QCOMPARE(neutral.value(QLatin1String(CentralLogger::Ui::kChartOutOfRange)).toBool(),
             false);

    // A max limit far above the readings must not stretch the axis: the
    // trend keeps the full tile height and the limit becomes a marker.
    const QVariantMap distant = charts.at(2).toMap();
    QCOMPARE(distant.value(QLatin1String(CentralLogger::Ui::kChartEdgeSensorId)).toInt(), 4);
    QCOMPARE(distant.value(QLatin1String(CentralLogger::Ui::kChartHasMaxThreshold)).toBool(),
             true);
    const double distantAxisMax =
        distant.value(QLatin1String(CentralLogger::Ui::kChartAxisYMax)).toDouble();
    QVERIFY(distantAxisMax > 28.0);
    QVERIFY(distantAxisMax < 40.0); // not stretched up to the 100 limit
}

// The summary "Show graph" dropdown must switch both the mini-chart tiles and
// the "Other loggers" table (the selected logger is excluded from the table).
void TestDashboardController::summaryGraphLoggerDropdownSwitchesChartAndOverview()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("graphpick"), QStringLiteral(":memory:")));

    DashboardController dash{nullptr};
    LoggerFormController form{nullptr};
    SettingsController settings{nullptr};

    dash.setDatabase(&db);
    form.setDatabase(&db);
    form.setDashboardController(&dash);
    settings.setDatabase(&db);
    settings.load();
    dash.setSettingsController(&settings);

    const qint64 firstId = form.addLogger(QStringLiteral("TRAM-G1"),
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("192.168.1.20"),
                                          5020, 8080, QString());
    const qint64 secondId = form.addLogger(QStringLiteral("TRAM-G2"),
                                           QStringLiteral("Beta"),
                                           QStringLiteral("192.168.1.21"),
                                           5020, 8080, QString());
    QVERIFY2(firstId > 0, qPrintable(form.lastError()));
    QVERIFY2(secondId > 0, qPrintable(form.lastError()));

    SensorCatalogRepository catalog(db.connection());
    auto addAnalog = [&catalog](qint64 loggerId, const QString &name) {
        LoggerSensor s;
        s.loggerId     = loggerId;
        s.edgeSensorId = 1;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = name;
        s.unit         = QStringLiteral("mg/L");
        s.decimals     = 2;
        QVERIFY(catalog.upsert(s));
    };
    addAnalog(firstId, QStringLiteral("COD"));
    addAnalog(secondId, QStringLiteral("TSS"));

    settings.setPrimaryLoggerId(firstId);
    QCOMPARE(dash.primaryLoggerId(), firstId);
    QCOMPARE(dash.graphLoggerId(), firstId); // defaults to primary
    QCOMPARE(dash.graphLoggerName(), QStringLiteral("Alpha"));

    // Dropdown lists every configured logger, not just the secondary ones.
    const QVariantList options = dash.graphLoggerOptions();
    QCOMPARE(options.size(), 2);
    QCOMPARE(options.at(0).toMap().value(QStringLiteral("id")).toLongLong(), firstId);
    QCOMPARE(options.at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Alpha"));
    QCOMPARE(options.at(1).toMap().value(QStringLiteral("id")).toLongLong(), secondId);

    auto makeSnapshot = [](qint64 loggerId, float value) {
        PollSnapshot snap;
        snap.loggerId   = loggerId;
        snap.success    = true;
        snap.measuredAt = QDateTime::currentDateTimeUtc();
        AnalogSample analog;
        analog.edgeSensorId = 1;
        analog.statusCode   = QLatin1String("00"); // valid
        analog.value        = value;
        snap.analogs.append(analog);
        return snap;
    };
    dash.onSnapshotApplied(makeSnapshot(firstId, 11.0f), 1,
                           catalog.listByLoggerId(firstId));
    dash.onSnapshotApplied(makeSnapshot(secondId, 22.0f), 1,
                           catalog.listByLoggerId(secondId));

    // Default: Alpha is graphed, Beta is the only overview row.
    auto *overview = dash.overviewTable();
    QCOMPARE(overview->rowCount(), 1);
    QCOMPARE(overview->data(overview->index(0, 0)).toString(),
             QStringLiteral("Beta"));
    QCOMPARE(dash.graphSensorCharts().size(), 1);
    QCOMPARE(dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLabel)).toString(),
             QStringLiteral("COD"));

    // Picking Beta moves the graph to Beta and swaps the overview to Alpha.
    dash.setGraphLoggerId(secondId);
    QCOMPARE(dash.primaryLoggerId(), firstId); // primary setting unchanged
    QCOMPARE(dash.graphLoggerId(), secondId);
    QCOMPARE(dash.graphLoggerName(), QStringLiteral("Beta"));
    QCOMPARE(dash.graphSensorCharts().size(), 1);
    QCOMPARE(dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLabel)).toString(),
             QStringLiteral("TSS"));
    QCOMPARE(overview->rowCount(), 1);
    QCOMPARE(overview->data(overview->index(0, 0)).toString(),
             QStringLiteral("Alpha"));

    // An explicit pick survives a primary-logger setting change.
    settings.setPrimaryLoggerId(secondId);
    QCOMPARE(dash.primaryLoggerId(), secondId);
    QCOMPARE(dash.graphLoggerId(), secondId);

    // Removing the selected logger falls back to the configured primary
    // logger and clears the explicit selection.
    QVERIFY(form.removeLogger(secondId));
    QCOMPARE(dash.graphLoggerId(), firstId);
    QCOMPARE(dash.graphLoggerName(), QStringLiteral("Alpha"));
    QCOMPARE(dash.graphLoggerOptions().size(), 1);
}

QTEST_MAIN(TestDashboardController)
#include "test_dashboard_controller.moc"
