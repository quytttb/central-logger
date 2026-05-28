#include "SensorCatalogRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtDebug>

namespace CentralLogger::Data {

namespace {

QVariant optDouble(const std::optional<double> &v)
{
    return v ? QVariant(*v) : QVariant(QMetaType(QMetaType::Double));
}

std::optional<double> readOptDouble(const QVariant &v)
{
    if (v.isNull()) {
        return std::nullopt;
    }
    return v.toDouble();
}

LoggerSensor rowToModel(const QSqlQuery &q)
{
    LoggerSensor s;
    s.id            = q.value(QStringLiteral("id")).toLongLong();
    s.loggerId      = q.value(QStringLiteral("logger_id")).toLongLong();
    s.edgeSensorId  = q.value(QStringLiteral("edge_sensor_id")).toInt();
    s.sensorType    = q.value(QStringLiteral("sensor_type")).toString();
    s.name          = q.value(QStringLiteral("name")).toString();
    s.unit          = q.value(QStringLiteral("unit")).toString();
    s.minThreshold  = readOptDouble(q.value(QStringLiteral("min_threshold")));
    s.maxThreshold  = readOptDouble(q.value(QStringLiteral("max_threshold")));
    s.active        = q.value(QStringLiteral("active")).toInt() != 0;
    const QVariant parentV = q.value(QStringLiteral("parent_edge_sensor_id"));
    if (!parentV.isNull()) {
        s.parentEdgeSensorId = parentV.toInt();
    }
    s.diType = q.value(QStringLiteral("di_type")).toString();
    return s;
}

// M-2: always emit a qWarning so the error surfaces even when the caller
// does not provide an errorOut string (silent failures are hard to diagnose
// in production logs).
void setErr(QString *out, const QSqlQuery &q, const char *context = nullptr)
{
    const QString text = q.lastError().text();
    if (out) {
        *out = text;
    } else if (!text.isEmpty()) {
        qWarning() << (context ? context : "SensorCatalogRepository")
                   << "SQL error:" << text;
    }
}

} // namespace

qint64 SensorCatalogRepository::ensureExists(qint64 loggerId,
                                             int edgeSensorId,
                                             const QString &sensorType,
                                             QString *errorOut)
{
    // C-4 fix: atomic upsert — no TOCTOU window.
    // D-1 fix: if the sensor already exists with active=0 (after a prune),
    // a new poll/reading returning this edge_sensor_id means the device has
    // re-added it. Set active=1 so it reappears in live tables immediately.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO logger_sensor "
        "  (logger_id, edge_sensor_id, sensor_type, name, unit, active) "
        "VALUES (:logger_id, :edge_sensor_id, :sensor_type, '', '', 1) "
        "ON CONFLICT(logger_id, sensor_type, edge_sensor_id) DO UPDATE SET active = 1"));
    q.bindValue(QStringLiteral(":logger_id"),      loggerId);
    q.bindValue(QStringLiteral(":edge_sensor_id"), edgeSensorId);
    q.bindValue(QStringLiteral(":sensor_type"),    sensorType);
    if (!q.exec()) {
        setErr(errorOut, q, "SensorCatalogRepository::ensureExists");
        return 0;
    }

    // Re-query to get the id whether the row was just inserted or already existed.
    const auto existing = findByLoggerAndEdgeId(loggerId, edgeSensorId, sensorType, errorOut);
    if (!existing) {
        return 0;
    }
    return existing->id;
}

bool SensorCatalogRepository::upsert(LoggerSensor &sensor, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO logger_sensor ("
        "  logger_id, edge_sensor_id, sensor_type, name, unit,"
        "  min_threshold, max_threshold, active,"
        "  parent_edge_sensor_id, di_type"
        ") VALUES ("
        "  :logger_id, :edge_sensor_id, :sensor_type, :name, :unit,"
        "  :min_threshold, :max_threshold, :active,"
        "  :parent_edge_sensor_id, :di_type"
        ") "
        // M-5: sensor_type is part of the UNIQUE conflict key and cannot
        // change on update (doing so would only write the same value back).
        // Removed to avoid the no-op assignment and keep the intent clear.
        "ON CONFLICT(logger_id, sensor_type, edge_sensor_id) DO UPDATE SET "
        "  name                  = excluded.name,"
        "  unit                  = excluded.unit,"
        "  min_threshold         = excluded.min_threshold,"
        "  max_threshold         = excluded.max_threshold,"
        "  active                = excluded.active,"
        "  parent_edge_sensor_id = excluded.parent_edge_sensor_id,"
        "  di_type               = excluded.di_type"));
    q.bindValue(QStringLiteral(":logger_id"),      sensor.loggerId);
    q.bindValue(QStringLiteral(":edge_sensor_id"), sensor.edgeSensorId);
    q.bindValue(QStringLiteral(":sensor_type"),    sensor.sensorType);
    q.bindValue(QStringLiteral(":name"),           sensor.name);
    q.bindValue(QStringLiteral(":unit"),           sensor.unit);
    q.bindValue(QStringLiteral(":min_threshold"),  optDouble(sensor.minThreshold));
    q.bindValue(QStringLiteral(":max_threshold"),  optDouble(sensor.maxThreshold));
    q.bindValue(QStringLiteral(":active"),         sensor.active ? 1 : 0);
    q.bindValue(QStringLiteral(":parent_edge_sensor_id"),
                sensor.parentEdgeSensorId.has_value()
                    ? QVariant(*sensor.parentEdgeSensorId)
                    : QVariant(QMetaType(QMetaType::Int)));
    q.bindValue(QStringLiteral(":di_type"),
                sensor.diType.isEmpty() ? QVariant() : sensor.diType);

    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }

    // lastInsertId is only valid for INSERT; on UPDATE we re-query the id.
    auto existing = findByLoggerAndEdgeId(sensor.loggerId, sensor.edgeSensorId,
                                          sensor.sensorType, errorOut);
    if (!existing) {
        return false;
    }
    sensor.id = existing->id;
    return true;
}

std::optional<LoggerSensor>
SensorCatalogRepository::findByLoggerAndEdgeId(qint64 loggerId,
                                               int edgeSensorId,
                                               const QString &sensorType,
                                               QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM logger_sensor "
        "WHERE logger_id = :lid AND edge_sensor_id = :eid AND sensor_type = :stype"));
    q.bindValue(QStringLiteral(":lid"),   loggerId);
    q.bindValue(QStringLiteral(":eid"),   edgeSensorId);
    q.bindValue(QStringLiteral(":stype"), sensorType);
    if (!q.exec()) {
        setErr(errorOut, q);
        return std::nullopt;
    }
    if (!q.next()) {
        return std::nullopt;
    }
    return rowToModel(q);
}

QVector<LoggerSensor>
SensorCatalogRepository::listByLoggerId(qint64 loggerId, QString *errorOut) const
{
    QVector<LoggerSensor> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM logger_sensor WHERE logger_id = :lid ORDER BY edge_sensor_id"));
    q.bindValue(QStringLiteral(":lid"), loggerId);
    if (!q.exec()) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        result.append(rowToModel(q));
    }
    return result;
}

int SensorCatalogRepository::pruneOrphanSensors(qint64 loggerId,
                                                const QVector<int> &liveAnalogEdgeIds,
                                                int maxDi,
                                                int maxDo,
                                                QString *errorOut)
{
    int totalDeactivated = 0;

    // ANALOG: edge_sensor_id is the wire sensor_id, not the block index Na.
    if (!liveAnalogEdgeIds.isEmpty()) {
        QStringList placeholders;
        placeholders.reserve(liveAnalogEdgeIds.size());
        for (int i = 0; i < liveAnalogEdgeIds.size(); ++i) {
            placeholders.append(QStringLiteral(":a%1").arg(i));
        }
        const QString sql = QStringLiteral(
            "UPDATE logger_sensor SET active = 0 "
            "WHERE logger_id = :lid AND sensor_type = 'ANALOG' AND active != 0 "
            "AND edge_sensor_id NOT IN (%1)").arg(placeholders.join(QLatin1Char(',')));

        QSqlQuery q(m_db);
        q.prepare(sql);
        q.bindValue(QStringLiteral(":lid"), loggerId);
        for (int i = 0; i < liveAnalogEdgeIds.size(); ++i) {
            q.bindValue(QStringLiteral(":a%1").arg(i), liveAnalogEdgeIds.at(i));
        }
        if (!q.exec()) {
            setErr(errorOut, q);
            return -1;
        }
        totalDeactivated += q.numRowsAffected();
    }

    struct TypeLimit { const char *type; int max; };
    const TypeLimit digitalLimits[] = {
        {"DI", maxDi},
        {"DO", maxDo},
    };

    // C-8 fix: set active=0 instead of DELETE to preserve sensor_reading history.
    for (const auto &tl : digitalLimits) {
        if (tl.max < 0 || tl.max == 0) {
            continue; // -1 = skip; 0 = Ndi/Ndo zero → no digital read this cycle
        }
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE logger_sensor SET active = 0 "
            "WHERE logger_id = :lid AND sensor_type = :stype AND edge_sensor_id >= :max"
            "  AND active != 0"));
        q.bindValue(QStringLiteral(":lid"),   loggerId);
        q.bindValue(QStringLiteral(":stype"), QString::fromLatin1(tl.type));
        q.bindValue(QStringLiteral(":max"),   tl.max);
        if (!q.exec()) {
            setErr(errorOut, q);
            return -1;
        }
        totalDeactivated += q.numRowsAffected();
    }
    return totalDeactivated;
}

} // namespace CentralLogger::Data
