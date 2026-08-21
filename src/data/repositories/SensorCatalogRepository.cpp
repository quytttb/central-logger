#include "SensorCatalogRepository.h"

#include "utils/AppConstants.h"
#include "utils/DbConstants.h"
#include "utils/SensorConstants.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtDebug>

#include <algorithm>

namespace CentralLogger::Data {

using CentralLogger::Defaults::kDecimalsMax;
using CentralLogger::Defaults::kDecimalsMin;

namespace {

QString serializeParentIds(const QVector<int> &ids)
{
    if (ids.isEmpty()) return {};
    QJsonArray arr;
    for (int id : ids) arr.append(id);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<int> deserializeParentIds(const QVariant &v)
{
    if (v.isNull() || !v.isValid()) return {};
    const QString json = v.toString();
    if (json.isEmpty()) return {};
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return {};
    QVector<int> result;
    for (const auto &item : doc.array()) {
        if (item.isDouble()) result.append(item.toInt());
    }
    return result;
}

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

// Column positions for the SELECT * FROM logger_sensor lists used by
// rowToModel. Using positional access avoids qt.sql.qsqlquery "unknown
// field name" warnings on prepared queries and is faster than named lookups.
enum ColSensor {
    ColSensorId = 0,
    ColSensorLoggerId,
    ColSensorEdgeSensorId,
    ColSensorSensorType,
    ColSensorName,
    ColSensorUnit,
    ColSensorMinThreshold,
    ColSensorMaxThreshold,
    ColSensorDecimals,
    ColSensorActive,
    ColSensorParentEdgeSensorId,
    ColSensorDiType,
    ColSensorAllParentIds,
};

LoggerSensor rowToModel(const QSqlQuery &q)
{
    LoggerSensor s;
    s.id            = q.value(ColSensorId).toLongLong();
    s.loggerId      = q.value(ColSensorLoggerId).toLongLong();
    s.edgeSensorId  = q.value(ColSensorEdgeSensorId).toInt();
    s.sensorType    = q.value(ColSensorSensorType).toString();
    s.name          = q.value(ColSensorName).toString();
    s.unit          = q.value(ColSensorUnit).toString();
    s.minThreshold  = readOptDouble(q.value(ColSensorMinThreshold));
    s.maxThreshold  = readOptDouble(q.value(ColSensorMaxThreshold));
    s.decimals      = q.value(ColSensorDecimals).toInt();
    s.active        = q.value(ColSensorActive).toInt() != 0;
    const QVariant parentV = q.value(ColSensorParentEdgeSensorId);
    if (!parentV.isNull()) {
        s.parentEdgeSensorId = parentV.toInt();
    }
    s.diType       = q.value(ColSensorDiType).toString();
    s.allParentIds = deserializeParentIds(q.value(ColSensorAllParentIds));
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
        "INSERT INTO %1 "
        "  (logger_id, edge_sensor_id, sensor_type, name, unit, active) "
        "VALUES (:logger_id, :edge_sensor_id, :sensor_type, '', '', 1) "
        "ON CONFLICT(logger_id, sensor_type, edge_sensor_id) DO UPDATE SET active = 1")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId),      loggerId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEdgeSensorId), edgeSensorId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorType),    sensorType);
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
        "INSERT INTO %1 ("
        "  logger_id, edge_sensor_id, sensor_type, name, unit,"
        "  min_threshold, max_threshold, decimals, active,"
        "  parent_edge_sensor_id, di_type, all_parent_ids"
        ") VALUES ("
        "  :logger_id, :edge_sensor_id, :sensor_type, :name, :unit,"
        "  :min_threshold, :max_threshold, :decimals, :active,"
        "  :parent_edge_sensor_id, :di_type, :all_parent_ids"
        ") "
        // M-5: sensor_type is part of the UNIQUE conflict key and cannot
        // change on update (doing so would only write the same value back).
        // Removed to avoid the no-op assignment and keep the intent clear.
        "ON CONFLICT(logger_id, sensor_type, edge_sensor_id) DO UPDATE SET "
        "  name                  = excluded.name,"
        "  unit                  = excluded.unit,"
        "  min_threshold         = excluded.min_threshold,"
        "  max_threshold         = excluded.max_threshold,"
        "  decimals              = excluded.decimals,"
        "  active                = excluded.active,"
        "  parent_edge_sensor_id = excluded.parent_edge_sensor_id,"
        "  di_type               = excluded.di_type,"
        "  all_parent_ids        = excluded.all_parent_ids")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId),      sensor.loggerId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEdgeSensorId), sensor.edgeSensorId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorType),    sensor.sensorType);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindName),           sensor.name);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindUnit),           sensor.unit);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindMinThreshold),  optDouble(sensor.minThreshold));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindMaxThreshold),  optDouble(sensor.maxThreshold));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindDecimals),
                std::clamp(sensor.decimals, kDecimalsMin, kDecimalsMax));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindActive),         sensor.active ? 1 : 0);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindParentEdgeSensorId),
                sensor.parentEdgeSensorId.has_value()
                    ? QVariant(*sensor.parentEdgeSensorId)
                    : QVariant(QMetaType(QMetaType::Int)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindDiType),
                sensor.diType.isEmpty() ? QVariant() : sensor.diType);
    {
        const QString ids = serializeParentIds(sensor.allParentIds);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindAllParentIds), ids.isEmpty() ? QVariant() : ids);
    }

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
        "SELECT * FROM %1 "
        "WHERE logger_id = :lid AND edge_sensor_id = :eid AND sensor_type = :stype")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLid),   loggerId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEid),   edgeSensorId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStype), sensorType);
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
        "SELECT * FROM %1 WHERE logger_id = :lid ORDER BY edge_sensor_id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLid), loggerId);
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
            "UPDATE %1 SET active = 0 "
            "WHERE logger_id = :lid AND sensor_type = '%2' AND active != 0 "
            "AND edge_sensor_id NOT IN (%3)")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 CentralLogger::Sensor::kTypeAnalog,
                 placeholders.join(QLatin1Char(',')));

        QSqlQuery q(m_db);
        q.prepare(sql);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLid), loggerId);
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
        { CentralLogger::Sensor::kTypeDi, maxDi },
        { CentralLogger::Sensor::kTypeDo, maxDo },
    };

    // C-8 fix: set active=0 instead of DELETE to preserve sensor_reading history.
    for (const auto &tl : digitalLimits) {
        if (tl.max < 0 || tl.max == 0) {
            continue; // -1 = skip; 0 = Ndi/Ndo zero → no digital read this cycle
        }
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE %1 SET active = 0 "
            "WHERE logger_id = :lid AND sensor_type = :stype AND edge_sensor_id >= :max"
            "  AND active != 0")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor)));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLid),   loggerId);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStype), QString::fromLatin1(tl.type));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindMax),   tl.max);
        if (!q.exec()) {
            setErr(errorOut, q);
            return -1;
        }
        totalDeactivated += q.numRowsAffected();
    }
    return totalDeactivated;
}

} // namespace CentralLogger::Data
