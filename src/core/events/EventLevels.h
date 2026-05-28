#pragma once

#include <QString>

namespace CentralLogger::Core {

/// Normalized severity for UI coloring: critical | warning | info.
/// Prefers @p eventType (user-visible label); @p level is the DB column.
inline QString displayLevelForEvent(const QString &eventType, const QString &level)
{
    const auto fromToken = [](const QString &token) -> QString {
        if (token == QLatin1String("warning") || token == QLatin1String("offline")) {
            return QStringLiteral("warning");
        }
        if (token == QLatin1String("alarm") || token == QLatin1String("critical")
            || token == QLatin1String("error")) {
            return QStringLiteral("critical");
        }
        if (token == QLatin1String("info") || token == QLatin1String("online")) {
            return QStringLiteral("info");
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
    return QStringLiteral("info");
}

} // namespace CentralLogger::Core
