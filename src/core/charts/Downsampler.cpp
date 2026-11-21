#include "Downsampler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace CentralLogger::Core {

Downsampler::Downsampler(qint64 fromMs, qint64 toMs, int pointBudget)
    : m_fromMs(fromMs), m_toMs(toMs), m_budget(std::max(2, pointBudget)),
      m_bucketCount((m_budget - 2) / 2)
{}

void Downsampler::append(const GraphPoint &point)
{
    if (point.timestampMs < m_fromMs || point.timestampMs > m_toMs
        || !std::isfinite(point.value))
        return;
    const IndexedPoint sample{point, m_size++};
    if (sample.sequence == 0)
        m_first = sample;
    m_last = sample;

    if (m_size <= m_budget)
        m_small.append(point);
    else if (m_size == m_budget + 1)
        m_small.clear();

    if (point.alarm)
        m_kept.append(sample);
    if (m_bucketCount == 0)
        return;

    const auto span = std::max<qint64>(1, m_toMs - m_fromMs);
    const int bucket = std::min(m_bucketCount - 1,
        int(double(point.timestampMs - m_fromMs) / double(span) * m_bucketCount));
    if (bucket != m_bucket) {
        flushBucket();
        m_bucket = bucket;
        m_min = m_max = sample;
    } else {
        if (point.value < m_min.point.value)
            m_min = sample;
        if (point.value > m_max.point.value)
            m_max = sample;
    }
}

void Downsampler::flushBucket()
{
    if (m_bucket >= 0) {
        m_kept.append(m_min);
        if (m_min.sequence != m_max.sequence)
            m_kept.append(m_max);
    }
}

QVector<GraphPoint> Downsampler::takePoints()
{
    if (m_size <= m_budget)
        return std::move(m_small);
    flushBucket();
    m_kept.append(m_first);
    m_kept.append(m_last);
    std::sort(m_kept.begin(), m_kept.end(), [](const IndexedPoint &a, const IndexedPoint &b) {
        return a.sequence < b.sequence;
    });
    const auto end = std::unique(m_kept.begin(), m_kept.end(), [](const IndexedPoint &a, const IndexedPoint &b) {
        return a.sequence == b.sequence;
    });
    QVector<GraphPoint> points;
    points.reserve(std::distance(m_kept.begin(), end));
    for (auto it = m_kept.begin(); it != end; ++it)
        points.append(it->point);
    return points;
}

} // namespace CentralLogger::Core
