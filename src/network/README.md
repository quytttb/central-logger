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

## REST (Task 5)

| File | Role |
|------|------|
| [`rest/RestConfigParser.h`](rest/RestConfigParser.h) | Header-only parser for `GET /config` (`revision`, `sensors[]`), `POST /config` apply result (`applied_revision`), `formatRestError(httpStatus, body)` per [`docs/contracts/rest-config-contract-v1.md`](../../docs/contracts/rest-config-contract-v1.md). Also `prettyJson` for the debug dialog. |
| [`rest/RestConfigService.{h,cpp}`](rest/RestConfigService.h) | `QObject` on the main thread, owns one `QNetworkAccessManager`. Issues `GET/POST /api/v1/config` and the **one-shot** `GET /api/v1/readings` for the debug dialog. Bearer header from `LoggerInfo.apiToken`; per-`(loggerId, endpoint)` in-flight guard to coalesce double clicks. |

### Rules (contract D1–D7)

- `/readings` is **never** called from `ModbusBridge`, `ModbusService`, or any timer — only `LoggerDetailViewModel::fetchReadingsDebug()` triggered by a user click.
- Live values stay Modbus-only; catalog metadata (`sensor_type`, `name`, `unit`, thresholds) lands via `SensorCatalogRepository::upsert` on **Save** from `LoggerFormDialog` (`saveLoggerFromForm`).
- Config GET/POST is **not** on Logger Detail — only `fetchReadingsDebug` and report download there.

### Wiring

`main.cpp` constructs one `RestConfigService`, sets the DB, and registers it on
`DashboardController` (form Connect / Save) and `LoggerDetailViewModel` (debug
readings + report only).
