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
    m_measuredAt.insert(snapshot.loggerId, snapshot.measuredAt);
    m_lastSnapshots.insert(snapshot.loggerId, snapshot);
}

void SensorSnapshotCache::markOffline(qint64 loggerId)
{
    m_measuredAt.remove(loggerId);
    m_lastSnapshots.remove(loggerId);
    auto it = m_rows.find(loggerId);
    if (it == m_rows.end())
        return;
    for (auto &row : *it) {
        row.valid = false;
        row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
        row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusUnknown);
        row.alarmType.clear();
        row.attachDiTypeCodes.clear();
        row.attachDiTypeLabels.clear();
    }
}

void SensorSnapshotCache::rebuildWithCatalog(
    qint64 loggerId, const QVector<Data::LoggerSensor> &catalog)
{
    const auto it = m_lastSnapshots.constFind(loggerId);
    if (it == m_lastSnapshots.constEnd() || !it->success) {
        return;
    }
    m_rows.insert(loggerId, SensorMerger::buildRows(loggerId, *it, catalog));
}

QVector<SensorLiveRow> SensorSnapshotCache::rowsFor(qint64 loggerId) const
{
    const auto it = m_rows.constFind(loggerId);
    return it != m_rows.constEnd() ? *it : QVector<SensorLiveRow>{};
}

QDateTime SensorSnapshotCache::measuredAtFor(qint64 loggerId) const
{
    return m_measuredAt.value(loggerId);
}

} // namespace CentralLogger::Core
