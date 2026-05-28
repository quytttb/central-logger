#pragma once

#include "core/sensors/SensorLiveRow.h"
#include "data/models/LoggerSensor.h"
#include "network/modbus/ModbusTypes.h"

#include <QHash>
#include <QVector>

namespace CentralLogger::Core {

/// Per-logger latest merged sensor rows. Plain in-memory cache fed by
/// `DashboardController::onSnapshotApplied`; consumed by
/// `SensorMonitoringTableModel`. No DB, no QObject.
///
/// See docs/thiet_ke_db.md §3.3 (RAM merge) and Task 7 spec — only the
/// most recent successful poll per logger is kept; failed snapshots leave
/// the cached entry untouched (UI still shows last known values until the
/// list model marks the logger offline).
class SensorSnapshotCache
{
public:
    /// Re-builds rows via SensorMerger and stores them under
    /// `snapshot.loggerId`. Failed snapshots (`!snapshot.success`) are
    /// ignored — the previous rows for that logger remain.
    void apply(const Network::PollSnapshot &snapshot,
               const QVector<Data::LoggerSensor> &catalog);

    /// Empty vector when there is no cached entry for @p loggerId.
    QVector<SensorLiveRow> rowsFor(qint64 loggerId) const;

    bool has(qint64 loggerId) const { return m_rows.contains(loggerId); }

    void remove(qint64 loggerId) { m_rows.remove(loggerId); }
    void clear() { m_rows.clear(); }

    int size() const { return m_rows.size(); }

private:
    QHash<qint64, QVector<SensorLiveRow>> m_rows;
};

} // namespace CentralLogger::Core
