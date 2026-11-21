#include "PollHistoryStore.h"

#include "utils/SensorConstants.h"

#include <algorithm>
#include <cmath>

namespace CentralLogger::Core {

void PollHistoryStore::append(const Network::PollSnapshot &snapshot)
{
    const qint64 loggerId = snapshot.loggerId;
    m_currentValues.remove(loggerId);
    if (!snapshot.success || !snapshot.measuredAt.isValid())
        return;

    const auto timestamp = snapshot.measuredAt.toMSecsSinceEpoch();
    if (m_lastSampleMs.contains(loggerId) && timestamp < m_lastSampleMs.value(loggerId)) {
        // System clock went backwards; restart the sorted RAM horizon.
        m_store.remove(loggerId);
        m_lastEvictedMs.remove(loggerId);
    }
    m_lastSampleMs[loggerId] = timestamp;
    auto &loggerStore = m_store[loggerId];

    for (const auto &analog : snapshot.analogs) {
        const bool currentValid = analog.isValid() && !analog.isStale()
            && std::isfinite(analog.value)
            && analog.statusCode != QLatin1String(CentralLogger::Sensor::kStatusErr);
        if (!currentValid)
            continue;

        m_currentValues[loggerId].insert(analog.edgeSensorId, analog.value);
        loggerStore[analog.edgeSensorId].push_back({timestamp, analog.value, analog.isAlarm()});
    }
    const auto cutoff = timestamp - Defaults::kGraphRamMaxMs;
    for (auto it = loggerStore.begin(); it != loggerStore.end();) {
        auto &points = it.value();
        while (!points.empty() && (points.size() > std::size_t(kMaxPoints)
                                  || points.front().timestampMs < cutoff)) {
            auto &lastEvicted = m_lastEvictedMs[loggerId];
            lastEvicted = std::max(lastEvicted, points.front().timestampMs);
            points.pop_front();
        }
        if (points.empty())
            it = loggerStore.erase(it);
        else
            ++it;
    }
    if (loggerStore.isEmpty())
        m_store.remove(loggerId);
}

void PollHistoryStore::markOffline(qint64 loggerId)
{
    m_currentValues.remove(loggerId);
}

GraphSeries PollHistoryStore::seriesForLogger(qint64 loggerId, qint64 fromMs, qint64 toMs) const
{
    GraphSeries result;

    const auto loggerIt = m_store.constFind(loggerId);
    if (loggerIt == m_store.constEnd()) return result;

    const auto &loggerStore = *loggerIt;
    for (auto it = loggerStore.constBegin(); it != loggerStore.constEnd(); ++it) {
        const auto &points = it.value();
        const auto begin = std::lower_bound(points.begin(), points.end(), fromMs,
            [](const GraphPoint &p, qint64 cutoff) { return p.timestampMs < cutoff; });
        const auto end = std::upper_bound(begin, points.end(), toMs,
            [](qint64 cutoff, const GraphPoint &p) { return cutoff < p.timestampMs; });
        if (begin != end)
            result.insert(it.key(), QVector<GraphPoint>(begin, end));
    }

    return result;
}

bool PollHistoryStore::has(qint64 loggerId) const
{
    return m_store.contains(loggerId);
}

bool PollHistoryStore::coversStart(qint64 loggerId, qint64 fromMs) const
{
    const auto it = m_store.constFind(loggerId);
    return it != m_store.cend() && std::all_of(it->cbegin(), it->cend(),
        [fromMs](const auto &points) { return points.front().timestampMs <= fromMs; });
}

std::optional<double> PollHistoryStore::currentValue(qint64 loggerId, int edgeSensorId) const
{
    const auto logger = m_currentValues.constFind(loggerId);
    if (logger != m_currentValues.cend()) {
        const auto sensor = logger->constFind(edgeSensorId);
        if (sensor != logger->cend())
            return *sensor;
    }
    return std::nullopt;
}

void PollHistoryStore::remove(qint64 loggerId)
{
    m_store.remove(loggerId);
    m_currentValues.remove(loggerId);
    m_lastSampleMs.remove(loggerId);
    m_lastEvictedMs.remove(loggerId);
}

void PollHistoryStore::clear()
{
    m_store.clear();
    m_currentValues.clear();
    m_lastSampleMs.clear();
    m_lastEvictedMs.clear();
}

int PollHistoryStore::pointCount(qint64 loggerId, int edgeSensorId) const
{
    const auto loggerIt = m_store.constFind(loggerId);
    if (loggerIt == m_store.constEnd()) return 0;
    const auto sensorIt = loggerIt->constFind(edgeSensorId);
    if (sensorIt == loggerIt->constEnd()) return 0;
    return static_cast<int>(sensorIt->size());
}

} // namespace CentralLogger::Core
