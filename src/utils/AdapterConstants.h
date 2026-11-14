#pragma once

#include <QtGlobal>

// Defaults and wire limits for the independent v1 adapter processes.
namespace CentralLogger::AdapterDefaults {

inline constexpr quint16 kModbusPort = 15020;
inline constexpr quint16 kRestPort = 18080;
inline constexpr int kUnitId = 1;
inline constexpr int kPollIntervalMs = 10'000;
inline constexpr int kRequestTimeoutMs = 5'000;
inline constexpr int kStaleAfterMs = 30'000;
inline constexpr int kSnapshotLeaseMs = 5'000;
inline constexpr int kInitialBackoffMs = 5'000;
inline constexpr int kMaximumBackoffMs = 300'000;
inline constexpr int kCatalogRefreshMs = 60'000;
inline constexpr qint64 kFutureClockToleranceMs = 30'000;
inline constexpr qint64 kMaxSecretFileBytes = 16 * 1024;
inline constexpr qint64 kMaxSourceTimestampMs = 1'000'000'000'000'000LL;
inline constexpr int kMinPollMs = 1'000;
inline constexpr int kMaxPollMs = 3'600'000;
inline constexpr int kMinTimeoutMs = 100;
inline constexpr int kMaxTimeoutMs = 300'000;
inline constexpr int kMinStaleMs = 1'000;
inline constexpr int kMaxStaleMs = 86'400'000;
// Default IDs match the six analog gas channels observed on .17
// (159,160,161,162,164,165). The .16 installation exposes a different set
// (164-170 including mA with an empty unit) and must pass --analog-ids
// explicitly after verifying each channel is analog.
inline constexpr char kObservedOsgAnalogIds[] = "159,160,161,162,164,165";

inline constexpr int kHeaderRegisters = 10;
inline constexpr int kAnalogRegisters = 8;
inline constexpr int kMaxReadRegisters = 125;
inline constexpr int kRegisterAddressSpace = 65'536;
inline constexpr int kMaxAnalogs =
    (kRegisterAddressSpace - kHeaderRegisters) / kAnalogRegisters;

inline constexpr int kMaxHttpHeaderBytes = 16 * 1024;
inline constexpr int kMaxHttpBodyBytes = 64 * 1024;
inline constexpr int kHttpRequestTimeoutMs = 10'000;
inline constexpr int kMaxHttpClients = 16;
inline constexpr qint64 kMaxUpstreamBodyBytes = 4 * 1024 * 1024;

} // namespace CentralLogger::AdapterDefaults
