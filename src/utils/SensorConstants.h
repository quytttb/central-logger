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
// After Thông tư 10/2021 (Bảng 35): 00=đang đo, 01=hiệu chuẩn, 02=báo lỗi.
// 03 = maintenance nội bộ (không có trong Bảng 35, dùng trong data-logger).
// Client chỉ hiển thị mã TT10 trực tiếp ("00"/"01"/"02"/"03"); không còn
// dùng cờ valid/stale/alarm riêng và không hiển thị nhãn OK/ALARM/STALE.
inline constexpr char kStatusOk[]      = "00";   // đang đo
inline constexpr char kStatusCalib[]   = "01";   // hiệu chuẩn
inline constexpr char kStatusErr[]     = "02";   // báo lỗi thiết bị
inline constexpr char kStatusMaint[]   = "03";   // maintenance nội bộ

// Chuỗi rỗng ("") = không có dữ liệu / mất kết nối (không hiển thị chip).
// Các hằng legacy OK/ALARM/STALE/ERR/WAIT đã bị xoá hoàn toàn.

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
