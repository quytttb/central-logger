# Config Push Failed (Save Logger) — Windows Agent Walkthrough

**Date:** 2026-05-28  
**Branch:** pull latest `main` (or branch containing this fix) before build  
**Audience:** Agent or developer on **Windows** verifying the fix after Linux implementation

---

## Executive Summary

Saving a **new logger** failed with banner `Config push failed (logger #N): Configuration changed on device...` even though **Connect** (`GET /config`) succeeded. SQLite stayed empty because the DB transaction rolled back.

**Root cause:** `POST /api/v1/config` body was missing required fields `api_version` and `request_id` (edge OpenAPI `ConfigRequest`). This is **not** a Qt/Windows platform bug — the same C++ code runs on Linux and Windows.

**Fix:** `RestConfigService::applyConfig` now sends the full envelope; error mapping and event logging on rollback were also corrected.

---

## Symptoms (before fix)

| Step | Result |
|------|--------|
| Add Logger form → **Connect** | OK — probe loads config |
| **Save** | Red banner: `Config push failed (logger #1): Configuration changed on device. Connect again, then save.` |
| SQLite | No row in `logger_info` |
| `system_event` | Often empty (FK failed after rollback when logging with transient logger id) |

**Log excerpt (typical):**

```
Apply config failed. Status: 422 Error: "Configuration changed on device..."
Body: {"detail":[{"type":"missing","loc":["body","api_version"],...},
                 {"type":"missing","loc":["body","request_id"],...}]}
```

**Body Central sent (incomplete):**

```json
{"config":{"station_code":"TRAM-1"},"expected_revision":1}
```

---

## Files changed

| File | Change |
|------|--------|
| [`src/network/rest/RestConfigService.cpp`](../../src/network/rest/RestConfigService.cpp) | POST envelope adds `api_version: 1`, `request_id: "central-<uuid>"` |
| [`src/network/rest/RestConfigService.h`](../../src/network/rest/RestConfigService.h) | Comment updated for ConfigRequest |
| [`src/network/rest/RestConfigParser.h`](../../src/network/rest/RestConfigParser.h) | `formatRestError` — no false "revision changed" on Pydantic `missing` 422 |
| [`src/core/DashboardController.cpp`](../../src/core/DashboardController.cpp) | On push fail after rollback: `logEvent(0, ...)` instead of `logEvent(savedId, ...)` |
| [`docs/contracts/rest-config-contract-v1.md`](../contracts/rest-config-contract-v1.md) | POST example aligned with edge OpenAPI |
| [`tests/network/test_rest_config_parser.cpp`](../../tests/network/test_rest_config_parser.cpp) | Tests for 422 missing-field vs revision conflict |

---

## Correct POST payload (after fix)

```json
{
  "api_version": 1,
  "request_id": "central-a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "expected_revision": 1,
  "config": {
    "station_code": "TRAM-1"
  }
}
```

Reference: `data-logger/openapi-v1.yaml` schema `ConfigRequest`; legacy Python client `central-logger-app/.../rest_config_client.py` `apply_config`.

---

## Build on Windows

Prerequisites: Qt **6.11** (Quick, Qml, Sql, Network, SerialBus, Graphs), CMake 3.16+, MSVC or MinGW per your usual setup.

```powershell
cd C:\Projects\central_logger   # adjust path
cmake --preset windows          # or your project preset
cmake --build --preset windows
```

If no preset, typical flow:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.11.0\msvc2019_64"
cmake --build build --config Release
```

Run binary: `build\src\app\Release\central_logger.exe` (path may vary).

---

## Manual test checklist

1. **Pull** this commit and rebuild.
2. Open app → **Loggers** → **Add Logger**.
3. Enter Host, API Port, Token → **Connect** → expect success / fields populated.
4. Set **Station code** (e.g. `TRAM-1`) different from probed value if you need to force a config patch.
5. **Save** → expect success (no red banner).
6. Verify SQLite (default `~/.central-logger/central-logger.db` or app path):
   - Row exists in `logger_info` with your `station_code`.
   - `last_revision` updated if edge returned `applied_revision`.
7. Optional: **Events** view — after a failed push (simulate with wrong token), a **Warning** row with `logger_id` NULL may appear (global event) describing the failed push.

### Why Linux might have looked fine with the same edge

`saveLoggerFromForm` only calls `POST /config` when `buildEditPatch` is **non-empty**. If station code/name/poll already match the probe snapshot, Save commits DB **without** POST — no 422. On Windows you may always change `station_code` on Add, so POST runs every time.

---

## Regression tests (any OS)

From build directory:

```bash
ctest -R rest_config --output-on-failure
```

Or run `test_rest_config_parser` directly. New cases:

- `formatRestErrorMaps422MissingFieldsNotRevision` — Pydantic body with `expected_revision` in `input` must **not** map to "Configuration changed on device".
- `formatRestErrorMaps422MissingApiVersion` — missing `api_version` → "missing fields" message.

---

## False positive message (before fix)

`formatRestError` treated any HTTP 422 whose body contained the substring `"revision"` as a revision conflict. Pydantic error JSON includes `"expected_revision"` inside `"input"` → misleading banner even when the real error was **missing `api_version` / `request_id`**.

---

## Station code — hidden from UI (follow-up)

**Symptom:** Manual station code on Add could POST to edge and break monitoring.

**Fix:**

- **No Station code field** in Add/Edit dialog.
- On **Add**, `logger_info.station_code` is taken from device `GET /config` snapshot only (`probedStationCode()`).
- On **Edit**, station code in DB is unchanged (not editable).
- **Never** include `station_code` in `POST /config` patch; POST only when `buildEditPatch` finds real changes (`station_name`, `poll_interval`, …).
- Loggers table column **Station** removed; list shows Name, Host, …

---

## If Save still fails after this fix

| HTTP | Check |
|------|--------|
| 401 | Token empty or wrong — Scan QR / re-enter token |
| 409 / revision mismatch | Another client changed config — **Connect** again, then **Save** |
| 422 forbidden fields | POST `config` must not include `logger_serial`, `cloud_enabled`, `cloud_endpoint` |
| Timeout | Host/API port, firewall, edge service running |

Capture raw response body from app log or a proxy (Fiddler) and compare with [`docs/contracts/rest-config-contract-v1.md`](../contracts/rest-config-contract-v1.md).
