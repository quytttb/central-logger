#pragma once

#include "utils/SensorConstants.h"

#include <QLatin1String>
#include <QString>

namespace CentralLogger::Utils {

/// Normalized severity for UI coloring: critical | warning | info.
/// Prefers @p eventType (user-visible label); @p level is the DB column.
inline QString displayLevelForEvent(const QString &eventType,
                                    const QString &level) {
  const auto fromToken = [](const QString &token) -> QString {
    if (token == QLatin1String(CentralLogger::Sensor::kLevelWarning) ||
        token == QLatin1String(CentralLogger::Sensor::kLevelOffline)) {
      return QString::fromUtf8(CentralLogger::Sensor::kLevelWarning);
    }
    if (token == QLatin1String(CentralLogger::Sensor::kLevelAlarm) ||
        token == QLatin1String(CentralLogger::Sensor::kLevelCritical) ||
        token == QLatin1String(CentralLogger::Sensor::kLevelError)) {
      return QString::fromUtf8(CentralLogger::Sensor::kLevelCritical);
    }
    if (token == QLatin1String(CentralLogger::Sensor::kLevelInfo) ||
        token == QLatin1String(CentralLogger::Sensor::kLevelOnline)) {
      return QString::fromUtf8(CentralLogger::Sensor::kLevelInfo);
    }
    return {};
  };

  const QString fromType = fromToken(eventType.trimmed().toLower());
  if (!fromType.isEmpty()) {
    return fromType;
  }
  const QString fromLevel = fromToken(level.trimmed().toLower());
  if (!fromLevel.isEmpty()) {
    return fromLevel;
  }
  return QString::fromUtf8(CentralLogger::Sensor::kLevelInfo);
}

} // namespace CentralLogger::Utils
