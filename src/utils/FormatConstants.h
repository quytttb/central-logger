#pragma once

// Centralized date/time format strings and reusable error message templates.
// Single source of truth so the Dashboard chart, History table, and Settings
// format timestamps identically.
//
// Strings are declared as `inline constexpr char[]` so they can be passed
// directly to `QStringLiteral` (which requires literal arguments at the call
// site).

namespace CentralLogger::Format {

// --- Date / time formats (passed to QDateTime::toString) --------------------
inline constexpr char kDateTimeDdMmYyyyHms[] = "dd/MM/yyyy HH:mm:ss";
inline constexpr char kDateDdMmYyyy[]       = "dd/MM/yyyy";
inline constexpr char kDateYyyyMmDdHms[]    = "yyyy-MM-dd HH:mm:ss";
inline constexpr char kTimeHhMmSs[]         = "HH:mm:ss";
inline constexpr char kTimeHhMm[]           = "HH:mm";

// --- User-facing error message templates -------------------------------------
// All `REST service not available` / token / token-empty / device-port error
// strings come from these constants. Used by both the QML form, the REST
// parser, and the dashboard controller.

inline constexpr char kErrDatabaseNotOpen[] = "Database not open";
inline constexpr char kErrRestUnavailable[] = "REST service not available.";

inline constexpr char kErrRestTokenEmpty[]       = "Device REST token empty \u2014 Scan QR on logger";
inline constexpr char kErrRestTokenMismatch[]    = "Token mismatch \u2014 Scan QR again on device";
inline constexpr char kErrLoggerUnreachable[]    = "Could not reach the logger. Check host, API port, and network.";
inline constexpr char kErrLoggerUnreachableFmt[] = "Could not reach the logger: %1";

inline constexpr char kErrRestUnauthorized[]      = "REST unauthorized (401)";
inline constexpr char kErrRevisionConflict[]      = "Configuration changed on device. Connect again, then save.";
inline constexpr char kErrMissingFields[]         = "Device rejected config request (missing fields). Update Central Logger.";
inline constexpr char kErrEdgeRejected422[]       = "Edge rejected payload (422). Check forbidden fields.";
inline constexpr char kErrApiNotAvailable[]       = "Logger API not available. Update data-logger firmware.";

inline constexpr char kErrNoLatestReport[]        = "No latest report on device. Generate a report on the data-logger first.";
inline constexpr char kErrReportEndpointMissing[] = "Report endpoint not found. Update data-logger firmware.";
inline constexpr char kErrReportTooLarge[]        = "Report too large (%1 MB, limit 50 MB)";
inline constexpr char kErrReportDataTooLarge[]    = "Report data exceeds 50 MB limit";

inline constexpr char kErrHttpFmt[]     = "HTTP %1";
inline constexpr char kErrHttpBodyFmt[] = "HTTP %1: %2";

} // namespace CentralLogger::Format
