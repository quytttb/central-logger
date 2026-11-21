#include "core/history/AverageExportService.h"

#include "utils/AppConstants.h"
#include "utils/CsvUtils.h"
#include "utils/DbConstants.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace CentralLogger::Core {

namespace {

using CentralLogger::Defaults::kHourBucketSeconds;
using CentralLogger::Utils::csvEscape;

struct AvgAccumulator
{
    double sum   = 0.0;
    int    count = 0;

    void add(const AvgAccumulator &bucket)
    {
        sum += bucket.sum;
        count += bucket.count;
    }
};

struct SensorColumn
{
    qint64  sensorId     = 0;
    int     edgeSensorId = 0;
    QString name;
    QString unit;
    int     decimals = CentralLogger::Defaults::kDecimalsDefault;
};

QString formatValue(const AvgAccumulator &acc, int decimals)
{
    if (acc.count <= 0) {
        return QString();
    }
    return CentralLogger::Sensor::formatAnalogValue(acc.sum / acc.count, decimals);
}

QString headerFor(const SensorColumn &col)
{
    return col.unit.isEmpty() ? col.name
                              : QStringLiteral("%1\n%2").arg(col.name, col.unit);
}

/// Local-time hour bucket alignment: recorded_at is stored ISO-8601 UTC.
/// Adding the timezone offset before truncating keeps buckets aligned to
/// local wall-clock hours (Asia/Ho_Chi_Minh = +7, no DST).
QString normalizedEpochSql()
{
    return QStringLiteral(
        "CAST(strftime('%s', replace(replace(r.recorded_at,'T',' '),'Z','')) AS INTEGER)");
}

} // namespace

AverageExportResult AverageExportService::build(const AverageExportParams &params) const
{
    AverageExportResult result;

    if (!m_db.isOpen()) {
        result.error = QStringLiteral("Database not open");
        return result;
    }
    if (params.loggerId <= 0) {
        result.error = QStringLiteral("Select a single logger before exporting averages.");
        return result;
    }
    if (!params.fromUtc.isValid() || !params.toUtc.isValid() ||
        params.fromUtc > params.toUtc) {
        result.error = QStringLiteral("Invalid date range.");
        return result;
    }

    QTimeZone tz = params.timeZone;
    if (!tz.isValid()) {
        tz = QTimeZone::systemTimeZone();
    }

    // --- Columns: active analog sensors of the logger, ordered by edge id ---
    QVector<SensorColumn> columns;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, edge_sensor_id, name, unit, decimals "
            "FROM %1 "
            "WHERE logger_id = :logger_id AND sensor_type = :sensor_type AND active = 1 "
            "ORDER BY edge_sensor_id ASC")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId), params.loggerId);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorType),
                    QLatin1String(CentralLogger::Sensor::kTypeAnalog));
        if (!q.exec()) {
            result.error = q.lastError().text();
            return result;
        }
        while (q.next()) {
            SensorColumn col;
            col.sensorId     = q.value(0).toLongLong();
            col.edgeSensorId = q.value(1).toInt();
            col.name         = q.value(2).toString();
            col.unit         = q.value(3).toString();
            col.decimals     = q.value(4).toInt();
            if (col.name.isEmpty()) {
                col.name = CentralLogger::Sensor::fallbackSensorName(col.edgeSensorId);
            }
            columns.append(col);
        }
    }
    if (columns.isEmpty()) {
        result.error = QStringLiteral("Logger has no analog sensors in the catalog.");
        return result;
    }

    // --- Hourly aggregates per sensor (valid, non-stale readings only) ------
    const int tzOffsetSeconds = tz.offsetFromUtc(params.fromUtc.toTimeZone(tz));

    // sensorId → (bucket epoch seconds → accumulator)
    QHash<qint64, QHash<qint64, AvgAccumulator>> hourly;

    {
        const QString bucketSql = QStringLiteral(
            "((%1 + :offset) / :hour) * :hour - :offset")
            .arg(normalizedEpochSql());

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT s.id, "
            "       %1 AS bucket, "
            "       AVG(r.value) AS avg_value, "
            "       COUNT(*) AS n "
            "FROM %2 r "
            "JOIN %3 s ON r.sensor_id = s.id "
            "WHERE s.logger_id = :logger_id "
            "  AND s.sensor_type = :sensor_type "
            "  AND r.status_code = '%4' "
            "  AND r.recorded_at >= :from AND r.recorded_at <= :to "
            "GROUP BY s.id, bucket "
            "ORDER BY bucket ASC")
            .arg(bucketSql,
                 QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 QLatin1String(CentralLogger::Sensor::kStatusOk)));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindOffset), tzOffsetSeconds);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindHour), kHourBucketSeconds);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId), params.loggerId);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorType),
                    QLatin1String(CentralLogger::Sensor::kTypeAnalog));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindFrom),
                    params.fromUtc.toUTC().toString(Qt::ISODateWithMs));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTo),
                    params.toUtc.toUTC().toString(Qt::ISODateWithMs));
        if (!q.exec()) {
            result.error = q.lastError().text();
            return result;
        }
        while (q.next()) {
            AvgAccumulator acc;
            acc.sum   = q.value(2).toDouble() * q.value(3).toInt();
            acc.count = q.value(3).toInt();
            hourly[q.value(0).toLongLong()].insert(q.value(1).toLongLong(), acc);
        }
    }

    const QDateTime fromLocal = params.fromUtc.toTimeZone(tz);
    const QDateTime toLocal   = params.toUtc.toTimeZone(tz);
    const QDateTime firstHour(fromLocal.date(), QTime(fromLocal.time().hour(), 0), tz);
    if (!firstHour.isValid() || firstHour > toLocal) {
        result.error = QStringLiteral("No time buckets in the selected range.");
        return result;
    }

    // --- Render rows -------------------------------------------------------
    QStringList lines;
    QStringList header;
    header.append(QString::fromUtf8(CentralLogger::Format::kAverageHeaderTime));
    for (const auto &col : columns) {
        header.append(headerFor(col));
    }
    lines.append([&header]() {
        QStringList escaped;
        for (const auto &h : header) {
            escaped.append(csvEscape(h));
        }
        return escaped.join(QLatin1Char(','));
    }());

    const qint64 offset = tzOffsetSeconds;
    const qint64 hourSec = kHourBucketSeconds;

    auto bucketKey = [offset, hourSec](const QDateTime &utc) {
        const qint64 secs = utc.toSecsSinceEpoch();
        return ((secs + offset) / hourSec) * hourSec - offset;
    };

    QVector<AvgAccumulator> totals(columns.size());
    int dataRows = 0;

    for (auto hour = firstHour; hour <= toLocal; hour = hour.addSecs(hourSec)) {
        QStringList cells;
        cells.append(csvEscape(hour.toString(QLatin1String(CentralLogger::Format::kTimeHhMmDdMmYyyy))));
        const auto key = bucketKey(hour.toUTC());

        for (int c = 0; c < columns.size(); ++c) {
            AvgAccumulator acc;
            const auto sensorIt = hourly.constFind(columns.at(c).sensorId);
            if (sensorIt != hourly.constEnd())
                acc = sensorIt->value(key);
            totals[c].add(acc);
            cells.append(formatValue(acc, columns.at(c).decimals));
        }

        lines.append(cells.join(QLatin1Char(',')));
        ++dataRows;
    }

    // --- Overall average row ----------------------------------------------
    QStringList avgCells;
    avgCells.append(csvEscape(QString::fromUtf8(CentralLogger::Format::kAverageLabel24h)));
    for (int c = 0; c < columns.size(); ++c) {
        avgCells.append(formatValue(totals.at(c), columns.at(c).decimals));
    }
    lines.append(avgCells.join(QLatin1Char(',')));

    result.csv        = lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    result.rowCount   = dataRows;
    result.columnCount = columns.size();
    return result;
}

} // namespace CentralLogger::Core
