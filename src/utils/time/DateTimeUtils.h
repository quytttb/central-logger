#pragma once

#include <QDateTime>
#include <QString>
#include <QVariant>

namespace CentralLogger::Utils {

/// Parses an ISO 8601 UTC timestamp (with or without milliseconds).
/// Handles the Qt edge case where fromString may return LocalTime spec when
/// the 'Z' suffix is missing — forces UTC in that case.
QDateTime parseUtc(const QString &iso);

/// Serialises @p dt to ISO 8601 with milliseconds, forced to UTC.
QString isoUtc(const QDateTime &dt);

/// Like isoUtc() but returns a null QString QVariant when @p dt is invalid.
/// Use when binding to a nullable SQL column.
QVariant isoUtcOrNull(const QDateTime &dt);

} // namespace CentralLogger::Utils
