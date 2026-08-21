#pragma once

// Centralized SQL table names and bind-parameter names.
// Single source of truth for the schema living in src/data/db/schema/001_initial.sql.
// Keep in lockstep with docs/thiet_ke_db.md.
//
// Strings are declared as `inline constexpr char[]` so they can be passed
// directly to `QStringLiteral` (which requires literal arguments).

namespace CentralLogger::Data::Db {

// --- Table names ------------------------------------------------------------
inline constexpr char kTableLoggerInfo[]     = "logger_info";
inline constexpr char kTableLoggerSensor[]   = "logger_sensor";
inline constexpr char kTableSensorReading[]  = "sensor_reading";
inline constexpr char kTableSystemEvent[]    = "system_event";
inline constexpr char kTableAppSettings[]    = "app_settings";

// --- logger_info column order (positional read in LoggerRepository) ---------
// The string is the SELECT list reused across findById / findAll / etc. so
// rowToModel() can read by index. Update Col* enum in the same .cpp file
// when this changes.
inline constexpr char kColumnsLoggerInfo[] =
    "id, station_code, name, host, modbus_port, modbus_unit_id, "
    "central_poll_interval_s, timeout_s, enabled, api_port, api_token, "
    "last_revision, status, last_seen, note, created_at";

// --- Bind parameter names (for prepared queries) -----------------------------
inline constexpr char kBindId[]                 = ":id";
inline constexpr char kBindCutoff[]             = ":cutoff";
inline constexpr char kBindLimit[]              = ":limit";
inline constexpr char kBindLim[]                = ":lim";
inline constexpr char kBindLoggerId[]           = ":logger_id";
inline constexpr char kBindLid[]                = ":lid";
inline constexpr char kBindSensorId[]           = ":sensor_id";
inline constexpr char kBindSid[]                = ":sid";
inline constexpr char kBindEid[]                = ":eid";
inline constexpr char kBindStype[]              = ":stype";
inline constexpr char kBindMax[]                = ":max";
inline constexpr char kBindBucket[]             = ":bucket";
inline constexpr char kBindFrom[]               = ":from";
inline constexpr char kBindTo[]                 = ":to";
inline constexpr char kBindStatus[]             = ":status";
inline constexpr char kBindTheme[]              = ":theme";
inline constexpr char kBindTz[]                 = ":tz";
inline constexpr char kBindRetention[]          = ":retention";
inline constexpr char kBindHistoryFlush[]       = ":history_flush";
inline constexpr char kBindStationCode[]        = ":station_code";
inline constexpr char kBindName[]               = ":name";
inline constexpr char kBindHost[]               = ":host";
inline constexpr char kBindModbusPort[]         = ":modbus_port";
inline constexpr char kBindModbusUnitId[]       = ":modbus_unit_id";
inline constexpr char kBindCentralPollIntervalS[] = ":central_poll_interval_s";
inline constexpr char kBindTimeoutS[]           = ":timeout_s";
inline constexpr char kBindEnabled[]            = ":enabled";
inline constexpr char kBindApiPort[]            = ":api_port";
inline constexpr char kBindApiToken[]           = ":api_token";
inline constexpr char kBindLastRevision[]       = ":last_revision";
inline constexpr char kBindLastSeen[]           = ":last_seen";
inline constexpr char kBindNote[]               = ":note";
inline constexpr char kBindEventType[]          = ":event_type";
inline constexpr char kBindMessage[]            = ":message";
inline constexpr char kBindLevel[]              = ":level";
inline constexpr char kBindEdgeSensorId[]       = ":edge_sensor_id";
inline constexpr char kBindSensorType[]         = ":sensor_type";
inline constexpr char kBindUnit[]               = ":unit";
inline constexpr char kBindMinThreshold[]       = ":min_threshold";
inline constexpr char kBindMaxThreshold[]       = ":max_threshold";
inline constexpr char kBindDecimals[]           = ":decimals";
inline constexpr char kBindActive[]             = ":active";
inline constexpr char kBindParentEdgeSensorId[] = ":parent_edge_sensor_id";
inline constexpr char kBindDiType[]             = ":di_type";
inline constexpr char kBindAllParentIds[]       = ":all_parent_ids";
inline constexpr char kBindValue[]              = ":value";
inline constexpr char kBindValid[]              = ":valid";
inline constexpr char kBindAlarm[]              = ":alarm";
inline constexpr char kBindStale[]              = ":stale";
inline constexpr char kBindLoggerTimestamp[]    = ":logger_timestamp";
inline constexpr char kBindRecordedAt[]         = ":recorded_at";

// --- SQLite driver + resource ----------------------------------------------
inline constexpr char kSqliteDriver[]    = "QSQLITE";
inline constexpr char kSchemaResource[]  = ":/db/schema/001_initial.sql";

} // namespace CentralLogger::Data::Db
