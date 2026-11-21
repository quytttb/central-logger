#pragma once

#include "core/sensors/SensorLiveRow.h"
#include "data/models/LoggerSensor.h"
#include "network/modbus/ModbusTypes.h"

#include <QDateTime>
#include <QHash>
#include <QVector>

namespace CentralLogger::Core {

/// Per-logger latest merged sensor rows. Plain in-memory cache fed by
/// `DashboardController::onSnapshotApplied`; consumed by
/// `SensorMonitoringTableModel`. No DB, no QObject.
///
/// See docs/thiet_ke_db.md §3.3 (RAM merge). A failed snapshot does not
/// replace the catalog; the controller explicitly marks cached values stale.
class SensorSnapshotCache
{
public:
    /// Re-builds rows via SensorMerger and stores them under
    /// `snapshot.loggerId`. Failed snapshots (`!snapshot.success`) are
    /// ignored — the previous rows for that logger remain.
    void apply(const Network::PollSnapshot &snapshot,
               const QVector<Data::LoggerSensor> &catalog);

    /// Hide cached values and mark them stale when the Modbus connection fails.
    /// The next successful apply replaces these rows with the new snapshot.
    void markOffline(qint64 loggerId);

    /// Re-merges the last successful snapshot for @p loggerId against an
    /// updated catalog (e.g. right after GET /config finishes) without
    /// waiting for the next Modbus poll cycle.
    void rebuildWithCatalog(qint64 loggerId,
                            const QVector<Data::LoggerSensor> &catalog);

    /// Empty vector when there is no cached entry for @p loggerId.
    QVector<SensorLiveRow> rowsFor(qint64 loggerId) const;

    /// Timestamp of the last successful snapshot for @p loggerId (invalid when offline/unset).
    QDateTime measuredAtFor(qint64 loggerId) const;

    bool has(qint64 loggerId) const { return m_rows.contains(loggerId); }

    void remove(qint64 loggerId)
    {
        m_rows.remove(loggerId);
        m_measuredAt.remove(loggerId);
        m_lastSnapshots.remove(loggerId);
    }
    void clear()
    {
        m_rows.clear();
        m_measuredAt.clear();
        m_lastSnapshots.clear();
    }

private:
    QHash<qint64, QVector<SensorLiveRow>> m_rows;
    QHash<qint64, QDateTime>              m_measuredAt;
    QHash<qint64, Network::PollSnapshot>  m_lastSnapshots;
};

} // namespace CentralLogger::Core
