#pragma once

#include <QSqlDatabase>
#include <QString>

namespace CentralLogger::Data {

/// Single entry point for the SQLite (QSQLITE) connection used by the
/// repository layer. See docs/adr/0001-db.md and docs/thiet_ke_db.md.
class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /// Opens (or creates) the SQLite database at @p databasePath using the
    /// given connection name. Creates parent directories when needed,
    /// enables foreign keys and runs `001_initial.sql` for fresh DBs.
    /// Pass memoryPath() as @p databasePath for tests.
    bool open(const QString &connectionName = defaultConnectionName(),
              const QString &databasePath = defaultPath(),
              QString *errorOut = nullptr);

    void close();

    bool isOpen() const { return m_db.isOpen(); }

    /// Live connection handle for repositories. Returns an invalid
    /// QSqlDatabase when the database is not open.
    QSqlDatabase connection() const { return m_db; }

    QString connectionName() const { return m_connectionName; }

    /// `~/.central-logger/central-logger.db`
    static QString defaultPath();

    /// SQLite in-memory path for unit tests (`:memory:`).
    static QString memoryPath() { return QStringLiteral(":memory:"); }

    static QString defaultConnectionName() { return QStringLiteral("central_logger"); }

private:
    bool ensureParentDirectory(const QString &databasePath, QString *errorOut);
    bool applyInitialSchema(QString *errorOut);
    bool ensureCurrentSchema(QString *errorOut);
    bool migrateSchemaIfNeeded(int currentVersion, QString *errorOut);
    bool isFreshDatabase() const;

    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace CentralLogger::Data
