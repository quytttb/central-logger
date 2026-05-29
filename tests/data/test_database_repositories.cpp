#include "data/db/Database.h"
#include "data/models/AppSettings.h"
#include "data/models/LoggerInfo.h"
#include "data/models/LoggerSensor.h"
#include "data/models/SensorReading.h"
#include "data/models/SystemEvent.h"
#include "data/repositories/EventRepository.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "data/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QString>
#include <QTest>
#include <QVector>

using namespace CentralLogger::Data;

namespace {

LoggerInfo makeLogger(const QString &stationCode, const QString &name = QStringLiteral("Trạm test"))
{
    LoggerInfo info;
    info.stationCode = stationCode;
    info.name        = name;
    info.host        = QStringLiteral("192.168.1.10");
    info.apiToken    = QStringLiteral("secret-token");
    return info;
}

QString uniqueConnectionName(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("test_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

} // namespace

class TestDatabaseRepositories : public QObject
{
    Q_OBJECT

private slots:
    void initInMemory();
    void seedAppSettings();
    void stationCodeUnique();
    void sensorAutoCreateAndReadingFk();
    void insertBatchJoinsOuterTransaction();
    void pruneSkipsDigitalWhenCountZero();
    void upsertUpdatesMetadata();
    void purgeRetention();
    void cascadeDeleteLogger();
    void eventInsertAndList();
    // C-8 regression: prune must deactivate (active=0), never DELETE catalog rows.
    void pruneOrphanSensorsDeactivatesNotDeletes();
    // D-1: ensureExists must reactivate sensors with active=0 after a prune.
    void sensorPruneAndRecover();
};

void TestDatabaseRepositories::initInMemory()
{
    Database db;
    QString err;
    QVERIFY2(db.open(uniqueConnectionName("init"), QStringLiteral(":memory:"), &err),
             qPrintable(err));
    QVERIFY(db.isOpen());

    QSqlQuery q(db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")));
    QStringList tables;
    while (q.next()) {
        tables.append(q.value(0).toString());
    }
    for (const QString &t :
         {QStringLiteral("app_settings"),
          QStringLiteral("logger_info"),
          QStringLiteral("logger_sensor"),
          QStringLiteral("sensor_reading"),
          QStringLiteral("system_event")}) {
        QVERIFY2(tables.contains(t), qPrintable(QStringLiteral("missing table: %1").arg(t)));
    }

    QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 3);
}

void TestDatabaseRepositories::seedAppSettings()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("seed"), QStringLiteral(":memory:")));

    SettingsRepository repo(db.connection());
    const AppSettings s = repo.get();
    QCOMPARE(s.theme,             QStringLiteral("dark"));
    QCOMPARE(s.systemTimezone,    QStringLiteral("Asia/Ho_Chi_Minh"));
    QCOMPARE(s.dataRetentionDays, 30);
    QCOMPARE(s.maintenanceMode,   false);

    AppSettings updated = s;
    updated.theme             = QStringLiteral("light");
    updated.dataRetentionDays = 7;
    QVERIFY(repo.update(updated));

    const AppSettings reloaded = repo.get();
    QCOMPARE(reloaded.theme,             QStringLiteral("light"));
    QCOMPARE(reloaded.dataRetentionDays, 7);
}

void TestDatabaseRepositories::stationCodeUnique()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("unique"), QStringLiteral(":memory:")));

    LoggerRepository repo(db.connection());
    LoggerInfo a = makeLogger(QStringLiteral("TRAM-001"));
    QString err;
    QVERIFY2(repo.insert(a, &err), qPrintable(err));
    QVERIFY(a.id > 0);
    QVERIFY(a.createdAt.isValid());

    LoggerInfo b = makeLogger(QStringLiteral("TRAM-001"), QStringLiteral("Trùng station_code"));
    err.clear();
    QVERIFY(!repo.insert(b, &err));
    QVERIFY(!err.isEmpty());

    const auto fetched = repo.findByStationCode(QStringLiteral("TRAM-001"));
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->id,   a.id);
    QCOMPARE(fetched->name, QStringLiteral("Trạm test"));
}

void TestDatabaseRepositories::sensorAutoCreateAndReadingFk()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("autocreate"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());
    SensorReadingRepository readings(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-AC"));
    QVERIFY(loggerRepo.insert(logger));

    const qint64 sensorId = catalog.ensureExists(logger.id, /*edgeSensorId=*/3,
                                                 QStringLiteral("ANALOG"));
    QVERIFY(sensorId > 0);

    // Idempotent: second call returns the same id, no duplicate rows.
    QCOMPARE(catalog.ensureExists(logger.id, 3, QStringLiteral("ANALOG")), sensorId);
    QCOMPARE(catalog.listByLoggerId(logger.id).size(), 1);

    const auto stored = catalog.findByLoggerAndEdgeId(logger.id, 3, QStringLiteral("ANALOG"));
    QVERIFY(stored.has_value());
    QCOMPARE(stored->sensorType, QStringLiteral("ANALOG"));

    SensorReading r;
    r.sensorId        = sensorId;
    r.value           = 25.5;
    r.loggerTimestamp = 1716220800;
    r.recordedAt      = QDateTime::currentDateTimeUtc();
    QVERIFY(readings.insertBatch({r}));
    QCOMPARE(readings.countForSensor(sensorId), 1);

    SensorReading bad;
    bad.sensorId   = 9999;
    bad.value      = 1.0;
    bad.recordedAt = QDateTime::currentDateTimeUtc();
    QString err;
    QVERIFY(!readings.insertBatch({bad}, &err));
    QVERIFY2(!err.isEmpty(), "FK violation must surface an error");
}

void TestDatabaseRepositories::insertBatchJoinsOuterTransaction()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("nested_tx"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());
    SensorReadingRepository readings(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-TX"));
    QVERIFY(loggerRepo.insert(logger));
    const qint64 sensorId = catalog.ensureExists(logger.id, 1, QStringLiteral("ANALOG"));
    QVERIFY(sensorId > 0);

    SensorReading r;
    r.sensorId   = sensorId;
    r.value      = 12.0;
    r.recordedAt = QDateTime::currentDateTimeUtc();

    QSqlDatabase conn = db.connection();
    QVERIFY(conn.transaction());
    QVERIFY(readings.insertBatch({r}, nullptr, /*manageTransaction*/ false));
    QVERIFY(conn.commit());
    QCOMPARE(readings.countForSensor(sensorId), 1);
}

void TestDatabaseRepositories::pruneSkipsDigitalWhenCountZero()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("prune_zero"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-ZERO"));
    QVERIFY(loggerRepo.insert(logger));
    QVERIFY(catalog.ensureExists(logger.id, 0, QStringLiteral("DI")) > 0);

    const int deactivated = catalog.pruneOrphanSensors(logger.id, {}, /*maxDi=*/0, /*maxDo=*/0);
    QCOMPARE(deactivated, 0);

    const auto row = catalog.findByLoggerAndEdgeId(logger.id, 0, QStringLiteral("DI"));
    QVERIFY(row.has_value());
    QVERIFY(row->active);
}

void TestDatabaseRepositories::upsertUpdatesMetadata()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("upsert"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-UP"));
    QVERIFY(loggerRepo.insert(logger));

    LoggerSensor s;
    s.loggerId      = logger.id;
    s.edgeSensorId  = 5;
    s.sensorType    = QStringLiteral("ANALOG");
    s.name          = QStringLiteral("Nhiệt độ phòng");
    s.unit          = QStringLiteral("C");
    s.minThreshold  = 10.0;
    s.maxThreshold  = 40.0;
    QVERIFY(catalog.upsert(s));
    QVERIFY(s.id > 0);

    LoggerSensor s2 = s;
    s2.id           = 0; // upsert relies on (logger_id, sensor_type, edge_sensor_id)
    s2.sensorType   = QStringLiteral("ANALOG");
    s2.name         = QStringLiteral("Nhiệt độ đã đổi tên");
    s2.maxThreshold = 50.0;
    QVERIFY(catalog.upsert(s2));
    QCOMPARE(s2.id, s.id);

    QCOMPARE(catalog.listByLoggerId(logger.id).size(), 1);
    const auto stored = catalog.findByLoggerAndEdgeId(logger.id, 5, QStringLiteral("ANALOG"));
    QVERIFY(stored.has_value());
    QCOMPARE(stored->name, QStringLiteral("Nhiệt độ đã đổi tên"));
    QVERIFY(stored->maxThreshold.has_value());
    QCOMPARE(*stored->maxThreshold, 50.0);
}

void TestDatabaseRepositories::purgeRetention()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("purge"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());
    SensorReadingRepository readings(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-PR"));
    QVERIFY(loggerRepo.insert(logger));
    const qint64 sensorId = catalog.ensureExists(logger.id, 1, QStringLiteral("ANALOG"));
    QVERIFY(sensorId > 0);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVector<SensorReading> batch;
    for (int daysAgo : {40, 35, 5, 1, 0}) {
        SensorReading r;
        r.sensorId   = sensorId;
        r.value      = daysAgo;
        r.recordedAt = now.addDays(-daysAgo);
        batch.append(r);
    }
    QVERIFY(readings.insertBatch(batch));
    QCOMPARE(readings.countForSensor(sensorId), 5);

    const QDateTime cutoff = now.addDays(-30);
    const int deleted = readings.purgeOlderThan(cutoff);
    QCOMPARE(deleted, 2);
    QCOMPARE(readings.countForSensor(sensorId), 3);
}

void TestDatabaseRepositories::cascadeDeleteLogger()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("cascade"), QStringLiteral(":memory:")));

    LoggerRepository loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());
    SensorReadingRepository readings(db.connection());
    EventRepository events(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-CASCADE"));
    QVERIFY(loggerRepo.insert(logger));
    const qint64 sensorId = catalog.ensureExists(logger.id, 0, QStringLiteral("ANALOG"));
    QVERIFY(sensorId > 0);

    SensorReading r;
    r.sensorId   = sensorId;
    r.value      = 1.0;
    r.recordedAt = QDateTime::currentDateTimeUtc();
    QVERIFY(readings.insertBatch({r}));

    SystemEvent ev;
    ev.loggerId  = logger.id;
    ev.eventType = QStringLiteral("Online");
    ev.message   = QStringLiteral("Logger online");
    QVERIFY(events.insert(ev));

    QVERIFY(loggerRepo.remove(logger.id));
    QCOMPARE(catalog.listByLoggerId(logger.id).size(), 0);
    QCOMPARE(readings.countForSensor(sensorId), 0);
    // C-3 fix: system_event now uses ON DELETE SET NULL so events survive
    // logger deletion (logger_id becomes NULL). App-wide audit is preserved.
    const auto remaining = events.listRecent();
    QCOMPARE(remaining.size(), 1);
    QVERIFY2(!remaining.first().loggerId.has_value(),
             "logger_id must be NULL after ON DELETE SET NULL");
}

void TestDatabaseRepositories::eventInsertAndList()
{
    Database db;
    QVERIFY(db.open(uniqueConnectionName("event"), QStringLiteral(":memory:")));

    EventRepository events(db.connection());

    SystemEvent appWide;
    appWide.eventType = QStringLiteral("Info");
    appWide.message   = QStringLiteral("App started");
    appWide.level     = QStringLiteral("info");
    QVERIFY(events.insert(appWide));
    QVERIFY(appWide.id > 0);
    QVERIFY(!appWide.loggerId.has_value());

    LoggerRepository loggerRepo(db.connection());
    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-EV"));
    QVERIFY(loggerRepo.insert(logger));

    SystemEvent perLogger;
    perLogger.loggerId  = logger.id;
    perLogger.eventType = QStringLiteral("Alarm");
    perLogger.message   = QStringLiteral("Temperature high");
    perLogger.level     = QStringLiteral("critical");
    QVERIFY(events.insert(perLogger));

    const auto recent = events.listRecent(5);
    QCOMPARE(recent.size(), 2);
    QCOMPARE(recent.first().eventType, QStringLiteral("Alarm"));
    QVERIFY(recent.first().loggerId.has_value());
    QCOMPARE(*recent.first().loggerId, logger.id);
}

void TestDatabaseRepositories::pruneOrphanSensorsDeactivatesNotDeletes()
{
    // C-8: pruneOrphanSensors must mark sensors active=0, not DELETE them.
    // Deleting would cascade into sensor_reading and destroy measurement history.
    Database db;
    QVERIFY(db.open(uniqueConnectionName("prune"), QStringLiteral(":memory:")));

    LoggerRepository    loggerRepo(db.connection());
    SensorCatalogRepository catalog(db.connection());
    SensorReadingRepository readings(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-PRUNE"));
    QVERIFY(loggerRepo.insert(logger));

    // Create 4 ANALOG sensors (edgeSensorId 0..3).
    QVector<qint64> sensorIds;
    for (int i = 0; i < 4; ++i) {
        const qint64 sid = catalog.ensureExists(logger.id, i, QStringLiteral("ANALOG"));
        QVERIFY(sid > 0);
        sensorIds.append(sid);
    }
    QCOMPARE(catalog.listByLoggerId(logger.id).size(), 4);

    // Insert one reading for every sensor.
    QVector<SensorReading> batch;
    for (qint64 sid : sensorIds) {
        SensorReading r;
        r.sensorId   = sid;
        r.value      = 1.0;
        r.recordedAt = QDateTime::currentDateTimeUtc();
        batch.append(r);
    }
    QVERIFY(readings.insertBatch(batch));

    // Wire now reports only sensor_ids 0 and 1 (not block-count based).
    const int deactivated = catalog.pruneOrphanSensors(logger.id,
                                                       QVector<int>{0, 1},
                                                       /*maxDi=*/-1, /*maxDo=*/-1);
    QCOMPARE(deactivated, 2); // sensors 2 and 3 deactivated

    // Catalog still has 4 rows (not deleted).
    const auto allSensors = catalog.listByLoggerId(logger.id);
    QCOMPARE(allSensors.size(), 4);

    int activeCount = 0, inactiveCount = 0;
    for (const auto &s : allSensors) {
        if (s.active) ++activeCount; else ++inactiveCount;
    }
    QCOMPARE(activeCount,   2);
    QCOMPARE(inactiveCount, 2);

    // Readings for deactivated sensors still exist (history preserved).
    QCOMPARE(readings.countForSensor(sensorIds[2]), 1);
    QCOMPARE(readings.countForSensor(sensorIds[3]), 1);

    // Idempotent: second prune with same limit changes nothing.
    const int secondPass = catalog.pruneOrphanSensors(logger.id, {0, 1}, -1, -1);
    QCOMPARE(secondPass, 0);
}

void TestDatabaseRepositories::sensorPruneAndRecover()
{
    // D-1: ensureExists must reactivate a sensor whose active flag was set to 0
    // by pruneOrphanSensors when the device later re-exposes the sensor.
    Database db;
    QVERIFY(db.open(uniqueConnectionName("recover"), QStringLiteral(":memory:")));

    LoggerRepository            loggerRepo(db.connection());
    SensorCatalogRepository     catalog(db.connection());
    SensorReadingRepository     readings(db.connection());

    LoggerInfo logger = makeLogger(QStringLiteral("TRAM-RCV"));
    QVERIFY(loggerRepo.insert(logger));

    // Create 3 ANALOG sensors (edgeSensorId 0, 1, 2).
    for (int i = 0; i < 3; ++i) {
        const qint64 sid = catalog.ensureExists(logger.id, i, QStringLiteral("ANALOG"));
        QVERIFY(sid > 0);
    }

    // Prune: wire only reports sensor_ids 0 and 1 → sensor 2 becomes active=0.
    const int deactivated = catalog.pruneOrphanSensors(logger.id, {0, 1}, -1, -1);
    QCOMPARE(deactivated, 1);

    const auto afterPrune = catalog.findByLoggerAndEdgeId(
        logger.id, 2, QStringLiteral("ANALOG"));
    QVERIFY(afterPrune.has_value());
    QVERIFY2(!afterPrune->active, "sensor must be inactive after prune");

    // Map expands again: edge re-reports edgeSensorId=2 — ensureExists must
    // flip active back to 1 and return the existing row id.
    const qint64 recoveredId = catalog.ensureExists(logger.id, 2, QStringLiteral("ANALOG"));
    QVERIFY(recoveredId > 0);
    QCOMPARE(recoveredId, afterPrune->id); // same row, not a duplicate

    const auto afterRecover = catalog.findByLoggerAndEdgeId(
        logger.id, 2, QStringLiteral("ANALOG"));
    QVERIFY(afterRecover.has_value());
    QVERIFY2(afterRecover->active, "sensor must be active again after ensureExists");

    // Readings can be linked to the recovered sensor without FK issues.
    SensorReading r;
    r.sensorId   = recoveredId;
    r.value      = 42.0;
    r.recordedAt = QDateTime::currentDateTimeUtc();
    QVERIFY(readings.insertBatch({r}));
    QCOMPARE(readings.countForSensor(recoveredId), 1);
}

QTEST_MAIN(TestDatabaseRepositories)
#include "test_database_repositories.moc"
