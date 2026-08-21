#include "Database.h"

#include "utils/AppConstants.h"
#include "utils/DbConstants.h"
#include "utils/Version.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Defaults::kSqliteBusyTimeoutMs;
using CentralLogger::Defaults::kSqliteMmapSize;
using CentralLogger::Version::kSchemaVersion;

constexpr auto kSchemaResource = CentralLogger::Data::Db::kSchemaResource;

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

/// Audit H-B: make sure the database uses incremental auto-vacuum so that
/// `PRAGMA incremental_vacuum` after retention purges can shrink the file.
/// Switching the mode on a non-empty database requires a one-off VACUUM.
bool ensureAutoVacuumIncremental(QSqlDatabase db, QString *errorOut)
{
    QSqlQuery q(db);
    int mode = -1;
    if (q.exec(QStringLiteral("PRAGMA auto_vacuum")) && q.next()) {
        mode = q.value(0).toInt();
    }
    if (mode == 2) {
        return true;
    }
    if (!q.exec(QStringLiteral("PRAGMA auto_vacuum = INCREMENTAL"))) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA auto_vacuum failed: %1")
                            .arg(q.lastError().text());
        }
        return false;
    }
    if (!q.exec(QStringLiteral("VACUUM"))) {
        if (errorOut) {
            *errorOut = QStringLiteral("VACUUM (auto_vacuum switch) failed: %1")
                            .arg(q.lastError().text());
        }
        return false;
    }
    // VACUUM resets journal_mode to the default; applyPerformancePragmas()
    // (called later by open()) re-applies WAL.
    return true;
}

} // namespace

int Database::schemaVersion()
{
    return CentralLogger::Version::kSchemaVersion;
}

Database::~Database()
{
    close();
}

QString Database::defaultPath()
{
    // Place the database alongside the log file (AppDataLocation) so users
    // only have to look in one place for Central Logger's per-user data.
    // On Linux this is ~/.local/share/4M Technologies/Central Logger/
    // (matching main.cpp's log file path); on Windows it is %APPDATA%.
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        // Fallback for restricted environments without AppDataLocation.
        const QString home = QDir::homePath();
        return QDir(home).filePath(QStringLiteral(".central-logger/central-logger.db"));
    }
    return QDir(appData).filePath(QStringLiteral("central-logger.db"));
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

    m_db = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), connectionName);
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

    if (!ensureAutoVacuumIncremental(m_db, errorOut)) {
        close();
        return false;
    }

    bool fresh = freshBefore || isFreshDatabase();

    if (!fresh) {
        int version = 0;
        if (!readUserVersion(&version, errorOut)) {
            close();
            return false;
        }
        if (version > kSchemaVersion) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Incompatible database schema (user_version=%1, expected %2). "
                    "Update the application or remove the database file and restart: %3")
                                .arg(version)
                                .arg(kSchemaVersion)
                                .arg(m_db.databaseName());
            }
            close();
            return false;
        }
        if (version < kSchemaVersion) {
            // Pre-production policy: no in-place migrations. An older DB is
            // backed up to `{path}.bak` and recreated from the canonical
            // schema; its data is intentionally not carried over.
            qInfo() << "Schema version" << version << "older than" << kSchemaVersion
                    << "— backing up" << databasePath
                    << "and recreating database from canonical schema";
            if (!backupAndResetDatabase(databasePath, errorOut)) {
                close();
                return false;
            }
            fresh = true;
        }
    }

    if (fresh) {
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

    // Existing DB already at the current schema version.
    if (!applyPerformancePragmas(m_db, errorOut)) {
        close();
        return false;
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
            "SELECT name FROM sqlite_master WHERE type='table' AND name='%1'")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableAppSettings)))) {
        return true;
    }
    return !query.next();
}

bool Database::readUserVersion(int *versionOut, QString *errorOut)
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
    *versionOut = q.value(0).toInt();
    return true;
}

bool Database::backupAndResetDatabase(const QString &databasePath, QString *errorOut)
{
    const QString connName = m_connectionName; // close() clears it

    if (databasePath == memoryPath() || databasePath.isEmpty()) {
        close();
        return reopenConnection(databasePath, connName, errorOut);
    }

    // Checkpoint the WAL into the main file so the .bak copy is self-contained.
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));

    close(); // release the file handles before touching the files

    const QString backupPath = databasePath + QStringLiteral(".bak");
    if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot remove old backup '%1'").arg(backupPath);
        }
        return false;
    }
    if (!QFile::rename(databasePath, backupPath)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot move '%1' to backup '%2'")
                            .arg(databasePath, backupPath);
        }
        return false;
    }
    for (const QString &suffix : {QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        QFile::remove(databasePath + suffix);
    }

    return reopenConnection(databasePath, connName, errorOut);
}

bool Database::reopenConnection(const QString &databasePath,
                                const QString &connectionName,
                                QString *errorOut)
{
    m_connectionName = connectionName;
    m_db = QSqlDatabase::addDatabase(QLatin1String(CentralLogger::Data::Db::kSqliteDriver), connectionName);
    m_db.setDatabaseName(databasePath);
    if (!m_db.open()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot reopen database '%1': %2")
                            .arg(databasePath, m_db.lastError().text());
        }
        return false;
    }
    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (errorOut) {
            *errorOut = QStringLiteral("PRAGMA foreign_keys failed: %1")
                            .arg(pragma.lastError().text());
        }
        return false;
    }
    return ensureAutoVacuumIncremental(m_db, errorOut);
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
        QStringLiteral("PRAGMA busy_timeout = %1").arg(kSqliteBusyTimeoutMs),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA temp_store = MEMORY"),
        QStringLiteral("PRAGMA mmap_size = %1").arg(kSqliteMmapSize),
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
