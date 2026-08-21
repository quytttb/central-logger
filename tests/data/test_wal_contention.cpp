#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/models/LoggerSensor.h"
#include "data/models/SensorReading.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"

#include <QDateTime>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace CentralLogger::Data;

/// Audit M-10 / #14: two separate connections to the SAME WAL file must be
/// able to write without one failing with SQLITE_BUSY. applyPerformancePragmas
/// sets busy_timeout=5000 on every connection, so a second writer that hits a
/// locked DB should block and then succeed instead of erroring instantly.
class TestWalContention : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;
    QString       m_dbPath;
    QString       m_connMain;
    QString       m_connSecond;
    QSqlDatabase  m_second;

    qint64 seedFixture(QSqlDatabase db)
    {
        LoggerRepository loggers(db);
        LoggerInfo info;
        info.stationCode = QStringLiteral("ST-WAL");
        info.name        = QStringLiteral("WAL Trạm");
        info.host        = QStringLiteral("127.0.0.1");
        if (!loggers.insert(info)) {
            return 0;
        }

        SensorCatalogRepository catalog(db);
        LoggerSensor s;
        s.loggerId     = info.id;
        s.edgeSensorId = 1;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = QStringLiteral("Temp");
        s.unit         = QStringLiteral("°C");
        if (!catalog.upsert(s)) {
            return 0;
        }
        return s.id;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmpDir.isValid());
        m_dbPath = m_tmpDir.filePath(QStringLiteral("wal_contention.db"));

        Database db;
        QString err;
        QVERIFY2(db.open(QStringLiteral("wal_main"), m_dbPath, &err),
                 qPrintable(err));
        m_connMain = db.connectionName();
        const qint64 sensorId = seedFixture(db.connection());
        QVERIFY(sensorId > 0);

        // Second connection: the history-writer style path. Open on the same
        // file with performance pragmas applied (this is what
        // HistoryWriterWorker::start does).
        m_connSecond = QStringLiteral("wal_second");
        m_second = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connSecond);
        m_second.setDatabaseName(m_dbPath);
        QVERIFY(m_second.open());
        QString pragmaErr;
        QVERIFY2(Database::applyPerformancePragmas(m_second, &pragmaErr),
                 qPrintable(pragmaErr));

        // Keep the Database alive for the test but release its own handle so
        // teardown below is explicit.
        db.close();
        m_connMain.clear();
    }

    void cleanupTestCase()
    {
        if (m_second.isValid()) {
            m_second.close();
            QSqlDatabase::removeDatabase(m_connSecond);
        }
    }

    void journalModeIsWal()
    {
        QSqlQuery q(m_second);
        QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString().toLower(), QStringLiteral("wal"));
    }

    void twoConnections_canConcurrentlyInsertReadings()
    {
        // Reader connection stays open + idle; the writer connection runs a
        // batch insert. Because both share the WAL file and busy_timeout is
        // set, the writer must not fail with SQLITE_BUSY.
        SensorReadingRepository repo(m_second);

        QVector<SensorReading> rows;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        for (int i = 0; i < 50; ++i) {
            SensorReading r;
            r.sensorId   = 1;
            r.value      = 20.0 + i * 0.1;
            r.valid      = true;
            r.recordedAt = now;
            rows.append(r);
        }

        QString err;
        const bool ok = repo.insertBatch(rows, &err, /*manageTransaction=*/true);
        QVERIFY2(ok, qPrintable(err));
    }

    void writerNotBlocked_byOpenReader()
    {
        // An open (but finished) query on another connection must not hold a
        // write lock that makes insertBatch fail.
        QSqlQuery reader(m_second);
        QVERIFY(reader.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sensor_reading")));

        QSqlDatabase conn = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QStringLiteral("wal_writer"));
        conn.setDatabaseName(m_dbPath);
        QVERIFY(conn.open());
        QString err;
        QVERIFY(Database::applyPerformancePragmas(conn, &err));

        SensorReadingRepository repo(conn);
        SensorReading r;
        r.sensorId   = 1;
        r.value      = 42.0;
        r.valid      = true;
        r.recordedAt = QDateTime::currentDateTimeUtc();
        QVERIFY2(repo.insertBatch({ r }, &err, /*manageTransaction=*/true),
                 qPrintable(err));

        reader.finish();
        conn.close();
        QSqlDatabase::removeDatabase(QStringLiteral("wal_writer"));
    }
};

QTEST_MAIN(TestWalContention)
#include "test_wal_contention.moc"
