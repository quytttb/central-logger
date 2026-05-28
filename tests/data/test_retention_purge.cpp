#include "data/db/Database.h"
#include "data/repositories/SensorReadingRepository.h"
#include "data/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QTest>

using CentralLogger::Data::Database;
using CentralLogger::Data::SensorReading;
using CentralLogger::Data::SensorReadingRepository;
using CentralLogger::Data::SettingsRepository;

class TestRetentionPurge : public QObject
{
    Q_OBJECT

private:
    Database m_db;

    void seedFixtures()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral(
            "INSERT INTO logger_info (id, station_code, name, host) "
            "VALUES (1, 'ST-001', 'Test', '127.0.0.1')"));
        q.exec(QStringLiteral(
            "INSERT INTO logger_sensor (id, logger_id, edge_sensor_id, sensor_type, name, unit) "
            "VALUES (1, 1, 1, 'ANALOG', 'Temp', '°C')"));
    }

    void insertReading(qint64 sensorId, double value, const QDateTime &recordedAtUtc)
    {
        QSqlQuery q(m_db.connection());
        q.prepare(QStringLiteral(
            "INSERT INTO sensor_reading (sensor_id, value, valid, alarm, stale, "
            "logger_timestamp, recorded_at) VALUES (:sid, :val, 1, 0, 0, 0, :ts)"));
        q.bindValue(QStringLiteral(":sid"), sensorId);
        q.bindValue(QStringLiteral(":val"), value);
        q.bindValue(QStringLiteral(":ts"),
                     recordedAtUtc.toUTC().toString(Qt::ISODateWithMs));
        QVERIFY(q.exec());
    }

    int totalReadings()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral("SELECT COUNT(*) FROM sensor_reading"));
        q.next();
        return q.value(0).toInt();
    }

private slots:
    void initTestCase()
    {
        QString err;
        QVERIFY2(m_db.open(QStringLiteral("test_retention_purge"), QStringLiteral(":memory:"), &err),
                 qPrintable(err));
        seedFixtures();
    }

    void cleanup()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral("DELETE FROM sensor_reading"));
    }

    void purgeRemovesOldReadings()
    {
        // Insert a reading 40 days old.
        const QDateTime old = QDateTime::currentDateTimeUtc().addDays(-40);
        insertReading(1, 42.0, old);

        // Insert a fresh reading.
        const QDateTime now = QDateTime::currentDateTimeUtc();
        insertReading(1, 10.0, now);

        QCOMPARE(totalReadings(), 2);

        // Purge with 30-day retention.
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-30);
        SensorReadingRepository repo(m_db.connection());
        QString err;
        const int deleted = repo.purgeOlderThan(cutoff, &err);

        QCOMPARE(deleted, 1);
        QCOMPARE(totalReadings(), 1);
    }

    void purgeKeepsRecentReadings()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        insertReading(1, 1.0, now);
        insertReading(1, 2.0, now.addSecs(-3600));

        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-30);
        SensorReadingRepository repo(m_db.connection());
        const int deleted = repo.purgeOlderThan(cutoff);

        QCOMPARE(deleted, 0);
        QCOMPARE(totalReadings(), 2);
    }

    void purgeEmptyTable()
    {
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-1);
        SensorReadingRepository repo(m_db.connection());
        const int deleted = repo.purgeOlderThan(cutoff);
        QCOMPARE(deleted, 0);
    }

    void purgeAllOldReadings()
    {
        // Insert 5 readings all 60 days old.
        const QDateTime old = QDateTime::currentDateTimeUtc().addDays(-60);
        for (int i = 0; i < 5; ++i) {
            insertReading(1, static_cast<double>(i), old.addSecs(i));
        }
        QCOMPARE(totalReadings(), 5);

        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-30);
        SensorReadingRepository repo(m_db.connection());
        const int deleted = repo.purgeOlderThan(cutoff);
        QCOMPARE(deleted, 5);
        QCOMPARE(totalReadings(), 0);
    }

    void retentionDaysFromSettings()
    {
        // Verify that SettingsRepository returns the default 30 days.
        SettingsRepository settings(m_db.connection());
        const auto s = settings.get();
        QCOMPARE(s.dataRetentionDays, 30);

        // Update to 7 days and verify.
        auto updated = s;
        updated.dataRetentionDays = 7;
        QVERIFY(settings.update(updated));

        const auto reloaded = settings.get();
        QCOMPARE(reloaded.dataRetentionDays, 7);

        // Insert a reading 10 days old — should be purged with 7-day retention.
        const QDateTime tenDaysAgo = QDateTime::currentDateTimeUtc().addDays(-10);
        insertReading(1, 99.0, tenDaysAgo);

        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-reloaded.dataRetentionDays);
        SensorReadingRepository repo(m_db.connection());
        const int deleted = repo.purgeOlderThan(cutoff);
        QCOMPARE(deleted, 1);

        // Restore default.
        updated.dataRetentionDays = 30;
        settings.update(updated);
    }
};

QTEST_MAIN(TestRetentionPurge)

#include "test_retention_purge.moc"
