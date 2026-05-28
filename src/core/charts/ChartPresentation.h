#pragma once

#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

namespace CentralLogger::Core {

/// Visible window + axis for Dashboard readings chart (FE-002).
struct ReadingsChartPresentation
{
    QVariantList plotPoints;
    QVariantMap  axis;
};

/// Fixed window of @p visiblePointCount time buckets ending at the current bucket
/// (aligned to @p bucketMinutes), same count as Logger Detail trending (PollHistoryStore).
/// Missing buckets are filled with count 0. @p anchorNowMs is for tests only (<=0 = now).
ReadingsChartPresentation buildReadingsChartPresentation(const QVariantList &allBuckets,
                                                         int visiblePointCount,
                                                         int bucketMinutes,
                                                         const QTimeZone &tz,
                                                         qint64 anchorNowMs = 0);

/// Map mouse position in plot area to nearest bucket tooltip payload (empty = miss).
QVariantMap snapReadingsChart(const QVariantList &plotPoints,
                              double axisXMinMs,
                              double axisXMaxMs,
                              double plotX,
                              double plotY,
                              double plotW,
                              double plotH,
                              double mouseX,
                              double mouseY,
                              int bucketMinutes = 5);

/// Axis range for multi-series trending chart (from PollHistoryStore series list).
QVariantMap computeTrendingAxisRange(const QVariantList &trendingSeries);

/// Multi-series trending tooltip snap (uses first series for anchor x/y).
QVariantMap snapTrendingChart(const QVariantList &trendingSeries,
                              double axisXMinMs,
                              double axisXMaxMs,
                              double plotX,
                              double plotY,
                              double plotW,
                              double plotH,
                              double mouseX,
                              double mouseY);

} // namespace CentralLogger::Core
