#pragma once

#include "data/models/LoggerSensor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace CentralLogger::Network::RestConfigParser {

/// Result of parsing a `GET /config` response. `revision` is -1 when the
/// payload doesn't carry one (caller should keep the previous value).
struct ConfigPayload
{
    int                                  revision = -1;
    QVector<CentralLogger::Data::LoggerSensor> sensors;
    QJsonObject                          configObject; // `config` subtree for POST replay
    /// Edge Modbus TCP unit ID from `config.modbus_tcp_unit_id` (-1 if absent).
    int                                  modbusTcpUnitId = -1;
    bool                                 valid    = false;
};

/// Result of parsing a `POST /config` response. `appliedRevision` is the
/// edge-confirmed revision when `ok` is true.
struct ApplyResult
{
    bool ok               = false;
    int  appliedRevision  = -1;
};

namespace detail {

inline QString readStr(const QJsonObject &o, const char *key)
{
    const auto v = o.value(QLatin1String(key));
    if (v.isString()) return v.toString();
    return {};
}

inline int readInt(const QJsonObject &o, const char *key, int fallback = -1)
{
    const auto v = o.value(QLatin1String(key));
    if (v.isDouble()) return v.toInt();
    return fallback;
}

inline std::optional<double> readOptDouble(const QJsonObject &o, const char *key)
{
    const auto v = o.value(QLatin1String(key));
    if (v.isDouble()) return v.toDouble();
    return std::nullopt;
}

inline QString normaliseType(const QString &raw)
{
    const QString upper = raw.trimmed().toUpper();
    if (upper == QLatin1String("ANALOG")
     || upper == QLatin1String("DI")
     || upper == QLatin1String("DO")) {
        return upper;
    }
    return QStringLiteral("UNKNOWN");
}

/// Walks an array of sensor objects and converts each to a LoggerSensor.
inline QVector<Data::LoggerSensor> readSensors(qint64 loggerId, const QJsonArray &arr)
{
    QVector<Data::LoggerSensor> out;
    out.reserve(arr.size());
    for (const auto &v : arr) {
        if (!v.isObject()) continue;
        const auto obj = v.toObject();
        Data::LoggerSensor s;
        s.loggerId     = loggerId;
        s.sensorType   = normaliseType(readStr(obj, "sensor_type"));
        if (s.sensorType == QLatin1String("DI") || s.sensorType == QLatin1String("DO")) {
            s.edgeSensorId = readInt(obj, "register_address", -1);
            if (s.edgeSensorId < 0) {
                s.edgeSensorId = readInt(obj, "sensor_id", -1);
            }
            if (s.edgeSensorId < 0) {
                s.edgeSensorId = readInt(obj, "id", -1);
            }
        } else {
            s.edgeSensorId = readInt(obj, "sensor_id", -1);
            if (s.edgeSensorId < 0) {
                s.edgeSensorId = readInt(obj, "id", -1);
            }
        }
        if (s.edgeSensorId < 0) continue;
        s.name         = readStr(obj, "name");
        s.unit         = readStr(obj, "unit");
        s.minThreshold = readOptDouble(obj, "min_threshold");
        s.maxThreshold = readOptDouble(obj, "max_threshold");
        const auto activeVal = obj.value(QLatin1String("active"));
        s.active             = activeVal.isBool() ? activeVal.toBool() : true;
        const int parentId   = readInt(obj, "parent_id", -1);
        if (parentId >= 0) {
            s.parentEdgeSensorId = parentId;
        }
        const QString diType = readStr(obj, "di_type").trimmed();
        if (!diType.isEmpty()) {
            s.diType = diType;
        }
        if (s.sensorType == QLatin1String("DI")) {
            if (parentId >= 0) {
                s.allParentIds.append(parentId);
            }
            const auto analogIdsVal = obj.value(QLatin1String("analog_ids"));
            if (analogIdsVal.isArray()) {
                for (const auto &aidVal : analogIdsVal.toArray()) {
                    const int aid = aidVal.isDouble() ? aidVal.toInt() : -1;
                    if (aid >= 0 && !s.allParentIds.contains(aid)) {
                        s.allParentIds.append(aid);
                    }
                }
            }
        }
        out.append(s);
    }
    return out;
}

inline bool isRevisionConflict422(const QByteArray &body, const QString &lower)
{
    if (lower.contains(QLatin1String("revision mismatch"))) {
        return true;
    }
    if (lower.contains(QLatin1String("revision")) && lower.contains(QLatin1String("conflict"))) {
        return true;
    }
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        return false;
    }
    const auto detailVal = doc.object().value(QLatin1String("detail"));
    if (detailVal.isString()) {
        const QString s = detailVal.toString().toLower();
        return s.contains(QLatin1String("revision mismatch"))
            || (s.contains(QLatin1String("revision")) && s.contains(QLatin1String("conflict")));
    }
    if (!detailVal.isArray()) {
        return false;
    }
    for (const auto &item : detailVal.toArray()) {
        if (!item.isObject()) {
            continue;
        }
        const auto obj  = item.toObject();
        const QString msg = obj.value(QLatin1String("msg")).toString().toLower();
        if (msg.contains(QLatin1String("revision mismatch"))
            || msg.contains(QLatin1String("revision conflict"))) {
            return true;
        }
    }
    return false;
}

inline bool isMissingConfigFields422(const QByteArray &body)
{
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        return false;
    }
    const auto detailVal = doc.object().value(QLatin1String("detail"));
    if (!detailVal.isArray()) {
        return false;
    }
    for (const auto &item : detailVal.toArray()) {
        if (!item.isObject()) {
            continue;
        }
        if (item.toObject().value(QLatin1String("type")).toString() != QLatin1String("missing")) {
            continue;
        }
        const auto loc = item.toObject().value(QLatin1String("loc"));
        if (!loc.isArray()) {
            continue;
        }
        for (const auto &locPart : loc.toArray()) {
            const QString part = locPart.toString();
            if (part == QLatin1String("api_version") || part == QLatin1String("request_id")) {
                return true;
            }
        }
    }
    return false;
}

} // namespace detail

/// Parses the response body of `GET /config`. Accepts the edge spec where
/// `sensors[]` lives either at the root or under `config`. Returns a payload
/// with `valid=false` on invalid JSON.
inline ConfigPayload parseConfigResponse(qint64 loggerId, const QByteArray &body)
{
    ConfigPayload out;
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return out;
    const auto root = doc.object();
    out.valid = true;

    out.revision = detail::readInt(root, "revision", -1);
    if (out.revision < 0) {
        // Some firmwares mirror the applied revision on GET.
        out.revision = detail::readInt(root, "applied_revision", -1);
    }

    if (root.value(QLatin1String("config")).isObject()) {
        out.configObject = root.value(QLatin1String("config")).toObject();
    }
    out.modbusTcpUnitId = detail::readInt(out.configObject, "modbus_tcp_unit_id", -1);

    QJsonArray sensorsArr;
    if (root.value(QLatin1String("sensors")).isArray()) {
        sensorsArr = root.value(QLatin1String("sensors")).toArray();
    } else if (out.configObject.value(QLatin1String("sensors")).isArray()) {
        sensorsArr = out.configObject.value(QLatin1String("sensors")).toArray();
    }
    out.sensors = detail::readSensors(loggerId, sensorsArr);
    return out;
}

/// Parses the response body of `POST /config`. The edge contract returns
/// `ok` plus `applied_revision`.
inline ApplyResult parseApplyResponse(const QByteArray &body)
{
    ApplyResult out;
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return out;
    const auto root  = doc.object();
    const auto okVal = root.value(QLatin1String("ok"));
    out.ok           = okVal.isBool() && okVal.toBool();
    out.appliedRevision = detail::readInt(root, "applied_revision", -1);
    return out;
}

/// Maps the contract's error table to a user-facing string. `httpStatus = 0`
/// means a transport error (timeout, DNS, refused) — the caller should pass
/// the QNetworkReply::errorString() in `body` for that case.
inline QString formatRestError(int httpStatus, const QByteArray &body, const QString &transportError = {})
{
    const QString bodyText = QString::fromUtf8(body);
    const QString lower    = bodyText.toLower();

    if (httpStatus == 0) {
        return transportError.isEmpty()
            ? QStringLiteral("Could not reach the logger. Check host, API port, and network.")
            : QStringLiteral("Could not reach the logger: %1").arg(transportError);
    }
    if (httpStatus == 401) {
        if (lower.contains(QLatin1String("not configured"))) {
            return QStringLiteral("Device REST token empty — Scan QR on logger");
        }
        if (lower.contains(QLatin1String("invalid bearer"))
         || lower.contains(QLatin1String("invalid token"))) {
            return QStringLiteral("Token mismatch — Scan QR again on device");
        }
        return QStringLiteral("REST unauthorized (401)");
    }
    if (httpStatus == 404) {
        return QStringLiteral("Logger API not available. Update data-logger firmware.");
    }
    if (httpStatus == 409
     || (httpStatus == 422 && detail::isRevisionConflict422(body, lower))) {
        return QStringLiteral("Configuration changed on device. Connect again, then save.");
    }
    if (httpStatus == 422) {
        if (detail::isMissingConfigFields422(body)) {
            return QStringLiteral(
                "Device rejected config request (missing fields). Update Central Logger.");
        }
        return QStringLiteral("Edge rejected payload (422). Check forbidden fields.");
    }

    // Generic fallback: try to surface `errors[0].message` if present.
    const auto doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const auto errs = doc.object().value(QLatin1String("errors"));
        if (errs.isArray() && !errs.toArray().isEmpty()) {
            const auto first = errs.toArray().first();
            if (first.isObject()) {
                const auto msg = first.toObject().value(QLatin1String("message")).toString();
                if (!msg.isEmpty()) {
                    return QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(msg);
                }
            }
        }
        const auto detail = doc.object().value(QLatin1String("detail")).toString();
        if (!detail.isEmpty()) {
            return QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(detail);
        }
    }

    return QStringLiteral("HTTP %1").arg(httpStatus);
}

/// Maps HTTP errors for `GET /reports/latest`. A 404 on this route (after Bearer
/// auth) usually means no report artifact yet — the route itself returns 401
/// when unauthenticated.
inline QString formatReportDownloadError(int httpStatus, const QByteArray &body,
                                         const QString &transportError = {})
{
    if (httpStatus == 404) {
        const QString lower = QString::fromUtf8(body).toLower();
        if (lower.contains(QLatin1String("report"))
            || lower.contains(QLatin1String("not found"))
            || body.trimmed().isEmpty()) {
            return QStringLiteral(
                "No latest report on device. Generate a report on the data-logger first.");
        }
        return QStringLiteral("Report endpoint not found. Update data-logger firmware.");
    }
    return formatRestError(httpStatus, body, transportError);
}

/// Pretty-prints `body` for the debug dialog. Falls back to UTF-8 text when
/// the body isn't valid JSON.
inline QString prettyJson(const QByteArray &body)
{
    const auto doc = QJsonDocument::fromJson(body);
    if (doc.isNull()) {
        return QString::fromUtf8(body);
    }
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

} // namespace CentralLogger::Network::RestConfigParser
