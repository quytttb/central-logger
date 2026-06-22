#include "ChartPresentation.h"

#include <QDateTime>
#include <QHash>

#include <QtMath>

#include <limits>

namespace CentralLogger::Core {

namespace {

const QString kLabelKey    = QStringLiteral("label");
const QString kBucketMsKey = QStringLiteral("bucketMs");
const QString kCountKey    = QStringLiteral("count");
const QString kPointsKey   = QStringLiteral("points");
const QString kXKey        = QStringLiteral("x");
const QString kYKey        = QStringLiteral("y");
const QString kTimeKey     = QStringLiteral("time");

int nearestBucketIndex(const QVariantList &plotPoints, qint64 tsMs)
{
    if (plotPoints.isEmpty())
        return -1;

    int best = 0;
    qint64 bestDist = std::numeric_limits<qint64>::max();
    for (int i = 0; i < plotPoints.size(); ++i) {
        const qint64 bucketMs = plotPoints.at(i).toMap().value(kBucketMsKey).toLongLong();
        const qint64 dist = qAbs(bucketMs - tsMs);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

int nearestTrendingPointIndex(const QVariantList &points, qint64 tsMs)
{
    if (points.isEmpty())
        return -1;

    int best = 0;
    qint64 bestDist = std::numeric_limits<qint64>::max();
    for (int p = 0; p < points.size(); ++p) {
        const qint64 xMs = static_cast<qint64>(points.at(p).toMap().value(kXKey).toDouble());
        const qint64 dist = qAbs(xMs - tsMs);
        if (dist < bestDist) {
            bestDist = dist;
            best = p;
        }
    }
    return best;
}

bool mouseInPlotArea(double plotX, double plotY, double plotW, double plotH,
                     double mouseX, double mouseY)
{
    return plotW > 0.0 && plotH > 0.0
           && mouseX >= plotX && mouseX <= plotX + plotW
           && mouseY >= plotY && mouseY <= plotY + plotH;
}

} // namespace

ReadingsChartPresentation buildReadingsChartPresentation(const QVariantList &allBuckets,
                                                         int visiblePointCount,
                                                         int bucketMinutes,
                                                         const QTimeZone &tz,
                                                         qint64 anchorNowMs)
{
    ReadingsChartPresentation out;
    if (visiblePointCount < 1)
        visiblePointCount = 1;
    if (bucketMinutes < 1)
        bucketMinutes = 5;

    const qint64 bucketMs = static_cast<qint64>(bucketMinutes) * 60 * 1000;
    const QTimeZone useTz = tz.isValid() ? tz : QTimeZone::systemTimeZone();

    QHash<qint64, QVariantMap> byBucket;
    byBucket.reserve(allBuckets.size());
    for (const QVariant &v : allBuckets) {
        const QVariantMap row = v.toMap();
        byBucket.insert(row.value(kBucketMsKey).toLongLong(), row);
    }

    const qint64 nowMs =
        anchorNowMs > 0 ? anchorNowMs : QDateTime::currentMSecsSinceEpoch();
    const qint64 endBucketMs = (nowMs / bucketMs) * bucketMs;

    int yMax = 1;
    out.plotPoints.reserve(visiblePointCount);
    for (int i = visiblePointCount - 1; i >= 0; --i) {
        const qint64 bucketStart = endBucketMs - static_cast<qint64>(i) * bucketMs;
        QVariantMap row;
        const auto it = byBucket.constFind(bucketStart);
        if (it != byBucket.constEnd()) {
            row = it.value();
        } else {
            row.insert(kBucketMsKey, bucketStart);
            row.insert(kCountKey, 0);
            row.insert(kLabelKey,
                       QDateTime::fromMSecsSinceEpoch(bucketStart, useTz)
                           .toString(QStringLiteral("HH:mm")));
        }
        yMax = qMax(yMax, row.value(kCountKey).toInt());
        out.plotPoints.append(row);
    }

    const double xMin =
        out.plotPoints.first().toMap().value(kBucketMsKey).toDouble();
    const double xMax =
        out.plotPoints.last().toMap().value(kBucketMsKey).toDouble()
        + static_cast<double>(bucketMs);

    out.axis = {{QStringLiteral("xMin"), xMin},
                {QStringLiteral("xMax"), xMax},
                {QStringLiteral("yMax"), static_cast<double>(yMax) * 1.1}};

    return out;
}

QVariantMap snapReadingsChart(const QVariantList &plotPoints,
                              double axisXMinMs,
                              double axisXMaxMs,
                              double plotX,
                              double plotY,
                              double plotW,
                              double plotH,
                              double mouseX,
                              double mouseY,
                              int bucketMinutes)
{
    if (!mouseInPlotArea(plotX, plotY, plotW, plotH, mouseX, mouseY))
        return {};

    const double ratio = (mouseX - plotX) / plotW;
    const qint64 tsMs = static_cast<qint64>(axisXMinMs + ratio * (axisXMaxMs - axisXMinMs));
    const int idx = nearestBucketIndex(plotPoints, tsMs);
    if (idx < 0)
        return {};

    const QVariantMap row = plotPoints.at(idx).toMap();
    const qint64 bucketMs = row.value(kBucketMsKey).toLongLong();
    const int count = row.value(kCountKey).toInt();

    QVariantMap position;
    position.insert(kXKey, bucketMs);
    position.insert(kYKey, count);

    QVariantList valueRows;
    QVariantMap valueRow;
    valueRow.insert(QStringLiteral("text"),
                    QStringLiteral("Readings: %1 / %2 min").arg(count).arg(bucketMinutes));
    valueRows.append(valueRow);

    QVariantMap hit;
    hit.insert(QStringLiteral("position"), position);
    hit.insert(QStringLiteral("captionText"), row.value(kLabelKey));
    hit.insert(QStringLiteral("valueRows"), valueRows);
    return hit;
}

QVariantMap computeTrendingAxisRange(const QVariantList &series)
{
    double yMin = std::numeric_limits<double>::max();
    double yMax = -std::numeric_limits<double>::max();
    double xMin = std::numeric_limits<double>::max();
    double xMax = -std::numeric_limits<double>::max();

    for (const QVariant &sv : series) {
        const QVariantList pts = sv.toMap().value(kPointsKey).toList();
        for (const QVariant &pv : pts) {
            const QVariantMap pt = pv.toMap();
            const double y = pt.value(kYKey).toDouble();
            const double x = pt.value(kXKey).toDouble();
            if (y < yMin)
                yMin = y;
            if (y > yMax)
                yMax = y;
            if (x < xMin)
                xMin = x;
            if (x > xMax)
                xMax = x;
        }
    }

    if (yMin > yMax) {
        const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        return {{QStringLiteral("yMin"), 0.0},
                {QStringLiteral("yMax"), 1.0},
                {QStringLiteral("xMin"), static_cast<double>(nowMs)},
                {QStringLiteral("xMax"), static_cast<double>(nowMs + 1)}};
    }

    const double yRange = yMax - yMin;
    const double yPad   = yRange > 0.0 ? yRange * 0.1 : 1.0;

    if (xMin > xMax) {
        const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        xMin = static_cast<double>(nowMs);
        xMax = static_cast<double>(nowMs + 1);
    } else {
        const double xRange = xMax - xMin;
        const double xPad   = xRange > 0.0 ? xRange * 0.05 : 60'000.0;
        xMin -= xPad;
        xMax += xPad;
    }

    return {{QStringLiteral("yMin"), yMin - yPad},
            {QStringLiteral("yMax"), yMax + yPad},
            {QStringLiteral("xMin"), xMin},
            {QStringLiteral("xMax"), xMax}};
}

QVariantMap snapTrendingChart(const QVariantList &trendingSeries,
                              double axisXMinMs,
                              double axisXMaxMs,
                              double plotX,
                              double plotY,
                              double plotW,
                              double plotH,
                              double mouseX,
                              double mouseY)
{
    if (!mouseInPlotArea(plotX, plotY, plotW, plotH, mouseX, mouseY))
        return {};
    if (trendingSeries.isEmpty())
        return {};

    const double ratio = (mouseX - plotX) / plotW;
    const qint64 tsMs = static_cast<qint64>(axisXMinMs + ratio * (axisXMaxMs - axisXMinMs));

    const QVariantList anchorPts = trendingSeries.first().toMap().value(kPointsKey).toList();
    const int idx = nearestTrendingPointIndex(anchorPts, tsMs);
    if (idx < 0)
        return {};

    QString caption;
    QVariantList valueRows;
    double anchorX = 0.0;
    double anchorY = 0.0;

    for (const QVariant &sv : trendingSeries) {
        const QVariantMap series = sv.toMap();
        const QVariantList pts = series.value(kPointsKey).toList();
        if (idx >= pts.size())
            continue;

        const QVariantMap pt = pts.at(idx).toMap();
        if (caption.isEmpty())
            caption = pt.value(kTimeKey).toString();
        const int decimals = qBound(0, series.value(QStringLiteral("decimals"), 4).toInt(), 6);
        QVariantMap row;
        row.insert(QStringLiteral("text"),
                   QStringLiteral("%1: %2")
                       .arg(series.value(QStringLiteral("label")).toString())
                       .arg(QString::number(pt.value(kYKey).toDouble(), 'f', decimals)));
        row.insert(QStringLiteral("seriesIndex"), valueRows.size());
        valueRows.append(row);

        if (valueRows.size() == 1) {
            anchorX = pt.value(kXKey).toDouble();
            anchorY = pt.value(kYKey).toDouble();
        }
    }

    if (valueRows.isEmpty())
        return {};

    QVariantMap position;
    position.insert(kXKey, anchorX);
    position.insert(kYKey, anchorY);

    QVariantMap hit;
    hit.insert(QStringLiteral("position"), position);
    hit.insert(QStringLiteral("captionText"), caption);
    hit.insert(QStringLiteral("valueRows"), valueRows);
    return hit;
}

} // namespace CentralLogger::Core
