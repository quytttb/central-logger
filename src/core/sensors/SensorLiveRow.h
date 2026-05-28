#pragma once

#include <QString>

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
    /// Chip text: WAIT|OK|ALARM|ERR|STALE or attach-DI label (Monitoring, Error, …).
    QString displayStatus;
    /// Raw attach-DI code (00–03/custom) when applicable; empty otherwise.
    QString diStatusCode;
    /// min|max|min+max when alarm; empty otherwise.
    QString alarmType;
    QString timestamp;    // HH:mm:ss UTC from snapshot.measuredAt
    bool    valid = true;
    bool    alarm = false;
    bool    stale = false;
    /// True when row.alarm — UI may show secondary ALARM badge on attach-DI chip.
    bool    showAlarmBadge = false;
};

} // namespace CentralLogger::Core
