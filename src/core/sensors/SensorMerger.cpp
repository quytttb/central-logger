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

using CentralLogger::Defaults::kDecimalsMax;
using CentralLogger::Defaults::kDecimalsMin;
using CentralLogger::Sensor::kTypeAnalog;
using CentralLogger::Sensor::kTypeDi;
using CentralLogger::Sensor::kTypeDo;
using CentralLogger::Sensor::kTypeUnknown;

QString formatAnalog(float value, int decimals) {
  // Per-sensor precision (synced from edge `decimals`, default 4). Keeps the
  // Detail live table and the persisted History value in agreement for the
  // same reading and sensor.
  return QString::number(static_cast<double>(value), 'f',
                         std::clamp(decimals, kDecimalsMin, kDecimalsMax));
}

QString fallbackName(const QString &sensorType, int edgeSensorId) {
  if (!sensorType.isEmpty() && sensorType != kTypeUnknown) {
    return QString(QLatin1String(CentralLogger::Sensor::kFallbackTypeFmt))
        .arg(sensorType).arg(edgeSensorId);
  }
  return QString(QLatin1String(CentralLogger::Sensor::kFallbackNameFmt)).arg(edgeSensorId);
}

struct ThresholdBreach {
  bool min = false;
  bool max = false;

  [[nodiscard]] bool any() const { return min || max; }
};

ThresholdBreach
checkThresholdBreach(double value, const std::optional<double> &minThreshold,
                     const std::optional<double> &maxThreshold) {
  ThresholdBreach breach;
  if (minThreshold.has_value() && value <= *minThreshold) {
    breach.min = true;
  }
  if (maxThreshold.has_value() && value >= *maxThreshold) {
    breach.max = true;
  }
  return breach;
}

bool isOutOfRange(double value, const std::optional<double> &minThreshold,
                  const std::optional<double> &maxThreshold) {
  return checkThresholdBreach(value, minThreshold, maxThreshold).any();
}

QString computeAlarmType(double value, bool alarmBit,
                         const std::optional<double> &minThreshold,
                         const std::optional<double> &maxThreshold) {
  const ThresholdBreach breach =
      checkThresholdBreach(value, minThreshold, maxThreshold);
  if (breach.min && breach.max) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMinMax);
  }
  if (breach.min) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMin);
  }
  if (breach.max) {
    return QLatin1String(CentralLogger::Sensor::kAlarmMax);
  }
  if (alarmBit) {
    return QLatin1String(CentralLogger::Sensor::kAlarmDevice);
  }
  return {};
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

QString applyAnalogStatus(SensorLiveRow &row, bool active, bool rtuConnected,
                          bool pollingActive, const QString &statusCode,
                          double value,
                          const std::optional<double> &minThreshold,
                          const std::optional<double> &maxThreshold,
                          const QStringList &attachDiCodes,
                          const QStringList &attachDiLabels) {
  row.attachDiTypeCodes = attachDiCodes;
  row.attachDiTypeLabels = attachDiLabels;
  row.alarmType.clear();

  // Không có dữ liệu từ thiết bị → không hiển thị chip (chuỗi rỗng)
  if (!active) {
    row.displayStatus = QString();
    return row.displayStatus;
  }
  // Mất kết nối adapter/Central → không hiển thị chip
  if (!rtuConnected && pollingActive) {
    row.displayStatus = QString();
    return row.displayStatus;
  }
  // Không có mã TT10 từ thiết bị → không hiển thị chip
  if (statusCode.isEmpty()) {
    row.displayStatus = QString();
    return row.displayStatus;
  }

  // Mã TT10 02 = báo lỗi thiết bị → hiển thị "02", vẫn kiểm tra ngưỡng để xác định loại
  const bool deviceError = (statusCode == QLatin1String(CentralLogger::Sensor::kStatusErr));
  if (deviceError) {
    row.alarmType = QLatin1String(CentralLogger::Sensor::kAlarmDevice);
  }
  // 01 = hiệu chuẩn, 03 = maintenance nội bộ → hiển thị mã, không coi là alarm
  if (statusCode == QLatin1String(CentralLogger::Sensor::kStatusCalib) ||
      statusCode == QLatin1String(CentralLogger::Sensor::kStatusMaint)) {
    row.displayStatus = statusCode;
    return row.displayStatus;
  }
  // 00 = đang đo → hiển thị "00"
  row.displayStatus = statusCode;

  // Kiểm tra ngưỡng cảnh báo — alarmType ghi nhận loại vi phạm, displayStatus giữ mã TT10
  const ThresholdBreach breach = checkThresholdBreach(value, minThreshold, maxThreshold);
  if (breach.any()) {
    if (breach.min && breach.max) {
      row.alarmType = QLatin1String(CentralLogger::Sensor::kAlarmMinMax);
    } else if (breach.min) {
      row.alarmType = QLatin1String(CentralLogger::Sensor::kAlarmMin);
    } else {
      row.alarmType = QLatin1String(CentralLogger::Sensor::kAlarmMax);
    }
  }

  return row.displayStatus;
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

  const QString timestamp =
      snapshot.measuredAt.isValid()
          ? snapshot.measuredAt.toUTC().toString(QLatin1String(CentralLogger::Format::kTimeHhMmSs))
          : QString();

  QVector<SensorLiveRow> rows;
  rows.reserve(snapshot.analogs.size() + snapshot.diBits.size() +
               snapshot.doBits.size());

  QSet<int> seenAnalogIds;

  for (const auto &sample : snapshot.analogs) {
    seenAnalogIds.insert(sample.edgeSensorId);
    const auto it = analogByEdgeId.constFind(sample.edgeSensorId);
    const Data::LoggerSensor *cat =
        (it != analogByEdgeId.constEnd()) ? *it : nullptr;

    SensorLiveRow row;
    row.edgeSensorId = sample.edgeSensorId;
    row.sensorType = cat ? cat->sensorType : QLatin1String(CentralLogger::Sensor::kTypeUnknown);
    row.unit = cat ? cat->unit : QString();
    row.name = (cat && !cat->name.isEmpty())
                   ? cat->name
                   : fallbackName(row.sensorType, row.edgeSensorId);
    // Status code comes from the edge (TT10) — no separate valid/stale/alarm
    // flags. The placeholder protects the summary table from showing stale
    // transport values as current measurements.
    row.value = (sample.statusCode.isEmpty()
                 || sample.statusCode == QLatin1String("STALE"))
                    ? QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder)
                    : formatAnalog(sample.value, cat ? cat->decimals
                                                     : CentralLogger::Defaults::kDecimalsDefault);
    row.timestamp = timestamp;
    row.statusCode = sample.statusCode;

    const bool active = cat ? cat->active : true;
    const QStringList attachCodes =
        resolveAttachDiCodes(catalog, sample.edgeSensorId, snapshot.diBits);
    applyAnalogStatus(
        row, active, rtuConnected, pollingActive, sample.statusCode,
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
                   ? fallbackName(sensor.sensorType, sensor.edgeSensorId)
                   : sensor.name;
    row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    row.displayStatus = QString();
    row.timestamp = QString();
    row.statusCode = QString();
    rows.append(row);
  }

  auto appendBitRow = [&](const Data::LoggerSensor &cat,
                          const QVector<bool> &bits) {
    if (cat.edgeSensorId < 0 || cat.edgeSensorId >= bits.size()) {
      return;
    }
    const bool on = bits[cat.edgeSensorId];

    SensorLiveRow row;
    row.edgeSensorId = cat.edgeSensorId;
    row.sensorType = cat.sensorType;
    row.unit = QString();
    row.name = cat.name.isEmpty()
                   ? fallbackName(cat.sensorType, cat.edgeSensorId)
                   : cat.name;
    row.statusCode = QLatin1String("00");
    row.value = on ? QLatin1String(CentralLogger::Sensor::kBitOn)
                   : QLatin1String(CentralLogger::Sensor::kBitOff);
    row.timestamp = timestamp;
    if (!cat.active) {
      row.displayStatus = QString();
    } else if (!rtuConnected && pollingActive) {
      row.displayStatus = QString();
    } else {
      row.displayStatus = QLatin1String(CentralLogger::Sensor::kStatusOk);
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

  auto typeRank = [](const QString &t) -> int {
    if (t == kTypeAnalog) {
      return 0;
    }
    if (t == kTypeDi) {
      return 1;
    }
    if (t == kTypeDo) {
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
                   ? fallbackName(cat.sensorType, cat.edgeSensorId)
                   : cat.name;
    row.value = QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    row.displayStatus = QString();
    row.timestamp = QString();
    row.statusCode = QString();
    rows.append(row);
  }

  auto typeRank = [](const QString &t) -> int {
    if (t == kTypeAnalog) {
      return 0;
    }
    if (t == kTypeDi) {
      return 1;
    }
    if (t == kTypeDo) {
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
