#include "core/charts/ChartPresentation.h"

#include <QTest>
#include <QTimeZone>

using namespace CentralLogger::Core;

class TestChartPresentation : public QObject
{
    Q_OBJECT

private slots:
    void buildReadingsChartPresentation_fixedWindowOfTwenty();
    void buildReadingsChartPresentation_fillsMissingBucketsWithZero();
    void snapReadingsChart_returnsNearestBucket();
    void snapTrendingChart_multiSeries();
    void computeTrendingAxisRange_padding();
};

void TestChartPresentation::buildReadingsChartPresentation_fixedWindowOfTwenty()
{
    QVariantList all;
    for (int i = 0; i < 25; ++i) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), QStringLiteral("t%1").arg(i));
        row.insert(QStringLiteral("bucketMs"), static_cast<qint64>(i) * 300'000);
        row.insert(QStringLiteral("count"), i + 1);
        all.append(row);
    }

    const qint64 anchorNowMs = 24 * 300'000 + 1;
    const auto pres = buildReadingsChartPresentation(
        all, 20, 5, QTimeZone::utc(), anchorNowMs);
    QCOMPARE(pres.plotPoints.size(), 20);
    QCOMPARE(pres.plotPoints.first().toMap().value(QStringLiteral("bucketMs")).toLongLong(),
             static_cast<qint64>(5 * 300'000));
    QCOMPARE(pres.plotPoints.last().toMap().value(QStringLiteral("count")).toInt(), 25);
    QVERIFY(pres.axis.value(QStringLiteral("yMax")).toDouble() >= 25.0 * 1.1);
}

void TestChartPresentation::buildReadingsChartPresentation_fillsMissingBucketsWithZero()
{
    QVariantList sparse;
    {
        QVariantMap row;
        row.insert(QStringLiteral("label"), QStringLiteral("t0"));
        row.insert(QStringLiteral("bucketMs"), static_cast<qint64>(19 * 300'000));
        row.insert(QStringLiteral("count"), 7);
        sparse.append(row);
    }

    const qint64 anchorNowMs = 19 * 300'000 + 1;
    const auto pres = buildReadingsChartPresentation(
        sparse, 20, 5, QTimeZone::utc(), anchorNowMs);
    QCOMPARE(pres.plotPoints.size(), 20);
    QCOMPARE(pres.plotPoints.last().toMap().value(QStringLiteral("count")).toInt(), 7);
    QCOMPARE(pres.plotPoints.first().toMap().value(QStringLiteral("count")).toInt(), 0);
}

void TestChartPresentation::snapReadingsChart_returnsNearestBucket()
{
    QVariantList plot;
    QVariantMap row;
    row.insert(QStringLiteral("label"), QStringLiteral("14:05"));
    row.insert(QStringLiteral("bucketMs"), static_cast<qint64>(1'700'000'000'000));
    row.insert(QStringLiteral("count"), 42);
    plot.append(row);

    const auto hit = snapReadingsChart(plot,
                                       1'699'999'000'000.0,
                                       1'700'001'000'000.0,
                                       10.0,
                                       10.0,
                                       100.0,
                                       80.0,
                                       60.0,
                                       50.0,
                                       5);
    QVERIFY(!hit.isEmpty());
    QCOMPARE(hit.value(QStringLiteral("captionText")).toString(), QStringLiteral("14:05"));
    QCOMPARE(hit.value(QStringLiteral("position")).toMap().value(QStringLiteral("y")).toInt(), 42);
}

void TestChartPresentation::snapTrendingChart_multiSeries()
{
    QVariantList series;
    {
        QVariantList pts;
        QVariantMap p;
        p.insert(QStringLiteral("x"), 1000.0);
        p.insert(QStringLiteral("y"), 1.5);
        p.insert(QStringLiteral("time"), QStringLiteral("10:00:00"));
        pts.append(p);
        QVariantMap s;
        s.insert(QStringLiteral("label"), QStringLiteral("A"));
        s.insert(QStringLiteral("points"), pts);
        series.append(s);
    }
    {
        QVariantList pts;
        QVariantMap p;
        p.insert(QStringLiteral("x"), 1000.0);
        p.insert(QStringLiteral("y"), 2.5);
        p.insert(QStringLiteral("time"), QStringLiteral("10:00:00"));
        pts.append(p);
        QVariantMap s;
        s.insert(QStringLiteral("label"), QStringLiteral("B"));
        s.insert(QStringLiteral("points"), pts);
        series.append(s);
    }

    const auto hit = snapTrendingChart(series, 900.0, 1100.0, 0.0, 0.0, 200.0, 100.0, 100.0, 50.0);
    QVERIFY(!hit.isEmpty());
    const auto rows = hit.value(QStringLiteral("valueRows")).toList();
    QCOMPARE(rows.size(), 2);
}

void TestChartPresentation::computeTrendingAxisRange_padding()
{
    QVariantList series;
    {
        QVariantList pts;
        QVariantMap p;
        p.insert(QStringLiteral("x"), 1000.0);
        p.insert(QStringLiteral("y"), 10.0);
        pts.append(p);
        p = QVariantMap();
        p.insert(QStringLiteral("x"), 2000.0);
        p.insert(QStringLiteral("y"), 20.0);
        pts.append(p);
        QVariantMap s;
        s.insert(QStringLiteral("points"), pts);
        series.append(s);
    }

    const auto range = computeTrendingAxisRange(series);
    QVERIFY(range.value(QStringLiteral("yMin")).toDouble() < 10.0);
    QVERIFY(range.value(QStringLiteral("yMax")).toDouble() > 20.0);
    QVERIFY(range.value(QStringLiteral("xMin")).toDouble() < 1000.0);
    QVERIFY(range.value(QStringLiteral("xMax")).toDouble() > 2000.0);
}

QTEST_APPLESS_MAIN(TestChartPresentation)
#include "test_chart_presentation.moc"
