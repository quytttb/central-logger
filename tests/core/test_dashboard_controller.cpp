#include "core/AppState.h"
#include "LoggerTestFixtures.h"
#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/LoggerListModel.h"
#include "core/SettingsController.h"
#include "core/sensors/LoggerOverviewTableModel.h"
#include "data/db/Database.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/EventRepository.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/modbus/ModbusTypes.h"
#include "utils/UiConstants.h"

#include <QDateTime>
#include <QSignalSpy>
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

    qint64 seedLogger(const QString &code, const QString &name,
                      const QString &host, int modbusPort, int apiPort,
                      const QString &token, const QString &note = {})
    {
        return LoggerTestFixtures::seedLogger(db, dash, code, name, host,
                                              modbusPort, apiPort, token, note);
    }
};

} // namespace

class TestDashboardController : public QObject
{
    Q_OBJECT

private slots:
    void seededLoggerUpdatesModel();
    void repositoryRejectsDuplicateStationCode();
    void saveWithoutProbeDoesNotMutateModel();
    void formHostValidation();
    void formDataReflectsRepositoryUpdate();
    void removeLoggerCascadesCatalog();
    void getLoggerFormDataMatchesInsert();
    void findAllWithSensorCountsCountsRows();
    void summaryResolvesPrimaryLoggerAndBuildsOverview();
    void summaryBuildsPerSensorMiniCharts();
    void summaryGraphLoggerDropdownSwitchesChartAndOverview();
    void setLoggerEnabledTogglesAndClearsLiveState();
    void liveSectionsAndLiveListTwoLoggersSimultaneous();
    void settingsLoggersViewModePersistsAcrossReload();
    void snapshotCacheStoresMeasuredAtAndClearsOnOffline();
    void overviewSeparatesSameIdWithDifferentMetadata();
    void listModelExposesPollingAndAlarmRoles();
};

void TestDashboardController::seededLoggerUpdatesModel()
{
    Harness h("add");

    QCOMPARE(h.dash.loggers()->rowCount(), 0);

    const qint64 id = h.seedLogger(QStringLiteral("TRAM-001"),
                                       QStringLiteral("Trạm test"),
                                       QStringLiteral("192.168.1.10"),
                                       5020, 8080,
                                       QStringLiteral("token"));
    QVERIFY(id > 0);
    QCOMPARE(h.dash.loggers()->rowCount(), 1);
    QCOMPARE(h.form.getLoggerFormData(id).value(QStringLiteral("name")).toString(),
             QStringLiteral("Trạm test"));
}

void TestDashboardController::repositoryRejectsDuplicateStationCode()
{
    Harness h("dupe");

    QVERIFY(h.seedLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("A"),
                             QStringLiteral("h"), 5020, 8080, {}) > 0);

    const qint64 second = h.seedLogger(QStringLiteral("TRAM-DUP"), QStringLiteral("B"),
                                           QStringLiteral("h"), 5020, 8080, {});
    QCOMPARE(second, qint64(-1));
    QCOMPARE(h.dash.loggers()->rowCount(), 1);
}

void TestDashboardController::saveWithoutProbeDoesNotMutateModel()
{
    Harness h("valid");

    QSignalSpy saved(&h.form, &LoggerFormController::formSaveFinished);
    h.form.saveLoggerFromForm(true, -1, QStringLiteral("X"), QStringLiteral("A"),
                              QStringLiteral("h"), 5020, 8080, {}, 1, 2, 5, true, {});
    QCOMPARE(saved.count(), 1);
    QVERIFY(!saved.takeFirst().at(0).toBool());
    QCOMPARE(h.dash.loggers()->rowCount(), 0);
}

void TestDashboardController::formHostValidation()
{
    Harness h("host");

    QVERIFY(!h.form.isValidHost(QStringLiteral("not a valid host!!!")));
    QVERIFY(!h.form.isValidHost(QStringLiteral("192.168.1")));
    QVERIFY(h.form.isValidHost(QStringLiteral("logger.local")));
}

void TestDashboardController::formDataReflectsRepositoryUpdate()
{
    Harness h("upd");

    const qint64 id = h.seedLogger(QStringLiteral("TRAM-U"), QStringLiteral("Old"),
                                       QStringLiteral("h1"), 5020, 8080,
                                       QStringLiteral("t1"));
    QVERIFY(id > 0);

    LoggerRepository repo(h.db.connection());
    auto info = repo.findById(id);
    QVERIFY(info.has_value());
    info->name = QStringLiteral("New");
    info->host = QStringLiteral("h2");
    info->modbusPort = 5030;
    info->apiPort = 9090;
    info->apiToken = QStringLiteral("t2");
    info->note = QStringLiteral("note edited");
    QVERIFY(repo.update(*info));
    h.dash.reloadLoggers();

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

    const qint64 id = h.seedLogger(QStringLiteral("TRAM-RM"), QStringLiteral("A"),
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

    const qint64 id = h.seedLogger(QStringLiteral("TRAM-F"), QStringLiteral("Form"),
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
    QCOMPARE(data.value(QStringLiteral("enabled")).toBool(),   true);

    QVERIFY(h.form.getLoggerFormData(9999).isEmpty());
}

void TestDashboardController::findAllWithSensorCountsCountsRows()
{
    Harness h("count");

    const qint64 a = h.seedLogger(QStringLiteral("CNT-A"), QStringLiteral("A"),
                                      QStringLiteral("h"), 5020, 8080, {});
    const qint64 b = h.seedLogger(QStringLiteral("CNT-B"), QStringLiteral("B"),
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

    const qint64 firstId = LoggerTestFixtures::seedLogger(db, dash, QStringLiteral("TRAM-S1"),
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("192.168.1.10"),
                                          5020, 8080, QString());
    const qint64 secondId = LoggerTestFixtures::seedLogger(db, dash, QStringLiteral("TRAM-S2"),
                                           QStringLiteral("Beta"),
                                           QStringLiteral("192.168.1.11"),
                                           5020, 8080, QString());
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);

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
    QCOMPARE(settings.primaryLoggerId(), secondId);
    QCOMPARE(dash.graphLoggerId(), secondId);

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
    QCOMPARE(dash.graphSensorCharts().size(), 1);
    QVERIFY(!dash.graphSensorCharts().first().toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartPoints)).toList().isEmpty());
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

    const qint64 loggerId = LoggerTestFixtures::seedLogger(db, dash, QStringLiteral("TRAM-M1"),
                                           QStringLiteral("Mini"),
                                           QStringLiteral("192.168.1.12"),
                                           5020, 8080, QString());
    QVERIFY(loggerId > 0);

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
    QCOMPARE(settings.primaryLoggerId(), loggerId);
    QCOMPARE(dash.graphLoggerId(), loggerId);

    // Single source of truth: edge qualityAlarm=false stays normal even when reading >= maxThreshold.
    PollSnapshot hysteresisSnap;
    hysteresisSnap.loggerId   = loggerId;
    hysteresisSnap.success    = true;
    hysteresisSnap.measuredAt = QDateTime::currentDateTimeUtc();
    AnalogSample hysteresisSample;
    hysteresisSample.edgeSensorId = 1;
    hysteresisSample.statusCode   = QLatin1String("00");
    hysteresisSample.hasQuality   = true;
    hysteresisSample.qualityValid = true;
    hysteresisSample.qualityAlarm = false;
    hysteresisSample.value        = 9.5f;
    hysteresisSnap.analogs.append(hysteresisSample);
    dash.onSnapshotApplied(hysteresisSnap, 1, catalog.listByLoggerId(loggerId));
    QCOMPARE(dash.graphSensorCharts().size(), 1);
    QCOMPARE(dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartOutOfRange)).toBool(),
             false);

    PollSnapshot snap;
    snap.loggerId   = loggerId;
    snap.success    = true;
    snap.measuredAt = hysteresisSnap.measuredAt.addSecs(2);
    AnalogSample outOfRange;
    outOfRange.edgeSensorId = 1;
    outOfRange.statusCode   = QLatin1String("00"); // valid
    outOfRange.hasQuality   = true;
    outOfRange.qualityValid = true;
    outOfRange.qualityAlarm = true;
    outOfRange.value        = 9.5f; // >= max (9.0) + edge alarm active
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

    const qint64 firstId = LoggerTestFixtures::seedLogger(db, dash, QStringLiteral("TRAM-G1"),
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("192.168.1.20"),
                                          5020, 8080, QString());
    const qint64 secondId = LoggerTestFixtures::seedLogger(db, dash, QStringLiteral("TRAM-G2"),
                                           QStringLiteral("Beta"),
                                           QStringLiteral("192.168.1.21"),
                                           5020, 8080, QString());
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);

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
    QCOMPARE(settings.primaryLoggerId(), firstId);
    QCOMPARE(dash.graphLoggerId(), firstId); // defaults to primary

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
    QCOMPARE(settings.primaryLoggerId(), firstId); // primary setting unchanged
    QCOMPARE(dash.graphLoggerId(), secondId);
    QCOMPARE(dash.graphSensorCharts().size(), 1);
    QCOMPARE(dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLabel)).toString(),
             QStringLiteral("TSS"));
    QCOMPARE(overview->rowCount(), 1);
    QCOMPARE(overview->data(overview->index(0, 0)).toString(),
             QStringLiteral("Alpha"));

    // An explicit pick survives a primary-logger setting change.
    settings.setPrimaryLoggerId(secondId);
    QCOMPARE(settings.primaryLoggerId(), secondId);
    QCOMPARE(dash.graphLoggerId(), secondId);

    // Removing the selected logger falls back to the configured primary
    // logger and clears the explicit selection.
    QVERIFY(form.removeLogger(secondId));
    QCOMPARE(dash.graphLoggerId(), firstId);
    QCOMPARE(dash.graphLoggerOptions().size(), 1);
}

void TestDashboardController::setLoggerEnabledTogglesAndClearsLiveState()
{
    Harness h("enabled_toggle");

    const qint64 id = h.seedLogger(QStringLiteral("TRAM-EN"),
                                       QStringLiteral("Toggle Logger"),
                                       QStringLiteral("192.168.1.30"),
                                       5020, 8080, {});
    QVERIFY(id > 0);

    SensorCatalogRepository catalog(h.db.connection());
    LoggerSensor s;
    s.loggerId     = id;
    s.edgeSensorId = 1;
    s.sensorType   = QStringLiteral("ANALOG");
    s.name         = QStringLiteral("Temp");
    s.unit         = QStringLiteral("C");
    s.decimals     = 1;
    QVERIFY(catalog.upsert(s));
    h.dash.reloadLoggers();

    PollSnapshot snap;
    snap.loggerId = id;
    snap.success  = true;
    snap.header.mapVersion  = 2;
    snap.header.statusFlags = 0x07; // polling + RTU + alarm
    snap.measuredAt = QDateTime::currentDateTimeUtc();
    AnalogSample sample;
    sample.edgeSensorId = 1;
    sample.statusCode   = QStringLiteral("00");
    sample.hasQuality   = true;
    sample.qualityValid = true;
    sample.qualityAlarm = true;
    sample.value        = 42.5f;
    snap.analogs = { sample };

    h.dash.onSnapshotApplied(snap, 1, catalog.listByLoggerId(id));
    QVERIFY(h.dash.isLoggerOnline(id));
    QCOMPARE(h.dash.graphSensorCharts().size(), 1);
    QCOMPARE(h.dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLatestText)).toString(),
             QStringLiteral("42.5"));
    QCOMPARE(LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("online")).toBool(), true);
    QCOMPARE(LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("anyAlarm")).toBool(), true);

    // Even if DB status was left 'online' for a disabled logger, countOnline must exclude disabled rows.
    LoggerRepository lr(h.db.connection());
    lr.updateStatus(id, CentralLogger::Sensor::kLoggerOnline);
    QVERIFY(h.form.setLoggerEnabled(id, false));
    QCOMPARE(lr.countOnline(), 0);
    QCOMPARE(h.dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLatestText)).toString(),
             QStringLiteral("—"));
    QCOMPARE(h.form.getLoggerFormData(id).value(QStringLiteral("enabled")).toBool(), false);
    QVERIFY(!h.dash.isLoggerOnline(id));
    QCOMPARE(LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("online")).toBool(), false);
    QCOMPARE(LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("anyAlarm")).toBool(), false);
    const QVariantList disabledSensors =
        LoggerTestFixtures::cardSection(h.dash, 0).value(QStringLiteral("sensors")).toList();
    QCOMPARE(disabledSensors.size(), 1);
    QCOMPARE(disabledSensors.at(0).toMap().value(QStringLiteral("value")).toString(),
             QStringLiteral("—"));

    // Reloading the model preserves the disabled state.
    h.dash.reloadLoggers();
    QCOMPARE(h.form.getLoggerFormData(id).value(QStringLiteral("enabled")).toBool(), false);

    // Late snapshot while disabled must be ignored.
    h.dash.onSnapshotApplied(snap, 1, catalog.listByLoggerId(id));
    QVERIFY(!h.dash.isLoggerOnline(id));

    // Re-enabling allows snapshots again — including Modbus Map v1 ("UNKNOWN" TT10 status).
    QVERIFY(h.form.setLoggerEnabled(id, true));
    QCOMPARE(h.form.getLoggerFormData(id).value(QStringLiteral("enabled")).toBool(), true);
    PollSnapshot v1Snap = snap;
    v1Snap.header.mapVersion = 1;
    v1Snap.measuredAt = snap.measuredAt.addSecs(2);
    v1Snap.analogs[0].statusCode = QLatin1String(CentralLogger::Sensor::kStatusUnknown);
    v1Snap.analogs[0].value = 43.0f;
    h.dash.onSnapshotApplied(v1Snap, 1, catalog.listByLoggerId(id));
    QVERIFY(h.dash.isLoggerOnline(id));
    QCOMPARE(h.dash.graphSensorCharts().at(0).toMap()
                 .value(QLatin1String(CentralLogger::Ui::kChartLatestText)).toString(),
             QStringLiteral("43.0"));
}

void TestDashboardController::liveSectionsAndLiveListTwoLoggersSimultaneous()
{
    Harness h("live_two");

    const qint64 alphaId = h.seedLogger(QStringLiteral("TRAM-L1"),
                                            QStringLiteral("Alpha"),
                                            QStringLiteral("192.168.1.40"),
                                            5020, 8080, {});
    const qint64 betaId = h.seedLogger(QStringLiteral("TRAM-L2"),
                                           QStringLiteral("Beta"),
                                           QStringLiteral("192.168.1.41"),
                                           5020, 8080, {});
    QVERIFY(alphaId > 0 && betaId > 0);

    SensorCatalogRepository catalog(h.db.connection());
    auto insertSensor = [&](qint64 loggerId, int edgeId, const QString &name,
                            const QString &unit, std::optional<double> minT,
                            std::optional<double> maxT) {
        LoggerSensor s;
        s.loggerId     = loggerId;
        s.edgeSensorId = edgeId;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = name;
        s.unit         = unit;
        s.minThreshold = minT;
        s.maxThreshold = maxT;
        s.decimals     = 2;
        QVERIFY(catalog.upsert(s));
    };

    insertSensor(alphaId, 1, QStringLiteral("pH"),  QStringLiteral("pH"),   6.0, 9.0);
    insertSensor(alphaId, 2, QStringLiteral("COD"), QStringLiteral("mg/L"), std::nullopt, std::nullopt);
    insertSensor(betaId,  1, QStringLiteral("O2"),  QStringLiteral("%V"),   0.0, 21.0);
    h.dash.reloadLoggers();

    // Alpha comes online with 2 analog readings (pH in alarm, COD without thresholds).
    PollSnapshot alphaSnap;
    alphaSnap.loggerId = alphaId;
    alphaSnap.success  = true;
    alphaSnap.header.mapVersion  = 2;
    alphaSnap.header.statusFlags = 0x07;
    alphaSnap.measuredAt = QDateTime::fromString(QStringLiteral("2026-11-18T04:15:30Z"),
                                                 Qt::ISODate);
    AnalogSample phSample;
    phSample.edgeSensorId = 1;
    phSample.statusCode   = QStringLiteral("00");
    phSample.hasQuality   = true;
    phSample.qualityValid = true;
    phSample.qualityAlarm = true;
    phSample.value        = 9.5f;

    AnalogSample codSample;
    codSample.edgeSensorId = 2;
    codSample.statusCode   = QStringLiteral("00");
    codSample.hasQuality   = true;
    codSample.qualityValid = true;
    codSample.qualityAlarm = false;
    codSample.value        = 35.0f;
    alphaSnap.analogs = { phSample, codSample };

    h.dash.onSnapshotApplied(alphaSnap, 2, catalog.listByLoggerId(alphaId));

    // Beta stays offline (failed snapshot).
    PollSnapshot betaSnap;
    betaSnap.loggerId = betaId;
    betaSnap.success  = false;
    h.dash.onSnapshotApplied(betaSnap, 1, catalog.listByLoggerId(betaId));

    QVariantList sections;
    for (int row = 0; row < h.dash.liveCards()->rowCount(); ++row)
        sections.append(LoggerTestFixtures::cardSection(h.dash, row));
    QCOMPARE(sections.size(), 2);

    const QVariantMap secAlpha = sections.at(0).toMap();
    QCOMPARE(secAlpha.value(QStringLiteral("loggerId")).toLongLong(), alphaId);
    QCOMPARE(secAlpha.value(QStringLiteral("name")).toString(), QStringLiteral("Alpha"));
    QCOMPARE(secAlpha.value(QStringLiteral("online")).toBool(), true);
    QCOMPARE(secAlpha.value(QStringLiteral("anyAlarm")).toBool(), true);
    QCOMPARE(secAlpha.value(QStringLiteral("sensorCount")).toInt(), 2);
    QVERIFY(!secAlpha.value(QStringLiteral("lastUpdate")).toString().isEmpty());

    const QVariantList alphaSensors = secAlpha.value(QStringLiteral("sensors")).toList();
    QCOMPARE(alphaSensors.size(), 2);
    const QVariantMap phMap = alphaSensors.at(0).toMap();
    QCOMPARE(phMap.value(QStringLiteral("value")).toString(), QStringLiteral("9.50"));
    QCOMPARE(phMap.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(phMap.value(QStringLiteral("hasLimit")).toBool(), true);
    QCOMPARE(phMap.value(QStringLiteral("alarmType")).toString(), QStringLiteral("max"));
    QCOMPARE(phMap.value(QStringLiteral("displayStatus")).toString(), QStringLiteral("00"));

    const QVariantMap codMap = alphaSensors.at(1).toMap();
    QCOMPARE(codMap.value(QStringLiteral("value")).toString(), QStringLiteral("35.00"));
    QCOMPARE(codMap.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(codMap.value(QStringLiteral("hasLimit")).toBool(), false);
    QVERIFY(codMap.value(QStringLiteral("limitText")).toString().isEmpty());

    const QVariantMap secBeta = sections.at(1).toMap();
    QCOMPARE(secBeta.value(QStringLiteral("loggerId")).toLongLong(), betaId);
    QCOMPARE(secBeta.value(QStringLiteral("online")).toBool(), false);
    QVERIFY(secBeta.value(QStringLiteral("lastUpdate")).toString().isEmpty());
    const QVariantList betaSensors = secBeta.value(QStringLiteral("sensors")).toList();
    QCOMPARE(betaSensors.size(), 1);
    QCOMPARE(betaSensors.at(0).toMap().value(QStringLiteral("value")).toString(),
             QStringLiteral("—"));
    QCOMPARE(betaSensors.at(0).toMap().value(QStringLiteral("valid")).toBool(), false);

    // Live cards & list models expose filtered entries and update in place.
    auto *liveCards = h.dash.liveCards();
    auto *liveList = h.dash.liveList();
    QVERIFY(liveCards);
    QVERIFY(liveList);
    QCOMPARE(liveCards->rowCount(), 2);
    QCOMPARE(liveList->columnCount(), 6);
    QCOMPARE(liveList->rowCount(), 3);
    QCOMPARE(liveList->data(liveList->index(0, LoggerLiveListModel::LoggerColumn)).toString(),
             QStringLiteral("Alpha"));
    QCOMPARE(liveList->data(liveList->index(0, LoggerLiveListModel::SensorColumn)).toString(),
             QStringLiteral("pH"));
    QCOMPARE(liveList->data(liveList->index(1, LoggerLiveListModel::LimitColumn)).toString(),
             QStringLiteral("—"));
    QCOMPARE(liveList->data(liveList->index(2, LoggerLiveListModel::ValueColumn)).toString(),
             QStringLiteral("—"));

    h.dash.setLiveFilterText(QStringLiteral("COD"));
    QCOMPARE(liveList->rowCount(), 1);
    QCOMPARE(liveList->data(liveList->index(0, LoggerLiveListModel::SensorColumn)).toString(),
             QStringLiteral("COD"));
    QCOMPARE(liveCards->rowCount(), 1);
    QCOMPARE(liveCards->data(liveCards->index(0, 0), LoggerLiveCardsModel::SensorCountRole).toInt(), 1);

    h.dash.setLiveFilterText(QString());
    QCOMPARE(liveList->rowCount(), 3);
    QCOMPARE(liveCards->rowCount(), 2);
}

void TestDashboardController::settingsLoggersViewModePersistsAcrossReload()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("view_mode"), QStringLiteral(":memory:")));

    SettingsController s1{nullptr};
    s1.setDatabase(&db);
    s1.load();
    QCOMPARE(s1.loggersViewMode(), QStringLiteral("cards"));

    QVERIFY(s1.saveLoggersViewMode(QStringLiteral("list")));
    QCOMPARE(s1.loggersViewMode(), QStringLiteral("list"));

    SettingsController s2{nullptr};
    s2.setDatabase(&db);
    s2.load();
    QCOMPARE(s2.loggersViewMode(), QStringLiteral("list"));

    // Invalid string normalizes back to "cards".
    QVERIFY(s2.saveLoggersViewMode(QStringLiteral("unknown-mode")));
    QCOMPARE(s2.loggersViewMode(), QStringLiteral("cards"));
}

void TestDashboardController::snapshotCacheStoresMeasuredAtAndClearsOnOffline()
{
    SensorSnapshotCache cache;
    LoggerSensor s;
    s.loggerId     = 1;
    s.edgeSensorId = 5;
    s.sensorType   = QStringLiteral("ANALOG");
    s.name         = QStringLiteral("Temp");
    s.unit         = QStringLiteral("C");

    PollSnapshot snap;
    snap.loggerId   = 1;
    snap.success    = true;
    snap.measuredAt = QDateTime::fromString(QStringLiteral("2026-05-24T03:14:15Z"), Qt::ISODate);
    AnalogSample a;
    a.edgeSensorId = 5;
    a.statusCode   = QStringLiteral("00");
    a.value        = 19.5f;
    snap.analogs   = { a };

    cache.apply(snap, { s });
    QVERIFY(cache.has(1));
    QCOMPARE(cache.measuredAtFor(1), snap.measuredAt);
    QCOMPARE(cache.rowsFor(1).first().value, QStringLiteral("19.5000"));

    // Failed snapshot is ignored by apply(); markOffline() hides cached values & clears measuredAt.
    PollSnapshot bad = snap;
    bad.success = false;
    cache.apply(bad, {});
    QCOMPARE(cache.rowsFor(1).first().value, QStringLiteral("19.5000"));

    cache.markOffline(1);
    QVERIFY(!cache.measuredAtFor(1).isValid());
    QCOMPARE(cache.rowsFor(1).first().value, QStringLiteral("—"));
    QVERIFY(!cache.rowsFor(1).first().valid);

    cache.remove(1);
    QVERIFY(!cache.has(1));
}

void TestDashboardController::overviewSeparatesSameIdWithDifferentMetadata()
{
    Harness h("overview_identity");
    const auto anchor = h.seedLogger(QStringLiteral("ANCHOR"),
                                         QStringLiteral("Anchor"), QStringLiteral("h"), 5020, 8080, {});
    const auto first = h.seedLogger(QStringLiteral("OSG-A"),
                                        QStringLiteral("OSG A"), QStringLiteral("h"), 5020, 8080, {});
    const auto second = h.seedLogger(QStringLiteral("OSG-B"),
                                         QStringLiteral("OSG B"), QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(anchor > 0 && first > 0 && second > 0);
    h.dash.setGraphLoggerId(anchor);

    auto makeSnap = [](qint64 loggerId, int edgeId, float val) {
        PollSnapshot snap;
        snap.loggerId   = loggerId;
        snap.success    = true;
        snap.measuredAt = QDateTime::currentDateTimeUtc();
        AnalogSample a;
        a.edgeSensorId = static_cast<quint16>(edgeId);
        a.statusCode   = QStringLiteral("00");
        a.value        = val;
        snap.analogs   = { a };
        return snap;
    };

    LoggerSensor oxygen;
    oxygen.loggerId = first;
    oxygen.edgeSensorId = 164;
    oxygen.sensorType = QStringLiteral("ANALOG");
    oxygen.name = QStringLiteral("O2");
    oxygen.unit = QStringLiteral("%V");
    h.dash.onSnapshotApplied(makeSnap(first, 164, 11.42f), 1, {oxygen});

    LoggerSensor nitrogen;
    nitrogen.loggerId = second;
    nitrogen.edgeSensorId = 164;
    nitrogen.sensorType = QStringLiteral("ANALOG");
    nitrogen.name = QStringLiteral("NO2");
    nitrogen.unit = QStringLiteral("mg/Nm³");
    h.dash.onSnapshotApplied(makeSnap(second, 164, 2.0f), 1, {nitrogen});

    auto *overview = h.dash.overviewTable();
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
}

void TestDashboardController::listModelExposesPollingAndAlarmRoles()
{
    Harness h("rtu_role");
    const qint64 loggerId = h.seedLogger(QStringLiteral("TRAM-RTU"),
                                             QStringLiteral("RTU"),
                                             QStringLiteral("h"), 5020, 8080, {});
    QVERIFY(loggerId > 0);

    PollSnapshot snap;
    snap.loggerId = loggerId;
    snap.success  = true;
    snap.header.mapVersion  = 1;
    snap.header.statusFlags = 0x02; // RTU connected only
    h.dash.onSnapshotApplied(snap, 0, {});

    auto *model = h.dash.loggers();
    const int row = model->indexOfLogger(loggerId);
    QVERIFY(row >= 0);
    const QModelIndex idx = model->index(row, 0);
    QCOMPARE(model->data(idx, LoggerListModel::OnlineRole).toBool(), true);
    QCOMPARE(model->data(idx, LoggerListModel::PollingRole).toBool(), false);
    QCOMPARE(model->data(idx, LoggerListModel::AnyAlarmRole).toBool(), false);
}

QTEST_MAIN(TestDashboardController)
#include "test_dashboard_controller.moc"
