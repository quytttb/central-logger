#pragma once

// Centralized default values, intervals, batch sizes, and other operational
// tunables. Every magic number that was previously sprinkled across data
// models, network configs, controllers, and form views lives here.
//
// Header-only; lives in the `utils` base library so it is includable from
// data / core / network / app layers.
//
// Version constants (DB schema, REST API, Modbus map) live in `Version.h`.

#include <QtGlobal>

namespace CentralLogger::Defaults {

// --- Network defaults --------------------------------------------------------
inline constexpr int kDefaultModbusPort   = 5020;
inline constexpr int kDefaultApiPort      = 8080;
inline constexpr int kDefaultModbusUnitId = 1;

// --- Polling / timing defaults ------------------------------------------------
inline constexpr int kDefaultPollIntervalSec = 2;
inline constexpr int kDefaultTimeoutSec      = 2;

// --- Interval clamp bounds (poll / flush / settings) -------------------------
inline constexpr int kMinIntervalSec = 1;
inline constexpr int kMaxIntervalSec = 3600; // 1 hour

// --- Derived millisecond helpers --------------------------------------------
inline constexpr int kMsPerSecond             = 1000;
inline constexpr int kSecondsPerHour          = 3600;
inline constexpr int kDefaultPollIntervalMs   = kDefaultPollIntervalSec * kMsPerSecond;
inline constexpr int kDefaultTimeoutMs        = kDefaultTimeoutSec      * kMsPerSecond;

// --- Modbus connection / poll ------------------------------------------------
inline constexpr int kConnectTimeoutMultiplier = 2;   // abort ConnectingState after timeout * N
inline constexpr int kConnectTimeoutFallbackMs = 4000;// when timeout < 1
inline constexpr int kModbusHeartbeatMs        = 15 * 60 * 1000; // store-on-change force-write cadence
inline constexpr int kMaxAnalogChunk           = 15;  // 15*8 = 120 ≤ 125-reg FC03 limit

// --- REST config service -----------------------------------------------------
inline constexpr int    kRestTransferTimeoutMs   = 10'000;  // GET / POST /config, GET /readings
inline constexpr int    kRestProbeTimeoutMs      = 8'000;   // one-shot probe
inline constexpr int    kRestReportTimeoutMs     = 30'000;  // large artifact download
inline constexpr int    kRestConfigPushTimeoutMs = 15'000;  // client-side wait for /config apply
inline constexpr qint64 kRestReportMaxBytes      = 50LL * 1024 * 1024; // 50 MB cap

// --- SQLite pragmas / housekeeping -------------------------------------------
inline constexpr int    kSqliteBusyTimeoutMs = 5'000;
inline constexpr qint64 kSqliteMmapSize      = 256LL * 1024 * 1024; // 256 MB

// --- Retention / vacuum ------------------------------------------------------
inline constexpr int kPurgeIntervalMs      = kSecondsPerHour * kMsPerSecond; // hourly
inline constexpr int kVacuumChunkPages     = 1000;                            // pages per PRAGMA incremental_vacuum
inline constexpr int kMaxVacuumIterations  = 64;                              // bound per purge cycle
inline constexpr int kDefaultPurgeChunkSize = 50'000;                         // rows per DELETE chunk

// --- History / batch writer --------------------------------------------------
inline constexpr int kHistorySearchLimit     = 5000;
inline constexpr int kHistoryMaxBatchSize    = 20;
inline constexpr int kHistoryMaxQueueSize    = 5000;
inline constexpr int kHistoryFlushIntervalS  = 5;

// --- Charts ------------------------------------------------------------------
inline constexpr int kChartDisplayPointCount  = 20;
inline constexpr int kReadingsChartRefreshMs  = 30'000;
inline constexpr int kChartDefaultBucketMin   = 5;

// --- Sensor display precision -----------------------------------------------
inline constexpr int kDecimalsMin     = 0;
inline constexpr int kDecimalsMax     = 6;
inline constexpr int kDecimalsDefault = 4;

// --- Logger status / events --------------------------------------------------
inline constexpr int kDefaultRetentionDays = 30;

// --- App-wide event recent list ---------------------------------------------
inline constexpr int kRecentEventsLimit = 20;

// --- File-based message handler ---------------------------------------------
inline constexpr qint64 kLogMaxBytes    = 5LL * 1024 * 1024; // 5 MB
inline constexpr int    kLogKeepBackups = 3;                 // app.log.1 … app.log.3

} // namespace CentralLogger::Defaults
