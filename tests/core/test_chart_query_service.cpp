#include "core/charts/ChartQueryService.h"
#include "data/db/Database.h"
#include "data/repositories/SensorReadingRepository.h"

#include <QSqlQuery>
#include <QTest>
#include <QVector>

using CentralLogger::Core::ChartQueryService;
using CentralLogger::Core::ReadingBucketPoint;
using CentralLogger::Data::Database;
using CentralLogger::Data::SensorReading;
using CentralLogger::Data::SensorReadingRepository;

class TestChartQueryService : public QObject
{
    Q_OBJECT

private:
    Database m_db;

    /// Seed a logger_info + logger_sensor so FK constraints pass.
    void seedFixtures()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral(
            "INSERT INTO logger_info (id, station_code, name, host) "
            "VALUES (1, 'ST-001', 'Test Logger', '127.0.0.1')"));
        q.exec(QStringLiteral(
            "INSERT INTO logger_sensor (id, logger_id, edge_sensor_id, sensor_type, name, unit) "
            "VALUES (1, 1, 1, 'ANALOG', 'Temp', '°C')"));
    }

    void insertReading(qint64 sensorId, double value, const QString &recordedAtUtc)
    {
        QSqlQuery q(m_db.connection());
        q.prepare(QStringLiteral(
            "INSERT INTO sensor_reading (sensor_id, value, valid, alarm, stale, logger_timestamp, recorded_at) "
            "VALUES (:sid, :val, 1, 0, 0, 0, :ts)"));
        q.bindValue(QStringLiteral(":sid"), sensorId);
        q.bindValue(QStringLiteral(":val"), value);
        q.bindValue(QStringLiteral(":ts"),  recordedAtUtc);
        QVERIFY(q.exec());
    }

private slots:
    void initTestCase()
    {
        QString err;
        QVERIFY2(m_db.open(QStringLiteral("test_chart_query_service"), QStringLiteral(":memory:"), &err),
                 qPrintable(err));
        seedFixtures();
    }

    void cleanup()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral("DELETE FROM sensor_reading"));
    }

    void emptyTable_returnsEmpty()
    {
        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        QCOMPARE(pts.size(), 0);
    }

    void singleBucket_countsCorrectly()
    {
        // Insert 3 readings in the current minute.
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        insertReading(1, 10.0, now);
        insertReading(1, 11.0, now);
        insertReading(1, 12.0, now);

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        QVERIFY(!pts.isEmpty());
        // All 3 should be in a single bucket.
        int total = 0;
        for (const auto &pt : pts) total += pt.count;
        QCOMPARE(total, 3);
    }

    void multipleBuckets_sortedAscending()
    {
        // Insert readings 60 minutes apart — should produce separate buckets.
        const QDateTime base = QDateTime::currentDateTimeUtc().addSecs(-7200); // 2h ago
        for (int i = 0; i < 3; ++i) {
            const QDateTime ts = base.addSecs(i * 3600);
            insertReading(1, static_cast<double>(i),
                          ts.toString(Qt::ISODateWithMs));
        }

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        QVERIFY(pts.size() >= 3);
        // Labels should be ascending.
        for (int i = 1; i < pts.size(); ++i) {
            QVERIFY2(pts[i].label >= pts[i-1].label,
                     qPrintable(QStringLiteral("Out of order at %1: '%2' < '%3'")
                                    .arg(i).arg(pts[i].label, pts[i-1].label)));
        }
    }

    void bucketMinutes_respectsParameter()
    {
        // With bucketMinutes=60 (1 hour), 3 readings in the same hour → 1 bucket.
        const QDateTime now = QDateTime::currentDateTimeUtc();
        insertReading(1, 1.0, now.addSecs(-30).toString(Qt::ISODateWithMs));
        insertReading(1, 2.0, now.addSecs(-60).toString(Qt::ISODateWithMs));
        insertReading(1, 3.0, now.addSecs(-90).toString(Qt::ISODateWithMs));

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(60);
        QVERIFY(!pts.isEmpty());
        // All 3 readings should fall in the same 60-min bucket.
        int total = 0;
        for (const auto &pt : pts) total += pt.count;
        QCOMPARE(total, 3);
        // With 60-min buckets all in the same hour, there should be 1 bucket.
        QCOMPARE(pts.size(), 1);
    }

    void olderThan24h_excluded()
    {
        // Insert a reading 25 hours ago — should not appear.
        const QDateTime old = QDateTime::currentDateTimeUtc().addSecs(-25 * 3600);
        insertReading(1, 99.0, old.toString(Qt::ISODateWithMs));

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        // Old reading should be excluded.
        int total = 0;
        for (const auto &pt : pts) total += pt.count;
        QCOMPARE(total, 0);
    }

    void labelFormat_isHHMM()
    {
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        insertReading(1, 5.0, now);

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        QVERIFY(!pts.isEmpty());
        // Label should match HH:MM format.
        QRegularExpression re(QStringLiteral(R"(^\d{2}:\d{2}$)"));
        QVERIFY2(re.match(pts[0].label).hasMatch(),
                 qPrintable(QStringLiteral("Bad label format: '%1'").arg(pts[0].label)));
    }

    void bucketMs_ascendingAndPositive()
    {
        const QDateTime base = QDateTime::currentDateTimeUtc().addSecs(-3600);
        insertReading(1, 1.0, base.toString(Qt::ISODateWithMs));
        insertReading(1, 2.0, base.addSecs(600).toString(Qt::ISODateWithMs));

        ChartQueryService svc(m_db.connection());
        const auto pts = svc.readingCountsLast24h(5);
        QVERIFY(pts.size() >= 2);
        for (const auto &pt : pts) {
            QVERIFY(pt.bucketMs > 0);
        }
        for (int i = 1; i < pts.size(); ++i) {
            QVERIFY(pts[i].bucketMs >= pts[i - 1].bucketMs);
        }
    }
};

QTEST_MAIN(TestChartQueryService)

#include "test_chart_query_service.moc"
