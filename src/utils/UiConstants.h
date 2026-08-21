#pragma once

// Centralized UI strings shared by C++ models and QML delegates:
// - QML role names (must match `import CentralLogger.Core`)
// - chart data keys for QVariantMap payloads (Dashboard + LoggerDetail charts)
// - theme values
//
// Strings are declared as `inline constexpr char[]` so they can be passed
// directly to `QStringLiteral`/`QLatin1StringView` (which require literal
// arguments at the call site).

namespace CentralLogger::Ui {

// --- QML role names (must stay in lockstep with roleNames() in *.cpp) -------
inline constexpr char kRoleDisplay[]           = "display";
inline constexpr char kRoleId[]                = "id";
inline constexpr char kRoleName[]              = "name";
inline constexpr char kRoleLoggerId[]          = "loggerId";
inline constexpr char kRoleStationCode[]       = "stationCode";
inline constexpr char kRoleHost[]              = "host";
inline constexpr char kRoleModbusPort[]        = "modbusPort";
inline constexpr char kRoleModbusUnitId[]      = "modbusUnitId";
inline constexpr char kRoleApiPort[]           = "apiPort";
inline constexpr char kRoleApiToken[]          = "apiToken";
inline constexpr char kRoleStatus[]            = "status";
inline constexpr char kRoleSensorCount[]       = "sensorCount";
inline constexpr char kRoleOnline[]            = "online";
inline constexpr char kRolePolling[]           = "polling";
inline constexpr char kRoleAnyAlarm[]          = "anyAlarm";
inline constexpr char kRoleRtuConnected[]      = "rtuConnected";
inline constexpr char kRoleLoggerName[]        = "loggerName";
inline constexpr char kRoleEventType[]         = "eventType";
inline constexpr char kRoleMessage[]           = "message";
inline constexpr char kRoleLevel[]             = "level";
inline constexpr char kRoleDisplayLevel[]      = "displayLevel";
inline constexpr char kRoleCreatedAt[]         = "createdAt";
inline constexpr char kRoleTime[]              = "time";
inline constexpr char kRoleLogger[]            = "logger";
inline constexpr char kRoleSensor[]            = "sensor";
inline constexpr char kRoleUnit[]              = "unit";
inline constexpr char kRoleValue[]             = "value";
inline constexpr char kRoleValid[]             = "valid";
inline constexpr char kRoleAlarm[]             = "alarm";
inline constexpr char kRoleStale[]             = "stale";
inline constexpr char kRoleSensorId[]          = "sensorId";
inline constexpr char kRoleSensorType[]        = "sensorType";
inline constexpr char kRoleDisplayStatus[]     = "displayStatus";
inline constexpr char kRoleAttachDiCodes[]     = "attachDiTypeCodes";
inline constexpr char kRoleAttachDiLabels[]    = "attachDiTypeLabels";
inline constexpr char kRoleAlarmType[]         = "alarmType";
inline constexpr char kRoleTimestamp[]         = "timestamp";

// --- Chart payload keys (QVariantMap keys consumed by QML/ChartGraphsView) -
inline constexpr char kChartLabel[]            = "label";
inline constexpr char kChartBucketMs[]         = "bucketMs";
inline constexpr char kChartCount[]            = "count";
inline constexpr char kChartPoints[]           = "points";
inline constexpr char kChartX[]                = "x";
inline constexpr char kChartY[]                = "y";
inline constexpr char kChartTime[]             = "time";
inline constexpr char kChartEdgeSensorId[]     = "edgeSensorId";
inline constexpr char kChartDecimals[]         = "decimals";
inline constexpr char kChartXMin[]             = "xMin";
inline constexpr char kChartXMax[]             = "xMax";
inline constexpr char kChartYMin[]             = "yMin";
inline constexpr char kChartYMax[]             = "yMax";
inline constexpr char kChartPosition[]         = "position";
inline constexpr char kChartCaptionText[]      = "captionText";
inline constexpr char kChartValueRows[]        = "valueRows";
inline constexpr char kChartText[]             = "text";
inline constexpr char kChartSeriesIndex[]      = "seriesIndex";

// --- Form / settings QML keys (QVariantMap from LoggerFormController) -------
inline constexpr char kFormKeyCentralPollIntervalS[] = "centralPollIntervalS";
inline constexpr char kFormKeyTimeoutS[]             = "timeoutS";
inline constexpr char kFormKeyStatus[]               = "status";

// --- Theme values ------------------------------------------------------------
inline constexpr char kThemeLight[] = "light";
inline constexpr char kThemeDark[]  = "dark";

// --- Fallback logger/sensor display strings ----------------------------------
inline constexpr char kDefaultTimezone[] = "Asia/Ho_Chi_Minh";

} // namespace CentralLogger::Ui
