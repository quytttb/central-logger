#pragma once

#include "utils/DbConstants.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include <QThread>

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

    /// Database path inside QStandardPaths::AppDataLocation
    /// (fallback: `~/.central-logger/central-logger.db`).
    static QString defaultPath();

    /// SQLite in-memory path for unit tests (`:memory:`).
    static QString memoryPath() { return QStringLiteral(":memory:"); }

    static QString defaultConnectionName() { return QStringLiteral("central_logger"); }

    /// Current `PRAGMA user_version` baked into the application.
    static int schemaVersion();

    /// WAL + related pragmas for concurrent read (UI) / write (history worker).
    static bool applyPerformancePragmas(QSqlDatabase db, QString *errorOut = nullptr);

    /// Opens a thread-scoped SQLite connection, runs @p fn(db), closes it, and
    /// removes the connection cleanly on every exit path.
    template <typename Fn>
    static bool withScopedConnection(const QString &prefix,
                                     const QString &dbPath,
                                     Fn &&fn,
                                     QString *errorOut = nullptr)
    {
        const QString connName = QStringLiteral("%1_%2").arg(
            prefix,
            QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId())));
        bool ok = false;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QLatin1String(Db::kSqliteDriver), connName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                if (errorOut) {
                    *errorOut = db.lastError().text();
                }
            } else {
                applyPerformancePragmas(db, nullptr);
                fn(db);
                db.close();
                ok = true;
            }
        }
        QSqlDatabase::removeDatabase(connName);
        return ok;
    }

private:
    bool ensureParentDirectory(const QString &databasePath, QString *errorOut);
    bool applyInitialSchema(QString *errorOut);
    bool readUserVersion(int *versionOut, QString *errorOut);
    bool isFreshDatabase() const;

    /// Backup an older-version DB to `{path}.bak` and reopen a brand-new
    /// empty file on the same connection (pre-production policy: recreate
    /// from canonical schema instead of in-place migration).
    bool backupAndResetDatabase(const QString &databasePath, QString *errorOut);
    bool openConnection(const QString &databasePath,
                        const QString &connectionName,
                        QString *errorOut);

    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace CentralLogger::Data
