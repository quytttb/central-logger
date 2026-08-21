#pragma once

// Centralized sensor domain strings:
// - sensor types (ANALOG / DI / DO / UNKNOWN)
// - operational status values (OK / ALARM / WAIT / STALE / ERR / INVALID)
// - alarm type strings (min / max / min+max / device)
// - attach-DI standard codes (00 / 01 / 02 / 03)
// - logger status strings (online / offline)
// - event levels (info / warning / critical / error)
// - event types (Info / Warning / Alarm / Online / Offline)
//
// Shared by SensorMerger (C++), HistoryTableModel (C++), OperationalStatus.qml,
// AttachDiType.qml, AppColors.qml, LoggerListModel, and DashboardController.
//
// Strings are declared as `inline constexpr char[]` so they can be passed
// directly to `QStringLiteral` (which requires literal arguments at the call
// site).

namespace CentralLogger::Sensor {

// --- Sensor types ----------------------------------------------------------
inline constexpr char kTypeAnalog[]  = "ANALOG";
inline constexpr char kTypeDi[]      = "DI";
inline constexpr char kTypeDo[]      = "DO";
inline constexpr char kTypeUnknown[] = "UNKNOWN";

// --- Operational status (chips in SensorMonitoringTable) -------------------
inline constexpr char kStatusOk[]      = "OK";
inline constexpr char kStatusAlarm[]   = "ALARM";
inline constexpr char kStatusWait[]    = "WAIT";
inline constexpr char kStatusErr[]     = "ERR";
inline constexpr char kStatusStale[]   = "STALE";
inline constexpr char kStatusInvalid[] = "INVALID";

// --- Alarm type (SensorLiveRow.alarmType) ----------------------------------
inline constexpr char kAlarmMin[]    = "min";
inline constexpr char kAlarmMax[]    = "max";
inline constexpr char kAlarmMinMax[] = "min+max";
inline constexpr char kAlarmDevice[] = "device";

// --- Attach-DI standard codes (00–03) --------------------------------------
inline constexpr char kAttachCodeMonitoring[]  = "00";
inline constexpr char kAttachCodeCalibrating[] = "01";
inline constexpr char kAttachCodeError[]       = "02";
inline constexpr char kAttachCodeMaintenance[] = "03";

// --- Logger status (logger_info.status) -----------------------------------
inline constexpr char kLoggerOnline[]  = "online";
inline constexpr char kLoggerOffline[] = "offline";

// --- Digital bit row value (SensorLiveRow.value for DI/DO) -----------------
inline constexpr char kBitOn[]  = "ON";
inline constexpr char kBitOff[] = "OFF";

// --- Placeholder when no live value yet -------------------------------------
inline constexpr char kValuePlaceholder[] = "\u2014";

// --- Fallback sensor display name ------------------------------------------
inline constexpr char kFallbackNameFmt[] = "Sensor #%1";
inline constexpr char kFallbackTypeFmt[] = "%1#%2";

// --- Event level (system_event.level) --------------------------------------
inline constexpr char kLevelInfo[]     = "info";
inline constexpr char kLevelWarning[]  = "warning";
inline constexpr char kLevelCritical[] = "critical";
inline constexpr char kLevelError[]    = "error";
inline constexpr char kLevelAlarm[]    = "alarm";
inline constexpr char kLevelOnline[]   = "online";
inline constexpr char kLevelOffline[]  = "offline";

// --- Event type (system_event.event_type) ----------------------------------
inline constexpr char kEventTypeInfo[]    = "Info";
inline constexpr char kEventTypeWarning[] = "Warning";
inline constexpr char kEventTypeAlarm[]   = "Alarm";
inline constexpr char kEventTypeOnline[]  = "Online";
inline constexpr char kEventTypeOffline[] = "Offline";

} // namespace CentralLogger::Sensor
