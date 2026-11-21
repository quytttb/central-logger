#include "GraphHistoryQueryService.h"

#include "data/db/Database.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "utils/SensorConstants.h"
#include "utils/time/DateTimeUtils.h"

#include <QSqlQuery>
#include <QTimeZone>

namespace CentralLogger::Core {

GraphHistoryResult GraphHistoryQueryService::query(const QString &databasePath, qint64 loggerId,
                                                   qint64 fromMs, qint64 toMs,
                                                   const std::atomic_bool &cancelled)
{
    GraphHistoryResult result;
    if (cancelled.load(std::memory_order_relaxed))
        return result;

    const QString from = Utils::isoUtc(QDateTime::fromMSecsSinceEpoch(fromMs, QTimeZone::UTC));
    const QString to = Utils::isoUtc(QDateTime::fromMSecsSinceEpoch(toMs, QTimeZone::UTC));
    Data::Database::withScopedConnection(QStringLiteral("graph_history"), databasePath,
        [&](QSqlDatabase &db) {
            const auto sensors = Data::SensorCatalogRepository(db).listByLoggerId(loggerId, &result.error);
            if (!result.error.isEmpty())
                return;

            // Use the (sensor_id, recorded_at) index. Process one sensor at a
            // time; don't materialize all rows from a dense multi-day window.
            QSqlQuery query(db);
            query.setForwardOnly(true);
            if (!query.prepare(QStringLiteral(
                    "SELECT recorded_at, value FROM %1 "
                    "WHERE sensor_id = :sensor_id AND recorded_at >= :from AND recorded_at <= :to "
                    "AND status_code != :status AND status_code != :status_code "
                    "ORDER BY recorded_at, id").arg(QLatin1String(Data::Db::kTableSensorReading)))) {
                result.error = query.lastError().text();
                return;
            }
            for (const auto &sensor : sensors) {
                if (cancelled.load(std::memory_order_relaxed))
                    return;
                if (!sensor.active || sensor.sensorType != QLatin1String(Sensor::kTypeAnalog))
                    continue;
                query.bindValue(QLatin1String(Data::Db::kBindSensorId), sensor.id);
                query.bindValue(QLatin1String(Data::Db::kBindFrom), from);
                query.bindValue(QLatin1String(Data::Db::kBindTo), to);
                query.bindValue(QLatin1String(Data::Db::kBindStatus), QLatin1String(Sensor::kStatusErr));
                query.bindValue(QLatin1String(Data::Db::kBindStatusCode), QLatin1String(Sensor::kLegacyStatusStale));
                if (!query.exec()) {
                    result.error = query.lastError().text();
                    return;
                }
                Downsampler sampler(fromMs, toMs);
                while (query.next()) {
                    if (cancelled.load(std::memory_order_relaxed))
                        return;
                    const auto time = Utils::parseUtc(query.value(0).toString());
                    // v8 does not persist the Modbus alarm bit. Never
                    // reconstruct an alarm from a current threshold or TT10.
                    if (time.isValid())
                        sampler.append({time.toMSecsSinceEpoch(), query.value(1).toDouble(), false});
                }
                if (query.lastError().isValid()) {
                    result.error = query.lastError().text();
                    return;
                }
                auto points = sampler.takePoints();
                if (!points.isEmpty())
                    result.series.insert(sensor.edgeSensorId, std::move(points));
            }
        }, &result.error);
    if (!result.error.isEmpty())
        result.series.clear();
    return result;
}

} // namespace CentralLogger::Core
