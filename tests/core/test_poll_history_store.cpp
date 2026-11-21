#include "core/charts/PollHistoryStore.h"
#include "network/modbus/ModbusTypes.h"

#include <QTest>

using CentralLogger::Core::PollHistoryStore;
using CentralLogger::Network::AnalogSample;
using CentralLogger::Network::PollSnapshot;

class TestPollHistoryStore : public QObject
{
    Q_OBJECT

private:
    /// Helper to build a successful PollSnapshot with N analog samples.
    static PollSnapshot makeSnapshot(qint64 loggerId,
                                     const QVector<QPair<int, float>> &analogs,
                                     const QDateTime &ts = QDateTime::currentDateTimeUtc())
    {
        PollSnapshot snap;
        snap.loggerId  = loggerId;
        snap.success   = true;
        snap.measuredAt = ts;
        snap.header.mapVersion   = 1;
        snap.header.statusFlags  = 0x01; // polling
        snap.header.na           = analogs.size();
        for (const auto &[id, val] : analogs) {
            AnalogSample s;
            s.edgeSensorId = id;
            s.statusCode   = QLatin1String("00");
            s.value        = val;
            snap.analogs.append(s);
        }
        return snap;
    }

private slots:
    void emptyByDefault()
    {
        PollHistoryStore store;
        QVERIFY(!store.has(1));
        QCOMPARE(store.loggerCount(), 0);
        QCOMPARE(store.pointCount(1, 1), 0);
        QVERIFY(store.seriesForLogger(1).isEmpty());
    }

    void appendSingleSnapshot()
    {
        PollHistoryStore store;
        const auto snap = makeSnapshot(10, {{1, 25.5f}, {2, 30.0f}});
        store.append(snap);

        QVERIFY(store.has(10));
        QCOMPARE(store.loggerCount(), 1);
        QCOMPARE(store.pointCount(10, 1), 1);
        QCOMPARE(store.pointCount(10, 2), 1);

        const auto series = store.seriesForLogger(10);
        QCOMPARE(series.size(), 2);
    }

    void appendMultipleSnapshots()
    {
        PollHistoryStore store;
        for (int i = 0; i < 5; ++i) {
            const auto snap = makeSnapshot(10, {{1, 20.0f + i}},
                QDateTime::currentDateTimeUtc().addSecs(i * 5));
            store.append(snap);
        }

        QCOMPARE(store.pointCount(10, 1), 5);

        const auto series = store.seriesForLogger(10);
        QCOMPARE(series.size(), 1);

        QVERIFY(series.contains(1));
        const auto points = series.value(1);
        QCOMPARE(points.size(), 5);

        // Verify ascending X (epoch ms)
        for (int i = 1; i < 5; ++i) {
            QVERIFY(points.at(i).timestampMs > points.at(i - 1).timestampMs);
        }

        // Verify y values ascending
        QCOMPARE(points.at(0).value, 20.0);
        QCOMPARE(points.at(4).value, 24.0);
    }

    void ringBufferCapsAtMemoryLimit()
    {
        PollHistoryStore store;
        const auto start = QDateTime::currentDateTimeUtc();
        for (int i = 0; i < PollHistoryStore::kMaxPoints + 10; ++i) {
            const auto snap = makeSnapshot(10, {{1, static_cast<float>(i)}},
                start.addSecs(i));
            store.append(snap);
        }

        QCOMPARE(store.pointCount(10, 1), PollHistoryStore::kMaxPoints);

        // The oldest 10 entries (0..9) should have been evicted.
        const auto series = store.seriesForLogger(10);
        const auto points = series.value(1);
        QCOMPARE(points.size(), PollHistoryStore::kMaxPoints);
        QCOMPARE(points.constFirst().value, 10.0);
        QCOMPARE(points.constLast().value, PollHistoryStore::kMaxPoints + 9.0);
        QVERIFY(!store.coversStart(10, start.toMSecsSinceEpoch()));
        QCOMPARE(store.latestEvictedTimestamp(10), start.addSecs(9).toMSecsSinceEpoch());
    }

    void ageLimitAndWindowFiltering()
    {
        using CentralLogger::Defaults::kGraphRamMaxMs;
        PollHistoryStore store;
        const auto now = QDateTime::currentDateTimeUtc();
        const qint64 ms = now.toMSecsSinceEpoch();
        store.append(makeSnapshot(10, {{1, 9}}, now.addMSecs(-kGraphRamMaxMs - 1)));
        store.append(makeSnapshot(10, {{1, 10}}, now.addMSecs(-kGraphRamMaxMs)));
        store.append(makeSnapshot(10, {{1, 20}}, now.addMSecs(-1000)));
        store.append(makeSnapshot(10, {{1, 30}}, now));
        QCOMPARE(store.pointCount(10, 1), 3);
        QVERIFY(store.coversStart(10, ms - kGraphRamMaxMs));
        QVERIFY(!store.coversStart(10, ms - kGraphRamMaxMs - 1));
        const auto points = store.seriesForLogger(10, ms - 1000, ms - 1).value(1);
        QCOMPARE(points.size(), 1);
        QCOMPARE(points.constFirst().value, 20.0);
        QVERIFY(store.seriesForLogger(10, ms + 1).isEmpty());
    }

    void failedSnapshotIgnored()
    {
        PollHistoryStore store;
        PollSnapshot snap;
        snap.loggerId = 10;
        snap.success  = false; // failed poll
        AnalogSample s;
        s.edgeSensorId = 1;
        s.statusCode   = QLatin1String("00");
        s.value        = 99.0f;
        snap.analogs.append(s);

        store.append(snap);
        QVERIFY(!store.has(10));
        QCOMPARE(store.pointCount(10, 1), 0);
    }

    void invalidSampleSkipped()
    {
        PollHistoryStore store;
        PollSnapshot snap;
        snap.loggerId  = 10;
        snap.success   = true;
        snap.measuredAt = QDateTime::currentDateTimeUtc();
        snap.header.mapVersion = 1;
        snap.header.na = 1;

        AnalogSample s;
        s.edgeSensorId = 1;
        s.statusCode   = QString(); // empty = invalid/stale
        s.value        = 42.0f;
        snap.analogs.append(s);

        store.append(snap);
        QCOMPARE(store.pointCount(10, 1), 0);
    }

    void errorDoesNotKeepLiveChartGreen()
    {
        PollHistoryStore store;
        auto snap = makeSnapshot(10, {{1, 42.0f}});
        store.append(snap);
        QVERIFY(store.currentValue(10, 1).has_value());
        snap.analogs[0].statusCode = QStringLiteral("02");
        snap.analogs[0].hasQuality = true;
        snap.analogs[0].qualityValid = false;
        store.append(snap);
        QCOMPARE(store.pointCount(10, 1), 1);
        QVERIFY(!store.currentValue(10, 1).has_value());
        snap.success = false;
        store.append(snap);
        QVERIFY(!store.currentValue(10, 1).has_value());
    }

    void staleSampleSkipped()
    {
        PollHistoryStore store;
        auto snap = makeSnapshot(10, {{1, 42.0f}});
        snap.analogs[0].statusCode = QString(); // empty = stale
        store.append(snap);
        QCOMPARE(store.pointCount(10, 1), 0);
    }

    void multipleLoggers()
    {
        PollHistoryStore store;
        store.append(makeSnapshot(10, {{1, 10.0f}}));
        store.append(makeSnapshot(20, {{1, 20.0f}, {2, 30.0f}}));

        QCOMPARE(store.loggerCount(), 2);
        QVERIFY(store.has(10));
        QVERIFY(store.has(20));
        QCOMPARE(store.pointCount(10, 1), 1);
        QCOMPARE(store.pointCount(20, 1), 1);
        QCOMPARE(store.pointCount(20, 2), 1);
    }

    void removeLogger()
    {
        PollHistoryStore store;
        store.append(makeSnapshot(10, {{1, 10.0f}}));
        store.append(makeSnapshot(20, {{1, 20.0f}}));

        store.remove(10);
        QVERIFY(!store.has(10));
        QVERIFY(store.has(20));
        QCOMPARE(store.loggerCount(), 1);
    }

    void clearAll()
    {
        PollHistoryStore store;
        store.append(makeSnapshot(10, {{1, 10.0f}}));
        store.append(makeSnapshot(20, {{1, 20.0f}}));

        store.clear();
        QCOMPARE(store.loggerCount(), 0);
    }

    void seriesFormatHasTimestampAndValue()
    {
        PollHistoryStore store;
        const QDateTime ts = QDateTime(QDate(2025, 6, 1), QTime(14, 30, 5), QTimeZone::utc());
        store.append(makeSnapshot(10, {{1, 25.5f}}, ts));

        const auto series = store.seriesForLogger(10);
        QCOMPARE(series.size(), 1);
        const auto points = series.value(1);
        QCOMPARE(points.size(), 1);
        const auto p0 = points.at(0);
        QCOMPARE(p0.timestampMs, ts.toMSecsSinceEpoch());
        QCOMPARE(p0.value, 25.5);
    }
};

QTEST_MAIN(TestPollHistoryStore)
#include "test_poll_history_store.moc"
