#pragma once

#include <QString>
#include <QStringList>

#include <initializer_list>

namespace CentralLogger::Core {

/// Returns true when @p needle is empty or occurs case-insensitively in any field.
inline bool matchesAnyLiveField(const QString &needle,
                                std::initializer_list<QString> fields)
{
    if (needle.isEmpty()) {
        return true;
    }
    for (const QString &field : fields) {
        if (field.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

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
    bool status03Auto = false; // OSG calls its custom 03 state "Auto".
    /// Active attach-DI type codes for this analog (`di_type`, sorted 02→03→01→custom).
    QStringList attachDiTypeCodes;
    /// Chip labels parallel to attachDiTypeCodes (catalog name for custom di_type codes).
    QStringList attachDiTypeLabels;
    /// min|max|min+max|device when edge alarm bit is active; empty otherwise.
    QString alarmType;
    bool    valid = false; // true when measurement/bit is valid and fresh
};

} // namespace CentralLogger::Core
