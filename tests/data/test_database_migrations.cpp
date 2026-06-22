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

bool columnExists(QSqlDatabase db, const QString &table, const QString &column)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }
    while (q.next()) {
        if (q.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

int userVersion(QSqlDatabase db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next()) {
        return -1;
    }
    return q.value(0).toInt();
}

bool applyLegacyV1Schema(QSqlDatabase db)
{
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
            "CREATE TABLE logger_sensor ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "logger_id INTEGER NOT NULL,"
            "edge_sensor_id INTEGER NOT NULL,"
            "sensor_type TEXT NOT NULL DEFAULT 'UNKNOWN',"
            "name TEXT NOT NULL DEFAULT '',"
            "unit TEXT NOT NULL DEFAULT '',"
            "min_threshold REAL,"
            "max_threshold REAL,"
            "active INTEGER NOT NULL DEFAULT 1,"
            "UNIQUE(logger_id, sensor_type, edge_sensor_id),"
            "FOREIGN KEY (logger_id) REFERENCES logger_info(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE sensor_reading ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "sensor_id INTEGER NOT NULL,"
            "value REAL NOT NULL,"
            "valid INTEGER NOT NULL DEFAULT 1,"
            "alarm INTEGER NOT NULL DEFAULT 0,"
            "stale INTEGER NOT NULL DEFAULT 0,"
            "logger_timestamp INTEGER NOT NULL DEFAULT 0,"
            "recorded_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),"
            "FOREIGN KEY (sensor_id) REFERENCES logger_sensor(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE system_event ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "logger_id INTEGER,"
            "event_type TEXT NOT NULL,"
            "message TEXT NOT NULL DEFAULT '',"
            "level TEXT NOT NULL DEFAULT 'info',"
            "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),"
            "FOREIGN KEY (logger_id) REFERENCES logger_info(id) ON DELETE SET NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE app_settings ("
            "id INTEGER PRIMARY KEY CHECK (id = 1),"
            "theme TEXT NOT NULL DEFAULT 'dark',"
            "system_timezone TEXT NOT NULL DEFAULT 'Asia/Ho_Chi_Minh',"
            "data_retention_days INTEGER NOT NULL DEFAULT 30,"
            "maintenance_mode INTEGER NOT NULL DEFAULT 0"
            ")"),
        QStringLiteral("INSERT INTO app_settings (id) VALUES (1)"),
    };

    for (const QString &sql : statements) {
        if (!execSql(db, sql)) {
            return false;
        }
    }
    return execSql(db, QStringLiteral("PRAGMA user_version = 1"));
}

bool applyLegacyV3Schema(QSqlDatabase db)
{
    if (!applyLegacyV1Schema(db)) {
        return false;
    }
    if (!execSql(db, QStringLiteral(
            "ALTER TABLE logger_sensor ADD COLUMN parent_edge_sensor_id INTEGER"))) {
        return false;
    }
    if (!execSql(db, QStringLiteral("ALTER TABLE logger_sensor ADD COLUMN di_type TEXT"))) {
        return false;
    }
    if (!execSql(db, QStringLiteral("ALTER TABLE logger_sensor ADD COLUMN all_parent_ids TEXT"))) {
        return false;
    }
    return execSql(db, QStringLiteral("PRAGMA user_version = 3"));
}

bool applyLegacyV4Schema(QSqlDatabase db)
{
    if (!applyLegacyV3Schema(db)) {
        return false;
    }
    if (!execSql(db, QStringLiteral(
            "ALTER TABLE app_settings ADD COLUMN history_flush_interval_s INTEGER DEFAULT 5"))) {
        return false;
    }
    if (!execSql(db, QStringLiteral(
            "UPDATE app_settings SET history_flush_interval_s = 5 WHERE id = 1"))) {
        return false;
    }
    return execSql(db, QStringLiteral("PRAGMA user_version = 4"));
}

bool applyLegacyV5Schema(QSqlDatabase db)
{
    if (!applyLegacyV4Schema(db)) {
        return false;
    }
    // Migration 005 dropped maintenance_mode from app_settings.
    if (!execSql(db, QStringLiteral(
            "ALTER TABLE app_settings DROP COLUMN maintenance_mode"))) {
        return false;
    }
    return execSql(db, QStringLiteral("PRAGMA user_version = 5"));
}

bool seedLegacyDb(const QString &dbPath, bool (*schemaFn)(QSqlDatabase))
{
    const QString connName = uniqueConnectionName("seed");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return false;
        }
        const bool ok = schemaFn(db);
        db.close();
        if (!ok) {
            return false;
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return true;
}

} // namespace

class TestDatabaseMigrations : public QObject
{
    Q_OBJECT

private slots:
    void upgradeFromV1ToV6();
    void upgradeFromV4ToV6();
    void upgradeFromV5ToV6();
    void alreadyAtV6();
    void newerThanApp();
    void inferVersionWhenZero();
    void backupCreatedOnMigrate();
};

void TestDatabaseMigrations::upgradeFromV1ToV6()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("v1.db"));
    QVERIFY(seedLegacyDb(dbPath, applyLegacyV1Schema));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("v1_up"), dbPath, &err), qPrintable(err));

    const QSqlDatabase c = db.connection();
    QCOMPARE(userVersion(c), Database::schemaVersion());
    QVERIFY(columnExists(c, QStringLiteral("logger_sensor"), QStringLiteral("di_type")));
    QVERIFY(columnExists(c, QStringLiteral("logger_sensor"), QStringLiteral("all_parent_ids")));
    QVERIFY(columnExists(c, QStringLiteral("logger_sensor"), QStringLiteral("decimals")));
    QVERIFY(columnExists(c, QStringLiteral("app_settings"),
                         QStringLiteral("history_flush_interval_s")));
    QVERIFY(!columnExists(c, QStringLiteral("app_settings"), QStringLiteral("maintenance_mode")));
}

void TestDatabaseMigrations::upgradeFromV4ToV6()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("v4.db"));
    QVERIFY(seedLegacyDb(dbPath, applyLegacyV4Schema));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("v4_up"), dbPath, &err), qPrintable(err));

    const QSqlDatabase c = db.connection();
    QCOMPARE(userVersion(c), 6);
    QVERIFY(columnExists(c, QStringLiteral("logger_sensor"), QStringLiteral("decimals")));
    QVERIFY(!columnExists(c, QStringLiteral("app_settings"), QStringLiteral("maintenance_mode")));
}

void TestDatabaseMigrations::upgradeFromV5ToV6()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("v5.db"));
    QVERIFY(seedLegacyDb(dbPath, applyLegacyV5Schema));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("v5_up"), dbPath, &err), qPrintable(err));

    const QSqlDatabase c = db.connection();
    QCOMPARE(userVersion(c), 6);
    QVERIFY(columnExists(c, QStringLiteral("logger_sensor"), QStringLiteral("decimals")));

    // New decimals column must default to 4 for rows migrated from older DBs.
    QVERIFY(execSql(c, QStringLiteral(
        "INSERT INTO logger_info (station_code, name, host) "
        "VALUES ('S', 'S', 'h')")));
    QVERIFY(execSql(c, QStringLiteral(
        "INSERT INTO logger_sensor (logger_id, edge_sensor_id, sensor_type) "
        "VALUES (1, 1, 'ANALOG')")));
    QSqlQuery q(c);
    QVERIFY(q.exec(QStringLiteral("SELECT decimals FROM logger_sensor WHERE edge_sensor_id = 1")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 4);
}

void TestDatabaseMigrations::alreadyAtV6()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("v6.db"));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("v6_a"), dbPath, &err), qPrintable(err));
    db.close();

    Database reopen;
    QVERIFY2(reopen.open(uniqueConnectionName("v6_b"), dbPath, &err), qPrintable(err));
    QCOMPARE(userVersion(reopen.connection()), 6);
}

void TestDatabaseMigrations::newerThanApp()
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

void TestDatabaseMigrations::inferVersionWhenZero()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("v3zero.db"));

    const QString connName = uniqueConnectionName("v3_zero");
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        legacy.setDatabaseName(dbPath);
        QVERIFY(legacy.open());
        QVERIFY(applyLegacyV3Schema(legacy));
        QVERIFY(execSql(legacy, QStringLiteral("PRAGMA user_version = 0")));
        legacy.close();
    }
    QSqlDatabase::removeDatabase(connName);

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("infer"), dbPath, &err), qPrintable(err));

    const QSqlDatabase c = db.connection();
    QCOMPARE(userVersion(c), 6);
    QVERIFY(columnExists(c, QStringLiteral("app_settings"),
                         QStringLiteral("history_flush_interval_s")));
    QVERIFY(!columnExists(c, QStringLiteral("app_settings"), QStringLiteral("maintenance_mode")));
}

void TestDatabaseMigrations::backupCreatedOnMigrate()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("legacy.db"));
    QVERIFY(seedLegacyDb(dbPath, applyLegacyV1Schema));

    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("file_up"), dbPath, &err), qPrintable(err));

    const QString backupPath = dbPath + QStringLiteral(".bak");
    QVERIFY2(QFile::exists(backupPath), qPrintable(QStringLiteral("missing backup: ") + backupPath));
    QCOMPARE(userVersion(db.connection()), 6);
}

QTEST_MAIN(TestDatabaseMigrations)
#include "test_database_migrations.moc"
