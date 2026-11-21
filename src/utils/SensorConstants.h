#pragma once

#include "utils/AppConstants.h"

#include <QString>

#include <algorithm>
#include <optional>

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
inline constexpr char kStatusUnknown[] = "UNKNOWN"; // v1 cannot carry TT10
inline constexpr char kLegacyStatusStale[] = "STALE"; // skip legacy samples in historical charts

// Chuỗi rỗng ("") = không có dữ liệu / mất kết nối (không hiển thị chip).
// Legacy values are not presented as TT10 status.

/// Map TT10 status code → human-readable status text (shown on chips).
inline QString statusText(const QString &code)
{
    if (code == QLatin1String(kStatusOk)) return QStringLiteral("Measuring");
    if (code == QLatin1String(kStatusCalib)) return QStringLiteral("Calibrating");
    if (code == QLatin1String(kStatusErr)) return QStringLiteral("Error");
    // History v7 stores the code but not the source-specific 03 label.
    if (code == QLatin1String(kStatusMaint)) return QStringLiteral("Custom");
    if (code == QLatin1String(kStatusUnknown)) return QStringLiteral("Unknown");
    return QString();
}

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

/// Format an analog numeric value with clamped decimal precision.
inline QString formatAnalogValue(double value, int decimals)
{
    return QString::number(
        value, 'f',
        std::clamp(decimals, Defaults::kDecimalsMin, Defaults::kDecimalsMax));
}

/// Fallback sensor label when a catalog row has no explicit name.
inline QString fallbackSensorName(int edgeSensorId,
                                  const QString &sensorType = {})
{
    if (!sensorType.isEmpty() && sensorType != QLatin1String(kTypeUnknown)) {
        return QString(QLatin1String(kFallbackTypeFmt))
            .arg(sensorType)
            .arg(edgeSensorId);
    }
    return QString(QLatin1String(kFallbackNameFmt)).arg(edgeSensorId);
}

/// Format a threshold scalar with up to max decimals, trimming trailing zeros.
inline QString formatThresholdNumber(double value)
{
    QString text = QString::number(value, 'f', Defaults::kDecimalsMax);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    if (text == QLatin1String("-0")) {
        text = QStringLiteral("0");
    }
    return text;
}

inline QString formatThresholdNumber(const std::optional<double> &value)
{
    return value.has_value() ? formatThresholdNumber(*value) : QString{};
}

/// Format an optional [min, max] threshold pair as "min – max", "≥ min", or "≤ max".
inline QString formatCompactLimit(const std::optional<double> &minT,
                                    const std::optional<double> &maxT)
{
    if (minT.has_value() && maxT.has_value()) {
        return QStringLiteral("%1 \u2013 %2")
            .arg(formatThresholdNumber(*minT), formatThresholdNumber(*maxT));
    }
    if (minT.has_value()) {
        return QStringLiteral("\u2265 %1").arg(formatThresholdNumber(*minT));
    }
    if (maxT.has_value()) {
        return QStringLiteral("\u2264 %1").arg(formatThresholdNumber(*maxT));
    }
    return {};
}

/// Card label: "[min - max]", "[≥ min]", "[≤ max]", or blank.
inline QString formatBracketLimit(const std::optional<double> &minT,
                                  const std::optional<double> &maxT)
{
    if (minT && maxT) {
        return QStringLiteral("[%1 - %2]")
            .arg(formatThresholdNumber(*minT), formatThresholdNumber(*maxT));
    }
    const auto single = formatCompactLimit(minT, maxT);
    return single.isEmpty() ? QString{} : QStringLiteral("[%1]").arg(single);
}

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
inline constexpr char kEventTypeOnline[]  = "Online";
inline constexpr char kEventTypeOffline[] = "Offline";

} // namespace CentralLogger::Sensor
