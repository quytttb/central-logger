#include "Database.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace CentralLogger::Data {

namespace {

constexpr auto kSchemaResource = ":/db/schema/001_initial.sql";
constexpr int  kSchemaVersion  = 6;

const char *migrationResourcePath(int version)
{
    switch (version) {
    case 2: return ":/db/migrations/002_logger_sensor_attach_di.sql";
    case 3: return ":/db/migrations/003_logger_sensor_all_parents.sql";
    case 4: return ":/db/migrations/004_app_settings_history_flush.sql";
    case 5: return ":/db/migrations/005_drop_maintenance_mode.sql";
    case 6: return ":/db/migrations/006_logger_sensor_decimals.sql";
    default: return nullptr;
    }
}

QString readResourceSql(const char *resourcePath, QString *errorOut)
{
    QFile file(QString::fromLatin1(resourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open resource '%1': %2")
                            .arg(QString::fromLatin1(resourcePath), file.errorString());
        }
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QStringList splitStatements(const QString &script)
{
    QStringList statements;
    for (const QString &raw : script.split(QLatin1Char(';'))) {
        const QString trimmed = raw.trimmed();
        if (!trimmed.isEmpty()) {
            statements.append(trimmed);
        }
    }
    return statements;
}

bool tableExists(QSqlDatabase db, const QString &table)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='%1'").arg(table))) {
        return false;
    }
    return q.next();
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

bool isIgnorableMigrationError(const QString &sql, const QString &err)
{
    if (sql.startsWith(QStringLiteral("ALTER TABLE"), Qt::CaseInsensitive)
        && sql.contains(QStringLiteral("ADD COLUMN"), Qt::CaseInsensitive)) {
        return err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive);
    }
    if (sql.startsWith(QStringLiteral("ALTER TABLE"), Qt::CaseInsensitive)
        && sql.contains(QStringLiteral("DROP COLUMN"), Qt::CaseInsensitive)) {
        return err.contains(QStringLiteral("no such column"), Qt::CaseInsensitive);
    }
    return false;
}

} // namespace

int Database::schemaVersion()
{
    return kSchemaVersion;
}

Database::~Database()
{
    close();
}

QString Database::defaultPath()
{
    const QString home = QDir::homePath();
    return QDir(home).filePath(QStringLiteral(".central-logger/central-logger.db"));
}

bool Database::open(const QString &connectionName,
                    const QString &databasePath,
                    QString *errorOut)
{
    if (m_db.isOpen()) {
        close();
    }

    m_connectionName = connectionName;

    if (databasePath != memoryPath()) {
        if (!ensureParentDirectory(databasePath, errorOut)) {
            return false;
        }
    }

    const bool freshBefore = !QFileInfo::exists(databasePath)
                             || databasePath == memoryPath();

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    m_db.setDatabaseName(databasePath);
    if (!m_db.open()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open database '%1': %2")
                            .arg(databasePath, m_db.lastError().text());
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        m_connectionName.clear();
        return false;
    }

    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA foreign_keys failed: %1")
                            .arg(pragma.lastError().text());
        }
        close();
        return false;
    }

    if (freshBefore || isFreshDatabase()) {
        if (!applyInitialSchema(errorOut)) {
            close();
            return false;
        }
        if (!applyPerformancePragmas(m_db, errorOut)) {
            close();
            return false;
        }
        return true;
    }

    int effectiveVersion = 0;
    if (!readEffectiveSchemaVersion(&effectiveVersion, errorOut)) {
        close();
        return false;
    }

    if (effectiveVersion > kSchemaVersion) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Incompatible database schema (user_version=%1, expected %2). "
                "Update the application or remove the database file and restart: %3")
                            .arg(effectiveVersion)
                            .arg(kSchemaVersion)
                            .arg(m_db.databaseName());
        }
        close();
        return false;
    }

    if (effectiveVersion < kSchemaVersion) {
        if (!backupDatabase(databasePath, errorOut)) {
            close();
            return false;
        }
    }

    if (!applyPerformancePragmas(m_db, errorOut)) {
        close();
        return false;
    }

    if (effectiveVersion < kSchemaVersion) {
        if (!migrateSchemaIfNeeded(effectiveVersion, errorOut)) {
            close();
            return false;
        }
    }

    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
    }
}

bool Database::ensureParentDirectory(const QString &databasePath, QString *errorOut)
{
    const QFileInfo info(databasePath);
    const QDir parent = info.absoluteDir();
    if (parent.exists()) {
        return true;
    }
    if (!parent.mkpath(QStringLiteral("."))) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot create directory: %1").arg(parent.absolutePath());
        }
        return false;
    }
    return true;
}

bool Database::isFreshDatabase() const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='app_settings'"))) {
        return true;
    }
    return !query.next();
}

int Database::inferSchemaVersion(int declaredVersion) const
{
    if (declaredVersion > 0) {
        return declaredVersion;
    }
    if (!tableExists(m_db, QStringLiteral("app_settings"))) {
        return declaredVersion;
    }

    const bool hasMaintenance = columnExists(m_db, QStringLiteral("app_settings"),
                                               QStringLiteral("maintenance_mode"));
    const bool hasHistoryFlush = columnExists(m_db, QStringLiteral("app_settings"),
                                              QStringLiteral("history_flush_interval_s"));
    const bool hasAllParents = columnExists(m_db, QStringLiteral("logger_sensor"),
                                            QStringLiteral("all_parent_ids"));
    const bool hasDiType = columnExists(m_db, QStringLiteral("logger_sensor"),
                                          QStringLiteral("di_type"));

    int inferred = 1;
    if (hasDiType) {
        inferred = 2;
    }
    if (hasAllParents) {
        inferred = 3;
    }
    if (hasHistoryFlush && hasMaintenance) {
        inferred = 4;
    }
    if (hasHistoryFlush && !hasMaintenance) {
        inferred = 5;
    }

    if (declaredVersion == 0 && inferred > 0) {
        qInfo() << "Inferred schema version" << inferred
                << "from table layout (PRAGMA user_version was 0)";
    }
    return inferred;
}

bool Database::backupDatabase(const QString &databasePath, QString *errorOut)
{
    if (databasePath == memoryPath() || databasePath.isEmpty()) {
        return true;
    }

    const QString walPath = databasePath + QStringLiteral("-wal");
    if (QFile::exists(walPath)) {
        QSqlQuery q(m_db);
        if (!q.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"))) {
            if (errorOut) {
                *errorOut = QStringLiteral("Pre-migration WAL checkpoint failed: %1")
                                .arg(q.lastError().text());
            }
            return false;
        }
    }

    const QString backupPath = databasePath + QStringLiteral(".bak");
    if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot remove old backup '%1'").arg(backupPath);
        }
        return false;
    }
    if (!QFile::copy(databasePath, backupPath)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot create backup '%1' from '%2'")
                            .arg(backupPath, databasePath);
        }
        return false;
    }
    return true;
}

bool Database::runMigrationStep(int version, QSqlQuery &query, QString *errorOut)
{
    const char *resourcePath = migrationResourcePath(version);
    if (!resourcePath) {
        if (errorOut) {
            *errorOut = QStringLiteral("No migration script for version %1").arg(version);
        }
        return false;
    }

    const QString script = readResourceSql(resourcePath, errorOut);
    if (script.isEmpty()) {
        return false;
    }

    for (const QString &statement : splitStatements(script)) {
        if (!query.exec(statement)) {
            const QString err = query.lastError().text();
            if (isIgnorableMigrationError(statement, err)) {
                continue;
            }
            if (errorOut) {
                *errorOut = QStringLiteral("Migration v%1 failed: %2 — %3")
                                .arg(version)
                                .arg(err, statement);
            }
            return false;
        }
    }
    return true;
}

bool Database::migrateSchemaIfNeeded(int currentVersion, QString *errorOut)
{
    if (currentVersion >= kSchemaVersion) {
        return true;
    }

    if (!m_db.transaction()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot begin migration transaction: %1")
                            .arg(m_db.lastError().text());
        }
        return false;
    }

    QSqlQuery q(m_db);
    for (int version = currentVersion + 1; version <= kSchemaVersion; ++version) {
        if (!runMigrationStep(version, q, errorOut)) {
            m_db.rollback();
            if (errorOut && !errorOut->isEmpty()) {
                const QString backupPath = m_db.databaseName() + QStringLiteral(".bak");
                *errorOut += QStringLiteral(
                    "\n\nA backup was saved to: %1\n"
                    "To restore, close the app, rename that file to replace the database, "
                    "then restart.")
                                 .arg(backupPath);
            }
            return false;
        }
    }

    if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        m_db.rollback();
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA user_version update failed: %1")
                            .arg(q.lastError().text());
        }
        return false;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        if (errorOut) {
            *errorOut = QStringLiteral("Migration commit failed: %1").arg(m_db.lastError().text());
        }
        return false;
    }
    return true;
}

bool Database::readEffectiveSchemaVersion(int *versionOut, QString *errorOut)
{
    if (!versionOut) {
        return false;
    }

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA user_version"))) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA user_version read failed: %1")
                            .arg(q.lastError().text());
        }
        return false;
    }
    if (!q.next()) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA user_version returned no rows");
        }
        return false;
    }

    *versionOut = inferSchemaVersion(q.value(0).toInt());
    return true;
}

bool Database::applyPerformancePragmas(QSqlDatabase db, QString *errorOut)
{
    if (!db.isValid() || !db.isOpen()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Database connection is not open");
        }
        return false;
    }

    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA temp_store = MEMORY"),
        QStringLiteral("PRAGMA mmap_size = 268435456"),
    };

    QSqlQuery q(db);
    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            if (errorOut) {
                *errorOut = QStringLiteral("%1 failed: %2").arg(sql, q.lastError().text());
            }
            return false;
        }
    }
    return true;
}

bool Database::applyInitialSchema(QString *errorOut)
{
    const QString script = readResourceSql(kSchemaResource, errorOut);
    if (script.isEmpty()) {
        return false;
    }

    if (!m_db.transaction()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot begin schema transaction: %1")
                            .arg(m_db.lastError().text());
        }
        return false;
    }

    QSqlQuery query(m_db);
    for (const QString &statement : splitStatements(script)) {
        if (!query.exec(statement)) {
            const QString message = QStringLiteral("Schema statement failed: %1 — %2")
                                        .arg(query.lastError().text(), statement);
            m_db.rollback();
            if (errorOut) {
                *errorOut = message;
            }
            return false;
        }
    }

    if (!m_db.commit()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Schema commit failed: %1").arg(m_db.lastError().text());
        }
        m_db.rollback();
        return false;
    }
    if (!applyPerformancePragmas(m_db, errorOut)) {
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion))) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA user_version failed: %1")
                            .arg(query.lastError().text());
        }
        return false;
    }
    return true;
}

} // namespace CentralLogger::Data
