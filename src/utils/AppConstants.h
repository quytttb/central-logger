#pragma once

// Application-wide default values and clamp bounds.
//
// Centralizes "magic numbers" that were previously inlined across data models,
// network configs and form controllers. Header-only; lives in the `utils` base
// library so it is includable from data / core / network layers.
namespace CentralLogger::Defaults {

// --- Network defaults --------------------------------------------------------
inline constexpr int kDefaultModbusPort   = 5020;
inline constexpr int kDefaultApiPort      = 8080;
inline constexpr int kDefaultModbusUnitId = 1;

// --- Config defaults ---------------------------------------------------------
inline constexpr int kDefaultPollIntervalSec = 2;
inline constexpr int kDefaultTimeoutSec      = 2;

// --- Interval clamp bounds (poll / flush / settings) -------------------------
inline constexpr int kMinIntervalSec = 1;
inline constexpr int kMaxIntervalSec = 3600; // 1 hour

// --- Derived millisecond helpers --------------------------------------------
inline constexpr int kMsPerSecond            = 1000;
inline constexpr int kDefaultPollIntervalMs  = kDefaultPollIntervalSec * kMsPerSecond;
inline constexpr int kDefaultTimeoutMs       = kDefaultTimeoutSec * kMsPerSecond;

} // namespace CentralLogger::Defaults
