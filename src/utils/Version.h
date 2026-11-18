#pragma once

#include <QtGlobal>

// Single source of truth for protocol / schema version constants.
//
// - kSchemaVersion: SQLite PRAGMA user_version baked into 001_initial.sql.
//   Bump together with the canonical schema when you add a new migration
//   (see docs/thiet_ke_db.md).
// - kApiVersion: rest-config-contract-v1.md — `api_version` envelope field.
// - kModbusMapVersion: modbus-map-v1.md — HR0 map version the edge firmware
//   must report for this app to accept its layout.
//
// kAppMajor/kAppMinor/kAppPatch are generated from project(VERSION) by the
// root CMakeLists.txt configure_file() step (see Version.h.in).

#include "Version_generated.h"

namespace CentralLogger::Version {

inline constexpr int kSchemaVersion    = 7;
inline constexpr int kApiVersion       = 1;
inline constexpr int kModbusMapVersion = 1;
inline constexpr int kModbusMapVersion2 = 2;
inline constexpr quint16 kModbusStatusUnknown = 0xffff;
inline constexpr quint16 kModbusStatus03Maintenance = 0;
inline constexpr quint16 kModbusStatus03Auto = 1;
inline constexpr quint16 kModbusUpstreamUnknown = 0;
inline constexpr quint16 kModbusUpstreamConnected = 1;
inline constexpr quint16 kModbusUpstreamDisconnected = 2;

} // namespace CentralLogger::Version
