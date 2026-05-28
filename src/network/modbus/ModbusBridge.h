#pragma once

#include "ModbusTypes.h"

#include <QObject>

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {

/// Runs on the main (UI) thread. Persists poll snapshots into SQLite and
/// re-emits a UI-friendly signal for the dashboard / list model.
class ModbusBridge : public QObject
{
    Q_OBJECT

public:
    explicit ModbusBridge(QObject *parent = nullptr);

    void setDatabase(Data::Database *db) { m_db = db; }

public slots:
    void applySnapshot(const CentralLogger::Network::PollSnapshot &snapshot);

signals:
    /// Emitted on the main thread after the snapshot has been persisted
    /// (status, last_seen, sensor_reading rows). Carries the full
    /// snapshot so downstream consumers (DashboardController) can feed
    /// the SensorMerger / SensorSnapshotCache without re-reading the
    /// worker thread's data. @p sensorCount is the catalog size for the
    /// logger after auto-creation.
    void snapshotApplied(const CentralLogger::Network::PollSnapshot &snapshot,
                         int sensorCount);

private:
    Data::Database *m_db = nullptr;
};

} // namespace CentralLogger::Network
