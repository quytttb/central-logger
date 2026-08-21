#pragma once

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

inline constexpr int kSchemaVersion    = 6;
inline constexpr int kApiVersion       = 1;
inline constexpr int kModbusMapVersion = 1;

} // namespace CentralLogger::Version
