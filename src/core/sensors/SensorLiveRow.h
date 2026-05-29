#pragma once

#include <QString>
#include <QStringList>

namespace CentralLogger::Core {

/// One merged row built from `logger_sensor` catalog + `PollSnapshot`.
/// See docs/thiet_ke_db.md §3.3.
struct SensorLiveRow
{
    int     edgeSensorId = 0;
    QString name;
    QString sensorType;   // ANALOG|DI|DO|UNKNOWN
    QString unit;
    QString value;        // formatted: "25.50" (analog) or "ON"/"OFF" (DI/DO)
    /// Operational chip: WAIT|OK|ALARM|ERR|STALE (not attach-DI labels).
    QString displayStatus;
    /// Active attach-DI type codes for this analog (`di_type`, sorted 02→03→01→custom).
    QStringList attachDiTypeCodes;
    /// Chip labels parallel to attachDiTypeCodes (catalog name for custom di_type codes).
    QStringList attachDiTypeLabels;
    /// min|max|min+max|device when alarm; empty otherwise.
    QString alarmType;
    QString timestamp;    // HH:mm:ss UTC from snapshot.measuredAt
    bool    valid = true;
    bool    alarm = false;
    bool    stale = false;
};

} // namespace CentralLogger::Core
