#include "core/charts/Downsampler.h"

#include <QTest>
#include <algorithm>

using namespace CentralLogger::Core;

class TestDownsampler : public QObject {
    Q_OBJECT

private slots:
    void keepsExtremaAndAlarmInChronologicalOrder()
    {
        Downsampler sampler(0, 99, 8);
        for (int i = 0; i < 100; ++i)
            sampler.append({i, i == 41 ? -80.0 : i == 46 ? 75.0 : 20.0, i == 42});
        const auto points = sampler.takePoints();
        QCOMPARE(points.constFirst().timestampMs, 0);
        QCOMPARE(points.constLast().timestampMs, 99);
        QVERIFY(points.size() <= 9); // budget plus alarm point
        for (int i : {41, 42, 46}) {
            QVERIFY(std::any_of(points.cbegin(), points.cend(), [i](const auto &p) {
                return p.timestampMs == i;
            }));
        }
        QVERIFY(std::is_sorted(points.cbegin(), points.cend(), [](const auto &a, const auto &b) {
            return a.timestampMs < b.timestampMs;
        }));
    }

    void smallSeriesAndOutsideWindow()
    {
        Downsampler sampler(10, 12);
        sampler.append({9, 9, false});
        sampler.append({10, 40, false});
        sampler.append({11, 22, true});
        sampler.append({12, 33, false});
        sampler.append({13, 100, false});
        const auto points = sampler.takePoints();
        QCOMPARE(points.size(), 3);
        QCOMPARE(points[0].value, 40);
        QCOMPARE(points[1].value, 22);
        QVERIFY(points[1].alarm);
        QCOMPARE(points[2].value, 33);
    }
};

QTEST_GUILESS_MAIN(TestDownsampler)
#include "test_downsampler.moc"
