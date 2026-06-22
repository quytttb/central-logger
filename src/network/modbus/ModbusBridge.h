#pragma once

#include "ModbusTypes.h"

#include <QList>
#include <QObject>
#include <QSqlDatabase>

namespace CentralLogger::Data {
class Database;
class SensorReading;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {

/// Persists Modbus poll data into SQLite and re-emits UI-friendly signals.
/// `applyLiveSnapshot` runs on the main thread (status + catalog, no
/// sensor_reading inserts). `applyBatch` runs on the history writer thread.
class ModbusBridge : public QObject
{
    Q_OBJECT

public:
    explicit ModbusBridge(QObject *parent = nullptr);

    void setDatabase(Data::Database *db) { m_db = db; m_standaloneConn = {}; }
    void setConnection(QSqlDatabase db);

public slots:
    /// Live pipeline: update logger status/catalog and notify UI. Readings
    /// are persisted asynchronously via applyBatch on the writer thread.
    void applyLiveSnapshot(const CentralLogger::Network::PollSnapshot &snapshot);

    /// History pipeline: batch-insert sensor_reading rows for many snapshots
    /// inside a single transaction. Thread-safe when using a dedicated
    /// QSqlDatabase connection (see HistoryWriterWorker).
    void applyBatch(const QList<CentralLogger::Network::PollSnapshot> &batch);

signals:
    void snapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                         int sensorCount);

private:
    QSqlDatabase sqlConnection() const;
    QVector<Data::SensorReading> buildReadings(const PollSnapshot &snapshot) const;

    Data::Database *m_db = nullptr;
    QSqlDatabase      m_standaloneConn;
};

} // namespace CentralLogger::Network
