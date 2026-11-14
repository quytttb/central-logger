#include "core/sensors/SensorSnapshotCache.h"

#include "core/sensors/SensorMerger.h"
#include "utils/SensorConstants.h"

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

void SensorSnapshotCache::markOffline(qint64 loggerId)
{
    auto it = m_rows.find(loggerId);
    if (it == m_rows.end())
        return;
    for (auto &row : *it) {
        row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
        row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusStale);
        row.valid = false;
        row.stale = true;
        row.alarm = false;
        row.alarmType.clear();
        row.attachDiTypeCodes.clear();
        row.attachDiTypeLabels.clear();
    }
}

QVector<SensorLiveRow> SensorSnapshotCache::rowsFor(qint64 loggerId) const
{
    const auto it = m_rows.constFind(loggerId);
    return it != m_rows.constEnd() ? *it : QVector<SensorLiveRow>{};
}

} // namespace CentralLogger::Core
