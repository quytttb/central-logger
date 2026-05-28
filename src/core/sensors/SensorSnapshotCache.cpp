#include "core/sensors/SensorSnapshotCache.h"

#include "core/sensors/SensorMerger.h"

namespace CentralLogger::Core {

void SensorSnapshotCache::apply(const Network::PollSnapshot &snapshot,
                                const QVector<Data::LoggerSensor> &catalog)
{
    if (!snapshot.success || snapshot.loggerId <= 0) {
        return;
    }
    auto rows = SensorMerger::buildRows(snapshot.loggerId, snapshot, catalog);
    m_rows.insert(snapshot.loggerId, std::move(rows));
}

QVector<SensorLiveRow> SensorSnapshotCache::rowsFor(qint64 loggerId) const
{
    const auto it = m_rows.constFind(loggerId);
    return it != m_rows.constEnd() ? *it : QVector<SensorLiveRow>{};
}

} // namespace CentralLogger::Core
