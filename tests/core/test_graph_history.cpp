#include "core/DashboardController.h"
#include "core/charts/GraphHistoryQueryService.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"

#include <QTemporaryDir>
#include <QTest>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;
using namespace CentralLogger::Network;

class TestGraphHistory : public QObject {
    Q_OBJECT

private slots:
    void expandsIntoSqlAndMergesLiveWithoutReplacingChartModel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("graph.db"));
        Database db;
        QVERIFY(db.open(QStringLiteral("graph_test"), path));
        LoggerRepository loggerRepo(db.connection());
        LoggerInfo logger;
        logger.stationCode = QStringLiteral("GRAPH-1");
        logger.name = QStringLiteral("Logger 1");
        logger.host = QStringLiteral("127.0.0.1");
        QVERIFY(loggerRepo.insert(logger));
        LoggerInfo other = logger;
        other.stationCode = QStringLiteral("GRAPH-2");
        QVERIFY(loggerRepo.insert(other));

        SensorCatalogRepository catalog(db.connection());
        LoggerSensor analog;
        analog.loggerId = logger.id;
        analog.edgeSensorId = 1;
        analog.sensorType = QStringLiteral("ANALOG");
        analog.name = QStringLiteral("Graph sensor");
        QVERIFY(catalog.upsert(analog));
        LoggerSensor di = analog;
        di.edgeSensorId = 2;
        di.sensorType = QStringLiteral("DI");
        QVERIFY(catalog.upsert(di));
        LoggerSensor otherAnalog = analog;
        otherAnalog.loggerId = other.id;
        QVERIFY(catalog.upsert(otherAnalog));

        const auto now = QDateTime::currentDateTimeUtc();
        const auto old = now.addDays(-3);
        SensorReadingRepository readings(db.connection());
        QVERIFY(readings.insertBatch({
            {0, analog.id, 21.5, QStringLiteral("00"), 0, old},
            {0, analog.id, 99.0, QStringLiteral("02"), 0, old.addSecs(1)},
            {0, di.id, 1.0, QStringLiteral(""), 0, old},
            {0, otherAnalog.id, 0.5, QStringLiteral("00"), 0, old},
            {0, analog.id, 100.0, QStringLiteral("00"), 0, now.addDays(-9)},
        }));
        const std::atomic_bool cancelled(false);
        const auto sql = GraphHistoryQueryService::query(path, logger.id,
            now.addDays(-7).toMSecsSinceEpoch(), now.toMSecsSinceEpoch(), cancelled);
        QVERIFY2(sql.error.isEmpty(), qPrintable(sql.error));
        QCOMPARE(sql.series.size(), 1);
        QCOMPARE(sql.series.value(1).size(), 1);
        QCOMPARE(sql.series.value(1).constFirst().value, 21.5);

        DashboardController dash(nullptr);
        dash.setDatabase(&db);
        dash.reloadLoggers();
        dash.setGraphLoggerId(logger.id);
        PollSnapshot snap;
        snap.loggerId = logger.id;
        snap.success = true;
        snap.measuredAt = now;
        snap.analogs.append({1, QStringLiteral("00"), 30.0f});
        dash.onSnapshotApplied(snap, 1, catalog.listByLoggerId(logger.id));
        QCOMPARE(dash.graphWindowMinutes(), 5);
        QCOMPARE(dash.graphCharts()->rowCount(), 1);
        QPersistentModelIndex chart(dash.graphCharts()->index(0));
        const auto span = [&dash] {
            const auto axis = dash.graphChartAxis();
            return axis.value(QStringLiteral("xMax")).toLongLong()
                - axis.value(QStringLiteral("xMin")).toLongLong();
        };

        while (dash.canZoomGraphOut())
            dash.zoomGraphOut();
        QCOMPARE(dash.graphWindowMinutes(), 7 * 24 * 60);
        // Don't clear the current renderer while the outstanding query is cancelled.
        QCOMPARE(span(), 5 * 60 * 1000);
        QTRY_VERIFY(!dash.graphHistoryLoading());
        QCOMPARE(span(), 7LL * 24 * 60 * 60 * 1000);
        QVERIFY(chart.isValid());
        const auto points = chart.data(GraphChartsModel::ChartRole).toMap()
            .value(QStringLiteral("points")).toList();
        QCOMPARE(points.size(), 2);
        QCOMPARE(points.constFirst().toMap().value(QStringLiteral("y")).toDouble(), 21.5);
        QCOMPARE(points.constLast().toMap().value(QStringLiteral("y")).toDouble(), 30.0);

        dash.setGraphLoggerId(other.id);
        QVERIFY(!chart.isValid());
        QCOMPARE(dash.graphCharts()->rowCount(), 0); // no previous logger's curve
        dash.resetGraphZoom();
        QTRY_VERIFY(!dash.graphHistoryLoading());
        QCOMPARE(span(), 5 * 60 * 1000);
        QCOMPARE(dash.graphCharts()->rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestGraphHistory)
#include "test_graph_history.moc"
