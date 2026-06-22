#include "core/sensors/SensorMerger.h"

#include "utils/sensors/AttachDiTypeHelper.h"

#include <QHash>
#include <QSet>
#include <algorithm>

namespace CentralLogger::Core {

using CentralLogger::Utils::AttachDiTypeHelper;

namespace {

constexpr auto kAnalog = "ANALOG";
constexpr auto kDi = "DI";
constexpr auto kDo = "DO";
constexpr auto kUnknown = "UNKNOWN";

QString formatAnalog(float value, int decimals) {
  // Per-sensor precision (synced from edge `decimals`, default 4). Keeps the
  // Detail live table and the persisted History value in agreement for the
  // same reading and sensor.
  return QString::number(static_cast<double>(value), 'f',
                         std::clamp(decimals, 0, 6));
}

QString fallbackName(const QString &sensorType, int edgeSensorId) {
  if (!sensorType.isEmpty() && sensorType != QLatin1String(kUnknown)) {
    return QStringLiteral("%1#%2").arg(sensorType).arg(edgeSensorId);
  }
  return QStringLiteral("Sensor #%1").arg(edgeSensorId);
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
    return QStringLiteral("min+max");
  }
  if (breach.min) {
    return QStringLiteral("min");
  }
  if (breach.max) {
    return QStringLiteral("max");
  }
  if (alarmBit) {
    return QStringLiteral("device");
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
    if (child.sensorType != QLatin1String(kDi) || !child.active) {
      continue;
    }
    if (!diLinksToAnalog(child, parentEdgeSensorId)) {
      continue;
    }
    const QString childCode = AttachDiTypeHelper::normalizeCode(
        child.diType.isEmpty() ? QStringLiteral("00") : child.diType);
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
    if (child.sensorType != QLatin1String(kDi)) {
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
        child.diType.isEmpty() ? QStringLiteral("00") : child.diType);
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

void applyAnalogStatus(SensorLiveRow &row, bool active, bool rtuConnected,
                       bool pollingActive, bool valid, bool stale,
                       bool alarmBit, double value,
                       const std::optional<double> &minThreshold,
                       const std::optional<double> &maxThreshold,
                       const QStringList &attachDiCodes,
                       const QStringList &attachDiLabels) {
  row.attachDiTypeCodes = attachDiCodes;
  row.attachDiTypeLabels = attachDiLabels;
  row.alarmType.clear();

  if (!active) {
    row.displayStatus = QStringLiteral("WAIT");
    return;
  }
  // Edge sets sensor ERR on connection_lost while polling; HR1=0 during TCP
  // warm-up is not the same as RTU disconnected.
  if (!valid || (!rtuConnected && pollingActive)) {
    row.displayStatus = QStringLiteral("ERR");
    return;
  }
  if (stale) {
    row.displayStatus = QStringLiteral("STALE");
    return;
  }

  const bool alarm =
      alarmBit || isOutOfRange(value, minThreshold, maxThreshold);
  if (alarm) {
    row.alarmType =
        computeAlarmType(value, alarmBit, minThreshold, maxThreshold);
    row.displayStatus = QStringLiteral("ALARM");
    return;
  }
  row.displayStatus = QStringLiteral("OK");
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
        sensor.sensorType == QLatin1String(kAnalog)) {
      analogByEdgeId.insert(sensor.edgeSensorId, &sensor);
    }
  }

  const QString timestamp =
      snapshot.measuredAt.isValid()
          ? snapshot.measuredAt.toUTC().toString(QStringLiteral("HH:mm:ss"))
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
    row.sensorType = cat ? cat->sensorType : QStringLiteral("UNKNOWN");
    row.unit = cat ? cat->unit : QString();
    row.name = (cat && !cat->name.isEmpty())
                   ? cat->name
                   : fallbackName(row.sensorType, row.edgeSensorId);
    row.valid = sample.isValid();
    row.alarm = sample.isAlarm();
    row.stale = sample.isStale();
    row.value = formatAnalog(sample.value, cat ? cat->decimals : 4);
    row.timestamp = timestamp;

    const bool active = cat ? cat->active : true;
    const QStringList attachCodes =
        resolveAttachDiCodes(catalog, sample.edgeSensorId, snapshot.diBits);
    applyAnalogStatus(
        row, active, rtuConnected, pollingActive, row.valid, row.stale,
        row.alarm, static_cast<double>(sample.value),
        cat ? cat->minThreshold : std::nullopt,
        cat ? cat->maxThreshold : std::nullopt, attachCodes,
        resolveAttachDiLabels(catalog, sample.edgeSensorId, attachCodes));

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
    row.sensorType = sensor.sensorType;
    row.unit = sensor.unit;
    row.name = sensor.name.isEmpty()
                   ? fallbackName(sensor.sensorType, sensor.edgeSensorId)
                   : sensor.name;
    row.value = QStringLiteral("—");
    row.displayStatus = QStringLiteral("WAIT");
    row.timestamp = QString();
    row.valid = false;
    row.alarm = false;
    row.stale = false;
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
    row.valid = true;
    row.alarm = false;
    row.stale = false;
    row.value = on ? QStringLiteral("ON") : QStringLiteral("OFF");
    row.timestamp = timestamp;
    if (!cat.active) {
      row.displayStatus = QStringLiteral("WAIT");
    } else if (!rtuConnected && pollingActive) {
      row.displayStatus = QStringLiteral("ERR");
    } else {
      row.displayStatus = QStringLiteral("OK");
    }
    rows.append(row);
  };

  QSet<int> seenDiBitIds;
  QSet<int> seenDoBitIds;
  for (const auto &sensor : catalog) {
    if (sensor.loggerId != loggerId) {
      continue;
    }
    if (sensor.sensorType == QLatin1String(kDi)) {
      if (seenDiBitIds.contains(sensor.edgeSensorId)) {
        continue;
      }
      seenDiBitIds.insert(sensor.edgeSensorId);
      appendBitRow(sensor, snapshot.diBits);
    } else if (sensor.sensorType == QLatin1String(kDo)) {
      if (seenDoBitIds.contains(sensor.edgeSensorId)) {
        continue;
      }
      seenDoBitIds.insert(sensor.edgeSensorId);
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
    qint64 loggerId, const QVector<Data::LoggerSensor> &catalog) {
  QVector<SensorLiveRow> rows;
  QSet<int> seenDiIds;
  QSet<int> seenDoIds;
  for (const auto &cat : catalog) {
    if (cat.loggerId != loggerId) {
      continue;
    }
    if (cat.sensorType != QLatin1String(kAnalog) &&
        cat.sensorType != QLatin1String(kDi) &&
        cat.sensorType != QLatin1String(kDo)) {
      continue;
    }
    if (cat.sensorType == QLatin1String(kDi)) {
      if (seenDiIds.contains(cat.edgeSensorId)) {
        continue;
      }
      seenDiIds.insert(cat.edgeSensorId);
    } else if (cat.sensorType == QLatin1String(kDo)) {
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
    row.value = QStringLiteral("—");
    row.displayStatus = QStringLiteral("WAIT");
    row.timestamp = QString();
    row.valid = false;
    row.alarm = false;
    row.stale = false;
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
