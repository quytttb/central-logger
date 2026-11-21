#pragma once

#include "utils/AppConstants.h"

#include <QMap>
#include <QVector>

namespace CentralLogger::Core {

struct GraphPoint {
    qint64 timestampMs = 0;
    double value = 0.0;
    bool alarm = false;
};

using GraphSeries = QMap<int, QVector<GraphPoint>>; // ordered by edge sensor ID

/// Streaming min/max time buckets (as in Data Logger Trending), in chronological
/// order with the first, last and all known alarm points retained. The budget
/// caps non-alarm output and intermediate storage, even for very long SQL ranges.
class Downsampler {
public:
    Downsampler(qint64 fromMs, qint64 toMs, int pointBudget = Defaults::kGraphDownsamplePoints);

    /// Input must be in nondecreasing timestamp order.
    void append(const GraphPoint &point);
    QVector<GraphPoint> takePoints();

private:
    struct IndexedPoint {
        GraphPoint point;
        qsizetype sequence = 0;
    };
    void flushBucket();

    qint64 m_fromMs;
    qint64 m_toMs;
    int m_budget;
    int m_bucketCount;
    int m_bucket = -1;
    qsizetype m_size = 0;
    IndexedPoint m_first, m_last, m_min, m_max;
    QVector<GraphPoint> m_small;
    QVector<IndexedPoint> m_kept;
};

} // namespace CentralLogger::Core
