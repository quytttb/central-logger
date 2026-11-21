# `src/network/` — Modbus + REST

Edge-network layer. Task 4 ships the Modbus side; REST lands in Task 5.

## Modbus (Task 4)

| File | Role |
|------|------|
| [`modbus/ModbusTypes.h`](modbus/ModbusTypes.h) | `ModbusHeader`, `AnalogSample`, `PollSnapshot` value types. |
| [`modbus/ModbusMapParser.h`](modbus/ModbusMapParser.h) | Header-only parser for HR0–HR6 header, ABCD-decoded analog blocks, and FC02/FC01 bit arrays per [`docs/contracts/modbus-map-v1.md`](../../docs/contracts/modbus-map-v1.md). |
| [`modbus/ModbusPollPlan.h`](modbus/ModbusPollPlan.h) | `planPollReads(na, ndi, ndo)` returns the PDU sequence (FC03 header → FC03 analog chunks ≤120 reg → FC02 DI → FC01 DO). |
| [`modbus/ModbusService.{h,cpp}`](modbus/ModbusService.h) | `QObject` worker on its own `QThread`. Owns one `QModbusTcpClient` and one `QTimer` per logger, runs the poll cycle, and emits `pollFinished(PollSnapshot)` via `Qt::QueuedConnection`. |
| [`modbus/ModbusBridge.{h,cpp}`](modbus/ModbusBridge.h) | Lives on the main thread. Slot `applySnapshot` writes `logger_info.status` + `last_seen`, calls `SensorCatalogRepository::ensureExists` for every analog/DI/DO id, and batches `SensorReading` rows via `SensorReadingRepository::insertBatch`. Re-emits `snapshotApplied(loggerId, online, sensorCount, statusFlags)`. |

### Thread layout

```text
main thread ─────────── DashboardController ─── LoggerListModel
       │                       ▲
       │ Qt::QueuedConnection  │
       ▼                       │
ModbusBridge ◀────── pollFinished(snapshot)
       ▲                       │
       │ ensureExists + insertBatch
       │ updateStatusAndLastSeen        ┌── QThread "ModbusWorker"
       │                                │
SQLite (Database connection, main)      ModbusService → QModbusTcpClient[*]
```

* Only the bridge touches SQLite — the worker thread never opens a DB connection.
* `DashboardController::reloadLoggers()` and `addLogger` / `updateLogger` /
  `removeLogger` push the current set of enabled loggers to the worker via
  `ModbusService::syncLoggers(QVector<LoggerRuntimeConfig>)`.

## Read-only REST

| File | Role |
|------|------|
| [`rest/RestConfigParser.h`](rest/RestConfigParser.h) | Parses GET config, sensor catalog and thresholds per [`docs/contracts/rest-config-contract-v1.md`](../../docs/contracts/rest-config-contract-v1.md). Formats HTTP errors and raw JSON diagnostics. |
| [`rest/RestConfigService.{h,cpp}`](rest/RestConfigService.h) | Main-thread GET-only client. Reads `/api/v1/config` with the supplied or saved Bearer token and offers one-shot `/api/v1/readings` for diagnostics. |

### Rules

- `/readings` is **never** called from `ModbusBridge`, `ModbusService`, timers, or UI — `RestConfigService::fetchReadingsDebug()` is kept strictly for C++ developer diagnostics (`qInfo` logging).
- Live values stay Modbus-only (FC03, FC02 and FC01 when applicable). Central never sends Modbus writes.
- `LoggerFormController` reads config on Add or Reload in `Settings > Loggers`. Save touches only SQLite; Edit makes no REST request. Thresholds are a local copy regardless of what write capability the edge advertises.

### Wiring

`main.cpp` constructs one `RestConfigService`, sets the DB, and hands it to
`LoggerFormController`. `ThresholdController` only reads the SQLite mirror
and refreshes it for display after a successful Reload.
