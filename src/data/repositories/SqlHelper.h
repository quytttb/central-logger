#pragma once

#include <QObject>
#include <QString>

namespace CentralLogger::Data {

/// Maps a raw SQLite driver error into a user-facing message.
///
/// Centralises the error-string sniffing that used to live in controllers so
/// CRUD code stays thin. Currently handles the `logger_info.station_code`
/// UNIQUE collision; falls back to the raw text for anything else.
inline QString humanizeSqlError(const QString &raw, const QString &stationCode) {
  if (raw.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive) &&
      raw.contains(QStringLiteral("station_code"))) {
    return QObject::tr("Station code \"%1\" đã tồn tại").arg(stationCode);
  }
  return raw;
}

} // namespace CentralLogger::Data
