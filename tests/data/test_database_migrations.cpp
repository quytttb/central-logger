#include "data/db/Database.h"

#include <QTemporaryDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

using namespace CentralLogger::Data;

namespace {

QString uniqueConnectionName(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("test_mig_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

bool execSql(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    return q.exec(sql);
}

bool tableHasRows(QSqlDatabase db, const QString &table)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT 1 FROM %1").arg(table))) {
        return false;
    }
    return q.next();
}

int userVersion(QSqlDatabase db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next()) {
        return -1;
    }
    return q.value(0).toInt();
}

/// Creates a legacy-format DB at @p legacyVersion: a couple of tables plus a
/// row in logger_info, stamped with the old PRAGMA user_version.
bool seedLegacyDb(const QString &dbPath, int legacyVersion)
{
    const QString connName = uniqueConnectionName("seed");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return false;
        }
        const QStringList statements = {
            QStringLiteral("PRAGMA foreign_keys = ON"),
            QStringLiteral(
                "CREATE TABLE logger_info ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "station_code TEXT NOT NULL UNIQUE,"
                "name TEXT NOT NULL,"
                "host TEXT NOT NULL,"
                "modbus_port INTEGER NOT NULL DEFAULT 5020,"
                "modbus_unit_id INTEGER NOT NULL DEFAULT 1,"
                "central_poll_interval_s INTEGER NOT NULL DEFAULT 2,"
                "timeout_s REAL NOT NULL DEFAULT 2.0,"
                "enabled INTEGER NOT NULL DEFAULT 1,"
                "api_port INTEGER NOT NULL DEFAULT 8080,"
                "api_token TEXT,"
                "last_revision INTEGER NOT NULL DEFAULT -1,"
                "status TEXT NOT NULL DEFAULT 'offline',"
                "last_seen TEXT,"
                "note TEXT,"
                "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))"
                ")"),
            QStringLiteral(
                "CREATE TABLE app_settings ("
                "id INTEGER PRIMARY KEY CHECK (id = 1),"
                "theme TEXT NOT NULL DEFAULT 'dark',"
                "system_timezone TEXT NOT NULL DEFAULT 'Asia/Ho_Chi_Minh',"
                "data_retention_days INTEGER NOT NULL DEFAULT 30,"
                "maintenance_mode INTEGER NOT NULL DEFAULT 0"
                ")"),
            QStringLiteral("INSERT INTO logger_info (station_code, name, host) "
                            "VALUES ('OLD-01', 'Legacy', '10.0.0.1')"),
            QStringLiteral("INSERT INTO app_settings (id) VALUES (1)"),
        };
        for (const QString &sql : statements) {
            if (!execSql(db, sql)) {
                db.close();
                return false;
            }
        }
        if (!execSql(db, QStringLiteral("PRAGMA user_version = %1").arg(legacyVersion))) {
            db.close();
            return false;
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return true;
}

} // namespace

class TestDatabaseMigrations : public QObject
{
    Q_OBJECT

private slots:
    void freshDbGetsCanonicalSchema();
    void olderDbBackedUpAndRecreated();
    void olderDbDataNotCarriedOver();
    void alreadyCurrentVersionKept();
    void newerThanAppFails();
};

// A brand-new file is created from 001_initial.sql at the current version.
void TestDatabaseMigrations::freshDbGetsCanonicalSchema()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("fresh.db"));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("fresh"), dbPath, &err), qPrintable(err));

    QCOMPARE(userVersion(db.connection()), Database::schemaVersion());
    QVERIFY(tableHasRows(db.connection(), QStringLiteral("app_settings")));
}

// A DB stamped with an older user_version is moved to `.bak` and a fresh
// database is created at the current version (no in-place migration).
void TestDatabaseMigrations::olderDbBackedUpAndRecreated()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("legacy.db"));
    QVERIFY(seedLegacyDb(dbPath, 3));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("old"), dbPath, &err), qPrintable(err));

    QCOMPARE(userVersion(db.connection()), Database::schemaVersion());

    const QString backupPath = dbPath + QStringLiteral(".bak");
    QVERIFY2(QFile::exists(backupPath),
             qPrintable(QStringLiteral("missing backup: ") + backupPath));
}

// The recreated database starts empty: legacy rows are intentionally not
// carried over (pre-production policy — data is not yet important).
void TestDatabaseMigrations::olderDbDataNotCarriedOver()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("legacy2.db"));
    QVERIFY(seedLegacyDb(dbPath, 2));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("old_data"), dbPath, &err), qPrintable(err));

    QSqlQuery q(db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM logger_info")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

// A DB already at the current version is opened as-is (no backup, no reset).
void TestDatabaseMigrations::alreadyCurrentVersionKept()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("current.db"));

    {
        Database db;
        QString err;
        QVERIFY2(db.open(uniqueConnectionName("cur_a"), dbPath, &err), qPrintable(err));

        QSqlQuery ins(db.connection());
        ins.prepare(QStringLiteral(
            "INSERT INTO logger_info (station_code, name, host) "
            "VALUES ('KEEP-01', 'Kept', '192.168.1.1')"));
        QVERIFY(ins.exec());
    }

    Database reopen;
    QString err;
    QVERIFY2(reopen.open(uniqueConnectionName("cur_b"), dbPath, &err), qPrintable(err));

    QCOMPARE(userVersion(reopen.connection()), Database::schemaVersion());
    QVERIFY2(!QFile::exists(dbPath + QStringLiteral(".bak")),
             "current-version DB must not be backed up");

    QSqlQuery q(reopen.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM logger_info")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

// A DB created by a newer app version still refuses to open.
void TestDatabaseMigrations::newerThanAppFails()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("newer.db"));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("seed"), dbPath, &err), qPrintable(err));
    execSql(db.connection(), QStringLiteral("PRAGMA user_version = 99"));
    db.close();

    Database reopen;
    QVERIFY(!reopen.open(uniqueConnectionName("newer"), dbPath, &err));
    QVERIFY2(err.contains(QStringLiteral("Incompatible")), qPrintable(err));
}

QTEST_MAIN(TestDatabaseMigrations)
#include "test_database_migrations.moc"
