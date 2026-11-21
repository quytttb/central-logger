#include "core/sensors/SensorMerger.h"

#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/sensors/AttachDiTypeHelper.h"

#include <QHash>
#include <QSet>
#include <algorithm>

namespace CentralLogger::Core {

using CentralLogger::Utils::AttachDiTypeHelper;

namespace {

using CentralLogger::Sensor::fallbackSensorName;
using CentralLogger::Sensor::formatAnalogValue;
using CentralLogger::Sensor::kTypeAnalog;
using CentralLogger::Sensor::kTypeDi;
using CentralLogger::Sensor::kTypeDo;

void sortLiveRows(QVector<SensorLiveRow> &rows) {
  auto typeRank = [](const QString &t) -> int {
    if (t == kTypeAnalog) return 0;
    if (t == kTypeDi)     return 1;
    if (t == kTypeDo)     return 2;
    return 3;
  };
  std::stable_sort(rows.begin(), rows.end(),
                   [&typeRank](const SensorLiveRow &a, const SensorLiveRow &b) {
                     const int ra = typeRank(a.sensorType);
                     const int rb = typeRank(b.sensorType);
                     return ra != rb ? ra < rb : a.edgeSensorId < b.edgeSensorId;
                   });
}

/// Ngưỡng mirror chỉ dùng để giải thích hướng vi phạm khi bit alarm từ edge đã bật.
QString explainEdgeAlarm(double value,
                         const std::optional<double> &minThreshold,
                         const std::optional<double> &maxThreshold) {
  const bool minBreached = minThreshold.has_value() && value <= *minThreshold;
  const bool maxBreached = maxThreshold.has_value() && value >= *maxThreshold;
  if (minBreached && maxBreached) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMinMax);
  }
  if (minBreached) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMin);
  }
  if (maxBreached) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMax);
  }
  return QLatin1String(CentralLogger::Sensor::kAlarmDevice);
}

bool diLinksToAnalog(const Data::LoggerSensor &child, int parentEdgeSensorId) {
  if (!child.allParentIds.isEmpty()) {
    return child.allParentIds.contains(parentEdgeSensorId);
  }
  return child.parentEdgeSensorId.has_value() &&
         *child.parentEdgeSensorId == parentEdgeSensorId;
}

QString catalogNameForAttachCode(const QVector<Data::LoggerSensor> &catalog,
                                 int parentEdgeSensorId, const QString &code) {
  for (const auto &child : catalog) {
    if (child.sensorType != kTypeDi || !child.active) {
      continue;
    }
    if (!diLinksToAnalog(child, parentEdgeSensorId)) {
      continue;
    }
    const QString childCode = AttachDiTypeHelper::normalizeCode(
        child.diType.isEmpty() ? QLatin1String(CentralLogger::Sensor::kAttachCodeMonitoring)
                               : child.diType);
    if (childCode == code) {
      return child.name;
    }
  }
  return {};
}

QStringList resolveAttachDiLabels(const QVector<Data::LoggerSensor> &catalog,
                                  int parentEdgeSensorId,
                                  const QStringList &codes) {
  QStringList labels;
  labels.reserve(codes.size());
  for (const QString &code : codes) {
    labels.append(AttachDiTypeHelper::displayLabel(
        code, catalogNameForAttachCode(catalog, parentEdgeSensorId, code)));
  }
  return labels;
}

QStringList resolveAttachDiCodes(const QVector<Data::LoggerSensor> &catalog,
                                 int parentEdgeSensorId,
                                 const QVector<bool> &diBits) {
  QSet<QString> seen;
  QStringList codes;

  for (const auto &child : catalog) {
    if (child.sensorType != kTypeDi) {
      continue;
    }
    if (!child.active || !diLinksToAnalog(child, parentEdgeSensorId)) {
      continue;
    }
    if (child.edgeSensorId < 0 || child.edgeSensorId >= diBits.size()) {
      continue;
    }
    if (!diBits.at(child.edgeSensorId)) {
      continue;
    }

    const QString code = AttachDiTypeHelper::normalizeCode(
        child.diType.isEmpty() ? QLatin1String(CentralLogger::Sensor::kAttachCodeMonitoring)
                               : child.diType);
    if (!AttachDiTypeHelper::isAttachActiveCode(code) || seen.contains(code)) {
      continue;
    }
    seen.insert(code);
    codes.append(code);
  }

  std::stable_sort(codes.begin(), codes.end(),
                   [](const QString &a, const QString &b) {
                     const int ra = AttachDiTypeHelper::sortRank(a);
                     const int rb = AttachDiTypeHelper::sortRank(b);
                     if (ra != rb) {
                       return ra < rb;
                     }
                     return a < b;
                   });
  return codes;
}

void applyAnalogStatus(SensorLiveRow &row, bool active, const QString &statusCode,
                       bool hasValidValue, bool edgeAlarmBit, double value,
                       const std::optional<double> &minThreshold,
                       const std::optional<double> &maxThreshold,
                       const QStringList &attachDiCodes,
                       const QStringList &attachDiLabels) {
  row.attachDiTypeCodes = attachDiCodes;
  row.attachDiTypeLabels = attachDiLabels;
  row.alarmType.clear();

  if (!active) {
    row.displayStatus = QString();
    return;
  }

  // TT10 status độc lập hoàn toàn với cờ chất lượng / cảnh báo ngưỡng.
  if (statusCode.isEmpty() || statusCode == QLatin1String("STALE")) {
    row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusUnknown);
  } else {
    row.displayStatus = statusCode;
  }

  // Tín hiệu cảnh báo vượt ngưỡng chỉ lấy từ bit alarm của thiết bị (edge).
  // Ngưỡng mirror tại Central chỉ giải thích hướng (min/max) chứ không tự phát sinh alarm.
  if (hasValidValue && edgeAlarmBit) {
    row.alarmType = explainEdgeAlarm(value, minThreshold, maxThreshold);
  }
}

} // namespace

QVector<SensorLiveRow>
SensorMerger::buildRows(qint64 loggerId, const Network::PollSnapshot &snapshot,
                        const QVector<Data::LoggerSensor> &catalog) {
  if (snapshot.loggerId != loggerId || !snapshot.success) {
    return {};
  }

  const bool rtuConnected = snapshot.header.isRtuConnected();
  const bool pollingActive = snapshot.header.isPolling();

  QHash<int, const Data::LoggerSensor *> analogByEdgeId;
  analogByEdgeId.reserve(catalog.size());
  for (const auto &sensor : catalog) {
    if (sensor.loggerId == loggerId &&
        sensor.sensorType == kTypeAnalog) {
      analogByEdgeId.insert(sensor.edgeSensorId, &sensor);
    }
  }

  QVector<SensorLiveRow> rows;
  rows.reserve(snapshot.analogs.size() + snapshot.diBits.size() +
               snapshot.doBits.size());

  QSet<int> seenAnalogIds;

  for (const auto &sample : snapshot.analogs) {
    seenAnalogIds.insert(sample.edgeSensorId);
    const auto it = analogByEdgeId.constFind(sample.edgeSensorId);
    const Data::LoggerSensor *cat =
        (it != analogByEdgeId.constEnd()) ? *it : nullptr;

    const bool hasValidValue = sample.isValid() && !sample.isStale();
    SensorLiveRow row;
    row.edgeSensorId = sample.edgeSensorId;
    row.sensorType = cat ? cat->sensorType : QLatin1String(CentralLogger::Sensor::kTypeUnknown);
    row.unit = cat ? cat->unit : QString();
    row.name = (cat && !cat->name.isEmpty())
                   ? cat->name
                   : fallbackSensorName(row.edgeSensorId, row.sensorType);
    row.valid = hasValidValue;
    row.value = !hasValidValue
                    ? QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder)
                    : formatAnalogValue(sample.value, cat ? cat->decimals
                                                          : CentralLogger::Defaults::kDecimalsDefault);
    row.status03Auto =
        snapshot.header.mapVersion == CentralLogger::Version::kModbusMapVersion2
        && snapshot.header.status03Label == CentralLogger::Version::kModbusStatus03Auto;

    const bool active = cat ? cat->active : true;
    const QStringList attachCodes =
        resolveAttachDiCodes(catalog, sample.edgeSensorId, snapshot.diBits);
    applyAnalogStatus(
        row, active, sample.statusCode, hasValidValue, sample.qualityAlarm,
        static_cast<double>(sample.value),
        cat ? cat->minThreshold : std::nullopt,
        cat ? cat->maxThreshold : std::nullopt, attachCodes,
        resolveAttachDiLabels(catalog, sample.edgeSensorId, attachCodes));

    rows.append(row);
  }

  for (const auto &sensor : catalog) {
    if (sensor.loggerId != loggerId) {
      continue;
    }
    if (sensor.sensorType != kTypeAnalog || !sensor.active) {
      continue;
    }
    if (seenAnalogIds.contains(sensor.edgeSensorId)) {
      continue;
    }

    SensorLiveRow row;
    row.edgeSensorId = sensor.edgeSensorId;
    row.sensorType = sensor.sensorType;
    row.unit = sensor.unit;
    row.name = sensor.name.isEmpty()
                   ? fallbackSensorName(sensor.edgeSensorId, sensor.sensorType)
                   : sensor.name;
    row.valid = false;
    row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusUnknown);
    rows.append(row);
  }

  auto appendBitRow = [&](const Data::LoggerSensor &cat,
                          const QVector<bool> &bits) {
    if (cat.edgeSensorId < 0 || cat.edgeSensorId >= bits.size()) {
      return;
    }
    const bool on = bits[cat.edgeSensorId];
    const bool bitValid = cat.active && !(!rtuConnected && pollingActive);

    SensorLiveRow row;
    row.edgeSensorId = cat.edgeSensorId;
    row.sensorType = cat.sensorType;
    row.unit = QString();
    row.name = cat.name.isEmpty()
                   ? fallbackSensorName(cat.edgeSensorId, cat.sensorType)
                   : cat.name;
    row.valid = bitValid;
    if (bitValid) {
      row.value = on ? QLatin1String(CentralLogger::Sensor::kBitOn)
                     : QLatin1String(CentralLogger::Sensor::kBitOff);
      row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusOk);
    } else {
      row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
      row.displayStatus = QString();
    }
    rows.append(row);
  };

  QSet<int> seenDiBitIds;
  QSet<int> seenDoBitIds;
  for (const auto &sensor : catalog) {
    if (sensor.loggerId != loggerId) {
      continue;
    }
    if (sensor.sensorType == kTypeDi) {
      if (seenDiBitIds.contains(sensor.edgeSensorId)) {
        continue;
      }
      seenDiBitIds.insert(sensor.edgeSensorId);
      appendBitRow(sensor, snapshot.diBits);
    } else if (sensor.sensorType == kTypeDo) {
      if (seenDoBitIds.contains(sensor.edgeSensorId)) {
        continue;
      }
      seenDoBitIds.insert(sensor.edgeSensorId);
      appendBitRow(sensor, snapshot.doBits);
    }
  }

  sortLiveRows(rows);
  return rows;
}

QVector<SensorLiveRow> SensorMerger::buildCatalogPlaceholders(
    qint64 loggerId, const QVector<Data::LoggerSensor> &catalog) {
  QVector<SensorLiveRow> rows;
  QSet<int> seenDiIds;
  QSet<int> seenDoIds;
  for (const auto &cat : catalog) {
    if (cat.loggerId != loggerId) {
      continue;
    }
    if (cat.sensorType != kTypeAnalog &&
        cat.sensorType != kTypeDi &&
        cat.sensorType != kTypeDo) {
      continue;
    }
    if (cat.sensorType == kTypeDi) {
      if (seenDiIds.contains(cat.edgeSensorId)) {
        continue;
      }
      seenDiIds.insert(cat.edgeSensorId);
    } else if (cat.sensorType == kTypeDo) {
      if (seenDoIds.contains(cat.edgeSensorId)) {
        continue;
      }
      seenDoIds.insert(cat.edgeSensorId);
    }

    SensorLiveRow row;
    row.edgeSensorId = cat.edgeSensorId;
    row.sensorType = cat.sensorType;
    row.unit = cat.unit;
    row.name = cat.name.isEmpty()
                   ? fallbackSensorName(cat.edgeSensorId, cat.sensorType)
                   : cat.name;
    row.valid = false;
    row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    row.displayStatus = cat.active
        ? QLatin1String(CentralLogger::Sensor::kStatusUnknown) : QString();
    rows.append(row);
  }

  sortLiveRows(rows);
  return rows;
}

} // namespace CentralLogger::Core
