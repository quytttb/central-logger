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
            s.flags        = 0x01; // valid
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

        const auto s0 = series.at(0).toMap();
        QCOMPARE(s0.value("edgeSensorId").toInt(), 1);
        const auto points = s0.value("points").toList();
        QCOMPARE(points.size(), 5);

        // Verify ascending X (epoch ms)
        for (int i = 1; i < 5; ++i) {
            QVERIFY(points.at(i).toMap().value("x").toDouble()
                    > points.at(i - 1).toMap().value("x").toDouble());
        }

        // Verify y values ascending
        QCOMPARE(points.at(0).toMap().value("y").toDouble(), 20.0);
        QCOMPARE(points.at(4).toMap().value("y").toDouble(), 24.0);
    }

    void ringBufferCapsAtDisplayLimit()
    {
        PollHistoryStore store;
        for (int i = 0; i < 30; ++i) {
            const auto snap = makeSnapshot(10, {{1, static_cast<float>(i)}},
                QDateTime::currentDateTimeUtc().addSecs(i));
            store.append(snap);
        }

        QCOMPARE(store.pointCount(10, 1), PollHistoryStore::kMaxPoints);

        // The oldest 10 entries (0..9) should have been evicted.
        const auto series = store.seriesForLogger(10);
        const auto points = series.at(0).toMap().value("points").toList();
        QCOMPARE(points.size(), 20);
        QCOMPARE(points.at(0).toMap().value("y").toDouble(), 10.0);
        QCOMPARE(points.at(19).toMap().value("y").toDouble(), 29.0);
    }

    void failedSnapshotIgnored()
    {
        PollHistoryStore store;
        PollSnapshot snap;
        snap.loggerId = 10;
        snap.success  = false; // failed poll
        AnalogSample s;
        s.edgeSensorId = 1;
        s.flags = 0x01;
        s.value = 99.0f;
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
        s.flags = 0x00; // invalid flag
        s.value = 42.0f;
        snap.analogs.append(s);

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

    void seriesFormatHasTimeField()
    {
        PollHistoryStore store;
        const QDateTime ts = QDateTime(QDate(2025, 6, 1), QTime(14, 30, 5), QTimeZone::utc());
        store.append(makeSnapshot(10, {{1, 25.5f}}, ts));

        const auto series = store.seriesForLogger(10);
        QCOMPARE(series.size(), 1);
        const auto s0 = series.at(0).toMap();
        const auto points = s0.value("points").toList();
        QCOMPARE(points.size(), 1);
        const auto p0 = points.at(0).toMap();
        QVERIFY(p0.contains("time"));
        QCOMPARE(p0.value("x").toLongLong(), ts.toMSecsSinceEpoch());
        // time field should be non-empty HH:mm:ss format
        const QString timeStr = p0.value("time").toString();
        QVERIFY(!timeStr.isEmpty());
        QVERIFY(timeStr.contains(':')); // basic sanity
    }
};

QTEST_MAIN(TestPollHistoryStore)
#include "test_poll_history_store.moc"
