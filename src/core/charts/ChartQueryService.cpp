#include "ChartQueryService.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtDebug>

namespace CentralLogger::Core {

QVector<ReadingBucketPoint> ChartQueryService::readingCountsLast24h(int bucketMinutes, QTimeZone tz) const
{
    QVector<ReadingBucketPoint> result;
    if (bucketMinutes < 1) bucketMinutes = 5;
    if (!tz.isValid()) tz = QTimeZone::systemTimeZone();

    // recorded_at is stored as ISO-8601 UTC (see SensorReadingRepository).
    // Compare against the same format — NOT datetime('now', …) which uses
    // "YYYY-MM-DD HH:MM:SS" and breaks lexicographic filtering.
    const QString cutoffUtc =
        QDateTime::currentDateTimeUtc().addSecs(-24 * 3600).toString(Qt::ISODateWithMs);
    const int bucketSec = bucketMinutes * 60;

    // recorded_at is stored as ISO-8601 with 'T' separator and 'Z' suffix
    // (e.g. "2024-01-15T08:30:00.000Z"). Older SQLite builds do not accept
    // the T/Z form in strftime, so we normalise with replace() first.
    // replace(replace(t,'T',' '),'Z','') yields "2024-01-15 08:30:00.000"
    // which all SQLite versions parse correctly.
    constexpr auto kNorm = "CAST(strftime('%s', "
                           "  replace(replace(recorded_at,'T',' '),'Z','')) "
                           "AS INTEGER)";

    // L-20: Fetch the raw bucket unix timestamp so C++ can convert it to the
    // configured system_timezone before formatting the "HH:mm" label.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT (%1 / :bucket) * :bucket AS bucket_ts, "
        "       COUNT(*) AS cnt "
        "FROM sensor_reading "
        "WHERE recorded_at >= :cutoff "
        "GROUP BY (%1 / :bucket) "
        "ORDER BY (%1 / :bucket) ASC").arg(QString::fromLatin1(kNorm)));
    q.bindValue(QStringLiteral(":bucket"), bucketSec);
    q.bindValue(QStringLiteral(":cutoff"), cutoffUtc);

    if (!q.exec()) {
        qWarning() << "ChartQueryService::readingCountsLast24h SQL error:"
                    << q.lastError().text();
        return result;
    }

    while (q.next()) {
        ReadingBucketPoint pt;
        const qint64 bucketTs = q.value(0).toLongLong();
        // Convert UTC unix timestamp to the target timezone for display.
        pt.bucketMs = bucketTs * 1000;
        pt.label = QDateTime::fromSecsSinceEpoch(bucketTs, tz)
                       .toString(QStringLiteral("HH:mm"));
        pt.count = q.value(1).toInt();
        result.append(pt);
    }

    return result;
}

} // namespace CentralLogger::Core
