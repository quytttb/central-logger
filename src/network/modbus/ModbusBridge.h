#pragma once

#include "ModbusTypes.h"
#include "data/models/LoggerSensor.h"
#include "utils/AppConstants.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSqlDatabase>

namespace CentralLogger::Data {
class Database;
class SensorReading;
class SensorCatalogRepository;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {

/// Persists Modbus poll data into SQLite and re-emits UI-friendly signals.
/// `applyLiveSnapshot` runs on its own dedicated thread (audit H-A — moved
/// off the UI thread) against a dedicated connection opened in start().
/// `applyBatch` runs on the history writer thread.
class ModbusBridge : public QObject
{
    Q_OBJECT

public:
    explicit ModbusBridge(QObject *parent = nullptr);

    void setDatabase(Data::Database *db) { m_db = db; m_standaloneConn = {}; }
    void setConnection(QSqlDatabase db);

    /// Path for the dedicated connection used by applyLiveSnapshot when the
    /// bridge runs on its own thread (audit H-A). Ignored when
    /// setDatabase()/setConnection() supplied a connection.
    void setDatabasePath(const QString &path) { m_databasePath = path; }

public slots:
    /// Opens the dedicated `modbus_live` connection. Invoke from the bridge's
    /// own thread (e.g. via QThread::started) before polling begins.
    void start();

    /// Closes the dedicated connection. Invoke before quitting the thread.
    void shutdown();

    /// Drops cached catalog ids for @p loggerId (safe from any thread when
    /// invoked queued onto the bridge's thread).
    void invalidateCatalogCache(qint64 loggerId);

    /// Live pipeline: update logger status/catalog and notify UI. Readings
    /// are persisted asynchronously via applyBatch on the writer thread.
    void applyLiveSnapshot(const CentralLogger::Network::PollSnapshot &snapshot);

public:
    /// History pipeline: batch-insert sensor_reading rows for many snapshots
    /// inside a single transaction. Caller must pass the dedicated
    /// QSqlDatabase connection that belongs to the calling thread
    /// (e.g. HistoryWriterWorker::m_db) — never the main-thread connection.
    void applyBatch(const QList<CentralLogger::Network::PollSnapshot> &batch,
                    QSqlDatabase db);

signals:
    /// Audit H-A: carries the already-fetched catalog rows so the main
    /// thread consumer (DashboardController) does not re-query the DB.
    /// Empty vector when @p snapshot.success is false.
    void snapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                         int sensorCount,
                         const QVector<Data::LoggerSensor> &catalogRows);

private:
    QSqlDatabase sqlConnection() const;
    QVector<Data::SensorReading> buildReadings(const PollSnapshot &snapshot,
                                                QSqlDatabase db);

    /// Per-logger catalog cache (audit M-2): skips the UPSERT+SELECT round
    /// trip per sample in the hot poll path. Lives on the bridge's thread
    /// only. Catalog ids never reuse (rows are deactivated, not deleted), so
    /// the cache stays valid; invalidateCatalogCache() still clears it after
    /// CRUD mutations for safety.
    struct CatalogCacheEntry
    {
        QHash<int, qint64> analogIds;   // edgeSensorId -> logger_sensor.id
        QHash<int, qint64> diIds;       // edgeSensorId (bit) -> id
        QHash<int, qint64> doIds;       // edgeSensorId (bit) -> id
    };
    CatalogCacheEntry &catalogCacheFor(qint64 loggerId, QSqlDatabase &db);

    Data::Database *m_db = nullptr;
    QSqlDatabase      m_standaloneConn;

    /// Dedicated live-pipeline connection opened in start(); used when the
    /// bridge runs on its own thread (audit H-A).
    QSqlDatabase       m_dedicatedConn;
    QString            m_databasePath;

    QHash<qint64, CatalogCacheEntry> m_catalogCache;

    // H-C (store-on-change): last written value/flags keyed by catalog
    // sensor id. Lives on the dedicated history-writer thread only, so no
    // locking is needed. See docs/adr/0002-store-on-change.md.
    QHash<qint64, double> m_lastValue;
    QHash<qint64, int>    m_lastFlags;   // valid|alarm|stale bitmask
    QHash<qint64, qint64> m_lastWrittenMs;
    static constexpr qint64 kHeartbeatMs = Defaults::kModbusHeartbeatMs;
};

} // namespace CentralLogger::Network
