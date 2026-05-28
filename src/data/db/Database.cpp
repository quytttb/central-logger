#include "Database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace CentralLogger::Data {

namespace {

constexpr auto kSchemaResource = ":/db/schema/001_initial.sql";
constexpr int  kSchemaVersion  = 2;

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

} // namespace

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
    } else if (!ensureCurrentSchema(errorOut)) {
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
            "SELECT name FROM sqlite_master WHERE type='table' AND name='app_settings'"))) {
        return true;
    }
    return !query.next();
}

bool Database::migrateSchemaIfNeeded(int currentVersion, QString *errorOut)
{
    if (currentVersion >= kSchemaVersion) {
        return true;
    }
    if (currentVersion > kSchemaVersion) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Database schema newer than application (user_version=%1, expected %2)")
                            .arg(currentVersion)
                            .arg(kSchemaVersion);
        }
        return false;
    }

    if (!m_db.transaction()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot begin migration transaction: %1")
                            .arg(m_db.lastError().text());
        }
        return false;
    }

    QSqlQuery q(m_db);
    if (currentVersion < 2) {
        if (!q.exec(QStringLiteral(
                "ALTER TABLE logger_sensor ADD COLUMN parent_edge_sensor_id INTEGER"))) {
            const QString err = q.lastError().text();
            if (!err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive)) {
                m_db.rollback();
                if (errorOut) {
                    *errorOut = QStringLiteral("Migration v2 parent_edge_sensor_id: %1").arg(err);
                }
                return false;
            }
        }
        if (!q.exec(QStringLiteral("ALTER TABLE logger_sensor ADD COLUMN di_type TEXT"))) {
            const QString err = q.lastError().text();
            if (!err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive)) {
                m_db.rollback();
                if (errorOut) {
                    *errorOut = QStringLiteral("Migration v2 di_type: %1").arg(err);
                }
                return false;
            }
        }
        currentVersion = 2;
    }

    if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(currentVersion))) {
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

bool Database::ensureCurrentSchema(QString *errorOut)
{
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
    const int version = q.value(0).toInt();
    if (version < kSchemaVersion) {
        return migrateSchemaIfNeeded(version, errorOut);
    }
    if (version > kSchemaVersion) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Incompatible database schema (user_version=%1, expected %2). "
                "Remove the file and restart: %3")
                            .arg(version)
                            .arg(kSchemaVersion)
                            .arg(m_db.databaseName());
        }
        return false;
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
