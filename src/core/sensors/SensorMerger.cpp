#include "core/sensors/SensorMerger.h"

#include "core/sensors/SensorStatusHelper.h"

#include <QHash>
#include <QSet>
#include <algorithm>

namespace CentralLogger::Core {

namespace {

constexpr auto kAnalog = "ANALOG";
constexpr auto kDi     = "DI";
constexpr auto kDo     = "DO";
constexpr auto kUnknown = "UNKNOWN";

QString formatAnalog(float value)
{
    return QString::number(static_cast<double>(value), 'f', 2);
}

QString fallbackName(const QString &sensorType, int edgeSensorId)
{
    if (!sensorType.isEmpty() && sensorType != QLatin1String(kUnknown)) {
        return QStringLiteral("%1#%2").arg(sensorType).arg(edgeSensorId);
    }
    return QStringLiteral("Sensor #%1").arg(edgeSensorId);
}

bool isOutOfRange(double value,
                  const std::optional<double> &minThreshold,
                  const std::optional<double> &maxThreshold)
{
    if (minThreshold.has_value() && value < *minThreshold) {
        return true;
    }
    if (maxThreshold.has_value() && value > *maxThreshold) {
        return true;
    }
    return false;
}

QString computeAlarmType(double value,
                         bool alarmBit,
                         const std::optional<double> &minThreshold,
                         const std::optional<double> &maxThreshold)
{
    bool minBreach = false;
    bool maxBreach = false;
    if (minThreshold.has_value() && value < *minThreshold) {
        minBreach = true;
    }
    if (maxThreshold.has_value() && value > *maxThreshold) {
        maxBreach = true;
    }
    if (minBreach && maxBreach) {
        return QStringLiteral("min+max");
    }
    if (minBreach) {
        return QStringLiteral("min");
    }
    if (maxBreach) {
        return QStringLiteral("max");
    }
    if (alarmBit) {
        return {};
    }
    return {};
}

/// Edge modbus_worker priority: 02 > 03 > 01 > custom > 00.
QString resolveAttachDiCode(const QVector<Data::LoggerSensor> &catalog,
                            int parentEdgeSensorId,
                            const QVector<bool> &diBits)
{
    QString bestCustom;
    bool has01 = false;
    bool has03 = false;

    for (const auto &child : catalog) {
        if (child.sensorType != QLatin1String(kDi)) {
            continue;
        }
        if (!child.active || !child.parentEdgeSensorId.has_value()) {
            continue;
        }
        if (*child.parentEdgeSensorId != parentEdgeSensorId) {
            continue;
        }
        if (child.edgeSensorId < 0 || child.edgeSensorId >= diBits.size()) {
            continue;
        }
        if (!diBits.at(child.edgeSensorId)) {
            continue;
        }

        const QString code = SensorStatusHelper::normalizeDiCode(
            child.diType.isEmpty() ? QStringLiteral("00") : child.diType);
        if (code == QStringLiteral("02")) {
            return code;
        }
        if (code == QStringLiteral("03")) {
            has03 = true;
        } else if (code == QStringLiteral("01")) {
            has01 = true;
        } else if (code != QStringLiteral("00")) {
            if (bestCustom.isEmpty()) {
                bestCustom = code;
            }
        }
    }

    if (has03) {
        return QStringLiteral("03");
    }
    if (has01) {
        return QStringLiteral("01");
    }
    if (!bestCustom.isEmpty()) {
        return bestCustom;
    }
    return QStringLiteral("00");
}

void applyAnalogStatus(SensorLiveRow &row,
                       bool active,
                       bool rtuConnected,
                       bool pollingActive,
                       bool valid,
                       bool stale,
                       bool alarmBit,
                       double value,
                       const std::optional<double> &minThreshold,
                       const std::optional<double> &maxThreshold,
                       const QString &attachDiCode)
{
    row.diStatusCode.clear();
    row.alarmType.clear();
    row.showAlarmBadge = false;

    if (!active) {
        row.displayStatus = QStringLiteral("WAIT");
        return;
    }
    // Edge sets sensor ERR on connection_lost while polling; HR1=0 at TCP
    // warm-up is not the same as RTU down (log evidence: hr1Flags 0→3).
    if (!valid || (!rtuConnected && pollingActive)) {
        row.displayStatus = QStringLiteral("ERR");
        return;
    }
    if (stale) {
        row.displayStatus = QStringLiteral("STALE");
        return;
    }

    const bool alarm = alarmBit || isOutOfRange(value, minThreshold, maxThreshold);
    if (alarm) {
        row.alarmType = computeAlarmType(value, alarmBit, minThreshold, maxThreshold);
    }

    if (SensorStatusHelper::isActiveDiCode(attachDiCode)) {
        row.diStatusCode = attachDiCode;
        row.displayStatus = SensorStatusHelper::labelForDiCode(attachDiCode);
        if (alarm) {
            row.showAlarmBadge = true;
        }
        return;
    }

    if (alarm) {
        row.displayStatus = QStringLiteral("ALARM");
        return;
    }
    row.displayStatus = QStringLiteral("OK");
}

} // namespace

QVector<SensorLiveRow> SensorMerger::buildRows(
    qint64 loggerId,
    const Network::PollSnapshot &snapshot,
    const QVector<Data::LoggerSensor> &catalog)
{
    if (snapshot.loggerId != loggerId || !snapshot.success) {
        return {};
    }

    const bool rtuConnected  = snapshot.header.isRtuConnected();
    const bool pollingActive = snapshot.header.isPolling();

    QHash<int, const Data::LoggerSensor *> analogByEdgeId;
    analogByEdgeId.reserve(catalog.size());
    for (const auto &sensor : catalog) {
        if (sensor.loggerId == loggerId && sensor.sensorType == QLatin1String(kAnalog)) {
            analogByEdgeId.insert(sensor.edgeSensorId, &sensor);
        }
    }

    const QString timestamp = snapshot.measuredAt.isValid()
        ? snapshot.measuredAt.toUTC().toString(QStringLiteral("HH:mm:ss"))
        : QString();

    QVector<SensorLiveRow> rows;
    rows.reserve(snapshot.analogs.size()
                 + snapshot.diBits.size()
                 + snapshot.doBits.size());

    QSet<int> seenAnalogIds;

    for (const auto &sample : snapshot.analogs) {
        seenAnalogIds.insert(sample.edgeSensorId);
        const auto it = analogByEdgeId.constFind(sample.edgeSensorId);
        const Data::LoggerSensor *cat = (it != analogByEdgeId.constEnd()) ? *it : nullptr;

        SensorLiveRow row;
        row.edgeSensorId = sample.edgeSensorId;
        row.sensorType   = cat ? cat->sensorType : QStringLiteral("UNKNOWN");
        row.unit         = cat ? cat->unit       : QString();
        row.name         = (cat && !cat->name.isEmpty())
                               ? cat->name
                               : fallbackName(row.sensorType, row.edgeSensorId);
        row.valid        = sample.isValid();
        row.alarm        = sample.isAlarm();
        row.stale        = sample.isStale();
        row.value        = formatAnalog(sample.value);
        row.timestamp    = timestamp;

        const bool active = cat ? cat->active : true;
        const QString attachCode = resolveAttachDiCode(catalog, sample.edgeSensorId,
                                                       snapshot.diBits);
        applyAnalogStatus(row,
                          active,
                          rtuConnected,
                          pollingActive,
                          row.valid,
                          row.stale,
                          row.alarm,
                          static_cast<double>(sample.value),
                          cat ? cat->minThreshold : std::nullopt,
                          cat ? cat->maxThreshold : std::nullopt,
                          attachCode);

        rows.append(row);
    }

    for (const auto &sensor : catalog) {
        if (sensor.loggerId != loggerId) {
            continue;
        }
        if (sensor.sensorType != QLatin1String(kAnalog) || !sensor.active) {
            continue;
        }
        if (seenAnalogIds.contains(sensor.edgeSensorId)) {
            continue;
        }

        SensorLiveRow row;
        row.edgeSensorId = sensor.edgeSensorId;
        row.sensorType   = sensor.sensorType;
        row.unit         = sensor.unit;
        row.name         = sensor.name.isEmpty()
                               ? fallbackName(sensor.sensorType, sensor.edgeSensorId)
                               : sensor.name;
        row.value          = QStringLiteral("—");
        row.displayStatus  = QStringLiteral("WAIT");
        row.timestamp      = QString();
        row.valid          = false;
        row.alarm          = false;
        row.stale          = false;
        rows.append(row);
    }

    auto appendBitRow = [&](const Data::LoggerSensor &cat,
                            const QVector<bool> &bits)
    {
        if (cat.edgeSensorId < 0 || cat.edgeSensorId >= bits.size()) {
            return;
        }
        const bool on = bits[cat.edgeSensorId];

        SensorLiveRow row;
        row.edgeSensorId = cat.edgeSensorId;
        row.sensorType   = cat.sensorType;
        row.unit         = QString();
        row.name         = cat.name.isEmpty()
                               ? fallbackName(cat.sensorType, cat.edgeSensorId)
                               : cat.name;
        row.valid        = true;
        row.alarm        = false;
        row.stale        = false;
        row.value        = on ? QStringLiteral("ON") : QStringLiteral("OFF");
        row.timestamp    = timestamp;
        if (!cat.active) {
            row.displayStatus = QStringLiteral("WAIT");
        } else if (!rtuConnected && pollingActive) {
            row.displayStatus = QStringLiteral("ERR");
        } else {
            row.displayStatus = QStringLiteral("OK");
        }
        rows.append(row);
    };

    for (const auto &sensor : catalog) {
        if (sensor.loggerId != loggerId) {
            continue;
        }
        if (sensor.parentEdgeSensorId.has_value()) {
            continue;
        }
        if (sensor.sensorType == QLatin1String(kDi)) {
            appendBitRow(sensor, snapshot.diBits);
        } else if (sensor.sensorType == QLatin1String(kDo)) {
            appendBitRow(sensor, snapshot.doBits);
        }
    }

    auto typeRank = [](const QString &t) -> int {
        if (t == QLatin1String(kAnalog)) {
            return 0;
        }
        if (t == QLatin1String(kDi)) {
            return 1;
        }
        if (t == QLatin1String(kDo)) {
            return 2;
        }
        return 3;
    };
    std::stable_sort(rows.begin(), rows.end(),
                     [&typeRank](const SensorLiveRow &a, const SensorLiveRow &b) {
                         const int ra = typeRank(a.sensorType);
                         const int rb = typeRank(b.sensorType);
                         if (ra != rb) {
                             return ra < rb;
                         }
                         return a.edgeSensorId < b.edgeSensorId;
                     });

    return rows;
}

QVector<SensorLiveRow> SensorMerger::buildCatalogPlaceholders(
    qint64 loggerId,
    const QVector<Data::LoggerSensor> &catalog)
{
    QVector<SensorLiveRow> rows;
    for (const auto &cat : catalog) {
        if (cat.loggerId != loggerId) {
            continue;
        }
        if (cat.parentEdgeSensorId.has_value()) {
            continue;
        }
        if (cat.sensorType != QLatin1String(kAnalog)
            && cat.sensorType != QLatin1String(kDi)
            && cat.sensorType != QLatin1String(kDo)) {
            continue;
        }

        SensorLiveRow row;
        row.edgeSensorId   = cat.edgeSensorId;
        row.sensorType     = cat.sensorType;
        row.unit           = cat.unit;
        row.name           = cat.name.isEmpty()
                                 ? fallbackName(cat.sensorType, cat.edgeSensorId)
                                 : cat.name;
        row.value          = QStringLiteral("—");
        row.displayStatus  = QStringLiteral("WAIT");
        row.timestamp      = QString();
        row.valid          = false;
        row.alarm          = false;
        row.stale          = false;
        rows.append(row);
    }

    auto typeRank = [](const QString &t) -> int {
        if (t == QLatin1String(kAnalog)) {
            return 0;
        }
        if (t == QLatin1String(kDi)) {
            return 1;
        }
        if (t == QLatin1String(kDo)) {
            return 2;
        }
        return 3;
    };
    std::stable_sort(rows.begin(), rows.end(),
                     [&typeRank](const SensorLiveRow &a, const SensorLiveRow &b) {
                         const int ra = typeRank(a.sensorType);
                         const int rb = typeRank(b.sensorType);
                         if (ra != rb) {
                             return ra < rb;
                         }
                         return a.edgeSensorId < b.edgeSensorId;
                     });
    return rows;
}

} // namespace CentralLogger::Core
