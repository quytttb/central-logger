#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QTimeZone>
#include <QVector>

namespace CentralLogger::Core {

/// One 5-minute bucket for the Dashboard readings-over-time chart.
struct ReadingBucketPoint
{
    QString label;      ///< Human-readable bucket label (HH:mm)
    qint64  bucketMs = 0; ///< Bucket start (UTC epoch ms) for DateTimeAxis X
    int     count = 0;  ///< Number of sensor_reading rows in the bucket
};

/// Provides SQL-based chart queries against `sensor_reading`.
/// Task 12 — FE-002 data half.  No QML, no Qt Graphs.
class ChartQueryService
{
public:
    explicit ChartQueryService(QSqlDatabase db) : m_db(std::move(db)) {}

    /// Returns reading counts aggregated in @p bucketMinutes-minute buckets
    /// over the last 24 hours.  Each point carries a label like "14:05"
    /// and the COUNT(*) of readings whose `recorded_at` falls within that
    /// bucket.
    ///
    /// L-20: @p tz controls the timezone used to format bucket labels.
    /// Pass QTimeZone::systemTimeZone() or a configured timezone
    /// (e.g. "Asia/Ho_Chi_Minh") so the chart labels match local time
    /// rather than always showing UTC.
    ///
    /// The results are sorted ascending by time so they can be plotted
    /// left-to-right as a line / bar chart.
    QVector<ReadingBucketPoint> readingCountsLast24h(int bucketMinutes = 5,
                                                     QTimeZone tz = QTimeZone::systemTimeZone()) const;

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Core
