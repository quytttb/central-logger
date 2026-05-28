# Python → C++ Migration Map

Mapping từng Python module sang đề xuất C++ layer tương ứng trong repo mới.
Sắp xếp theo **payoff** (CPU / latency / thread safety / Python↔QML crossing).

## DB Schema (tóm tắt)

Nguồn: legacy `central-logger-app` → `src/central_logger/db/models.py` (không copy file; schema tóm tắt dưới đây)

```
logger_info
  id           INTEGER PK
  name         TEXT  NOT NULL  (index)
  host         TEXT  NOT NULL
  port         INT   DEFAULT 5020
  unit_id      INT   DEFAULT 1
  poll_interval_s INT DEFAULT 2
  timeout_s    REAL  DEFAULT 2.0
  enabled      BOOL  DEFAULT 1
  note         TEXT
  created_at   DATETIME
  api_base_url TEXT
  api_port     INT   DEFAULT 8080
  api_token    TEXT
  last_revision INT  DEFAULT -1

sensor_reading
  id           INTEGER PK
  logger_id    INT FK(logger_info.id)  (index)
  sensor_id    INT  (index)
  value        REAL
  valid        BOOL  DEFAULT 1
  alarm        BOOL  DEFAULT 0
  stale        BOOL  DEFAULT 0
  logger_timestamp INT DEFAULT 0
  recorded_at  DATETIME (index)

system_event
  id           INTEGER PK
  logger_id    INT FK nullable (index)
  logger_name  TEXT
  event_type   TEXT  -- "Alarm|Offline|Online|Warning|Info"
  message      TEXT
  level        TEXT  -- "critical|warning|error|info"
  created_at   DATETIME (index)

app_settings
  id           INTEGER PK = 1  (always single row)
  theme        TEXT  DEFAULT "dark"
  system_timezone TEXT DEFAULT "Asia/Ho_Chi_Minh"
  data_retention_days INT DEFAULT 30
  maintenance_mode BOOL DEFAULT 0
```

**Notes for C++:**
- SQLite via `QSqlDatabase` (`QSQLITE`, `Qt6::Sql`) — **đã chốt** trong repo mới: [`docs/adr/0001-db.md`](../../adr/0001-db.md) (không `libsqlite3` trực tiếp).
- Migrations: currently ALTER TABLE + CREATE INDEX in `db/session.py`; replicate as version-gated SQL in C++.
- Retention: `DELETE FROM sensor_reading WHERE recorded_at < :cutoff` — unchanged.

---

## Ưu tiên migration theo payoff

### Tier 1 — Migrate first (hot path, pure logic, no Python dependencies)

#### `services/modbus_map.py` → `ModbusMapParser` (header-only C++ library)

| Python | C++ proposal |
|--------|-------------|
| `decode_float_abcd(reg_high, reg_low)` | Inline `float decodeFloatABCD(uint16_t hi, uint16_t lo)` |
| `decode_uint32_be(reg_high, reg_low)` | Inline `uint32_t decodeUint32BE(uint16_t hi, uint16_t lo)` |
| `parse_header(regs)` → `LoggerHeader` | `LoggerHeader parseHeader(const QList<uint16_t>& regs)` |
| `parse_sensor_block(block)` → `SensorSnapshot` | `SensorSnapshot parseSensorBlock(const uint16_t* block)` |
| `parse_sensor_array(regs, count)` | `QList<SensorSnapshot> parseSensorArray(const QList<uint16_t>& regs, int count)` |
| Constants: `MAP_VERSION=1`, `HR_*`, `FLAG_*`, `SENSOR_FLAG_*`, `HEADER_REGISTERS=10`, `SENSOR_BLOCK_SIZE=8` | `constexpr` in `ModbusMapV1.h` |

**Register layout (frozen v1):**
```
HR0  = map version (must = 1)
HR1  = status flags  bit0=polling  bit1=rtu_connected  bit2=any_alarm
HR2..HR3  = Unix timestamp uint32 big-endian (HR2=high)
HR4  = sensor count N
HR10 + i*8  = sensor block i:
  +0  sensor_id  uint16
  +1  flags      bit0=valid  bit1=alarm  bit2=stale
  +2..+3  float32 ABCD big-endian
  +4..+7  reserved
```

---

#### `services/modbus_client.py` + `services/modbus_manager.py` → `ModbusService`

| Python | C++ proposal |
|--------|-------------|
| `AsyncModbusTcpClient` (pymodbus asyncio) | `QModbusTcpClient` (Qt 6 Modbus module) **or** raw `QTcpSocket` with Modbus frame codec |
| Per-logger asyncio tasks, exponential backoff | `QObject` per logger on `QThread` worker, `QTimer` for retry |
| `ReadOutcome` dataclass | `struct ReadOutcome { bool ok; QString error; QList<uint16_t> registers; }` |
| `reconnect_delay` backoff | `backoffMs` doubling 1s → 2s → ... cap at 30s |

**Recommended:** `QtModbusClient` for simplicity; raw socket if firmware has non-standard framing.

---

### Tier 2 — High value (DB writes + model update on every poll)

#### `controllers/modbus_bridge.py` → `ModbusBridge` C++ class

| Python | C++ proposal |
|--------|-------------|
| `threading.Thread` + `asyncio` event loop | `QThread` worker + `QEventLoop` or pure `QThread` with `QTimer` |
| `_on_snapshot(outcome, logger_config)` → DB insert + model update | `void onSnapshot(ReadOutcome outcome, LoggerConfig cfg)` — runs in worker thread |
| Batch `SensorReading` inserts | `QSqlDatabase` + `QSqlQuery` with transaction |
| `_snapshotForUi.emit` → `QueuedConnection` | `QMetaObject::invokeMethod(controller, "onSnapshotUi", Qt::QueuedConnection, ...)` |
| Header flag → `LoggerListModel.update_from_snapshot` | Same: emit signal to main thread |

---

#### `controllers/chart_queries.py` → `ChartQueryService` C++

| Python | C++ proposal |
|--------|-------------|
| `build_ingestion_chart_24h(bucket_minutes=5)` | `QString buildIngestionChart24h(int bucketMinutes)` |
| SQL: `GROUP BY (strftime('%s', recorded_at) - :start) / :bucket_sec` | Same SQL via `QSqlQuery` |
| `build_sensor_trending_poll_chart(logger_id, points, ...)` | `QString buildSensorTrendingChart(int loggerId, const QList<PollPoint>& points)` |
| In-memory `POLL_HISTORY_MAX = 24` deque per logger | `QQueue<PollPoint>` per logger, max 24 |
| `chart_timezone()` reads `AppSettings.system_timezone` | `ZoneInfo` → `QTimeZone` |
| Returns JSON string | Keep JSON (QML Canvas consumes it) **or** expose `QAbstractSeries` (Phase 2 ADR) |

**Benchmark baseline:** 2.3 ms avg @100k rows. Target: maintain ≤5 ms.

---

### Tier 3 — Moderate value (merge/status per poll)

#### `services/sensor_catalog.py` + `controllers/sensor_state.py` → `SensorStateService` C++

| Python | C++ proposal |
|--------|-------------|
| `merge_sensor_rows(modbus_rows, rest_rows, catalog)` | `QList<SensorRow> mergeSensorRows(...)` |
| `SensorState.poll_history[id]` deque | `QMap<int, QQueue<PollPoint>>` |
| `SensorState.sensor_catalog[id]` | `QMap<int, QList<SensorCatalogEntry>>` |
| `display_status` computation | Move to `SensorStatusHelper` (currently in `StatusBadges.js`) |
| `cache_sensors(logger_id, payload, force=False)` | `void cacheSensors(int loggerId, SensorPayload payload, bool force = false)` |
| REST readings stale check | Keep same logic; `QElapsedTimer` per logger |

---

#### `viewmodels/logger_list_model.py` → `LoggerListModel` C++ (`QAbstractListModel`)

| Python | C++ proposal |
|--------|-------------|
| `LoggerItem` dataclass (14 fields) | `struct LoggerItem` |
| `LoggerRoles` IntEnum (12 roles) | `enum class LoggerRole : int` |
| `onlineCount()`, `alarmCount()` | `Q_INVOKABLE int onlineCount() const`, `int alarmCount() const` |
| `add_logger`, `remove_logger`, `update_connection`, `update_from_snapshot` | `Q_SLOT void addLogger(...)`, etc. |
| `itemAt(index)` `Q_INVOKABLE` | `Q_INVOKABLE QVariantMap itemAt(int index) const` |

**Benefit:** Eliminates Python↔QML bridge crossing on every row access and dataChanged.

---

#### `viewmodels/sensor_monitoring_table_model.py` → `SensorMonitoringTableModel` C++ (`QAbstractTableModel`)

| Python | C++ proposal |
|--------|-------------|
| 5 columns, 5 roles | `enum class SensorRole : int` with 5 values |
| `updateFromJson(QVariantList)` slot | `Q_SLOT void updateFromJson(const QString& json)` |
| `_value_text(sensor_type, value)` | `static QString valueText(const QString& sensorType, const QVariant& value)` |
| `SensorRow` dataclass | `struct SensorRow` |

---

### Tier 4 — Lower priority (keep thin or Python-like in C++)

#### `controllers/rest_coordinator.py` + `services/rest_config_client.py` → `RestConfigService` C++

| Python | C++ proposal |
|--------|-------------|
| `httpx.AsyncClient` (asyncio) | `QNetworkAccessManager` (Qt network, non-blocking) |
| `LoggerConfigClient.get_config()`, `post_config()`, `get_readings()`, `get_latest_report()` | `void fetchConfig(int id)`, `void applyConfig(int id, int rev, QJsonObject cfg)`, etc. |
| `RestScheduler` per-logger timing | `QTimer` per logger or priority queue |
| `format_rest_error_message(result, endpoint, operation=...)` | `static QString formatRestError(...)` — keep verbose error strings |

#### `controllers/rest_scheduler.py` → integrate into `RestConfigService`

No significant porting complexity; `QTimer` per logger replaces per-item scheduling.

#### `controllers/rest_facade.py` → `RestEndpoint` struct

```cpp
struct RestEndpoint {
    QString host;
    int port = 8080;
    QString token;
    QString baseUrl;
    bool hasToken() const { return !token.isEmpty(); }
};
```

---

### Tier 5 — Keep simple / port last

#### `controllers/event_journal.py` → `EventJournalService` C++

Simple DB insert + dedup; `QSqlQuery` INSERT into `system_event`; dedup via `QHash<int, QString>` last-message cache.

#### `controllers/logger_ops.py` → `LoggerRepository` C++

CRUD functions (`insert_logger`, `update_connection`, `update_api`, `delete_logger_and_readings`, `logger_form_json`); 1:1 porting to `QSqlQuery`.

#### `controllers/settings_controller.py` → `SettingsController` C++ (`QObject` + `@QmlElement`)

`AppSettings` row id=1 read/write; `Q_PROPERTY` for theme, timezone, retention, maintenanceMode.

#### `services/qr_provision.py` + `utils/native_libs.py` → `QrProvisionService` C++

Replace `pyzbar` with `QZXing` (LGPL) or `ZBar` Qt wrapper; eliminate ZBar DLL Windows complexity.

#### `services/rest_auth.py` → `RestAuthHelper` C++

Token normalization (`strip`, `"Bearer "` prefix removal); small pure functions.

---

## Keep in Python (không cần port)

| Python | Lý do giữ |
|--------|-----------|
| `DashboardController` QML facade slots | Thin bridge; in C++ rewrite this becomes the C++ controller directly |
| `SettingsController` | Ported to C++ controller (thin enough) |
| QML `.js` logic files | Replaced by C++ ViewModel methods (see `06-qml-logic-inventory.md`) |
| Build scripts (`build.sh`, `build.ps1`, etc.) | New repo uses CMake + GitHub Actions; keep concept |
| `pysidedeploy.spec` | Replaced by Qt Installer Framework |

---

## QML Bridge pattern migration

| Pattern Python (hiện tại) | Pattern C++ (repo mới) |
|--------------------------|------------------------|
| `@QmlElement` + `QML_IMPORT_NAME = "CentralLogger.Core"` | `QML_ELEMENT` macro + `qt_add_qml_module(... URI "CentralLogger.Core" ...)` in CMakeLists.txt |
| `@QmlSingleton` `AppState` | `QML_SINGLETON` macro on C++ `AppState : QObject` |
| `@Slot(...)` | `Q_SLOT` / `Q_INVOKABLE` |
| `@Property(type, notify=signal)` | `Q_PROPERTY(type name READ ... WRITE ... NOTIFY ...)` |
| `Signal(int, "QString")` | `Q_SIGNAL void mySignal(int id, const QString& msg)` |
| `rootContext().setContextProperty("TrayCtl", ...)` | C++ `QSystemTrayIcon` exposed as `Q_PROPERTY` or context property |
| `Qt.ConnectionType.QueuedConnection` | `Qt::QueuedConnection` in `connect(...)` |
| `asyncio.run_coroutine_threadsafe(coro, loop)` | `QMetaObject::invokeMethod` or post to `QThreadPool` |

---

## Suggested C++ project layout (`src/` in new repo)

```
src/
├── core/
│   ├── modbus/
│   │   ├── ModbusMapParser.h       # header-only, no Qt
│   │   ├── ModbusService.h/.cpp    # QModbusTcpClient + QThread
│   │   └── ModbusBridge.h/.cpp     # snapshot → DB + model
│   ├── rest/
│   │   ├── RestConfigService.h/.cpp
│   │   ├── RestEndpoint.h
│   │   └── RestAuthHelper.h
│   ├── chart/
│   │   └── ChartQueryService.h/.cpp
│   └── event/
│       ├── EventJournalService.h/.cpp
│       └── RetentionService.h/.cpp
├── data/
│   ├── models/                     # LoggerInfo, SensorReading, etc. as structs
│   ├── repositories/
│   │   ├── LoggerRepository.h/.cpp
│   │   └── SensorRepository.h/.cpp
│   └── db/
│       ├── Database.h/.cpp         # QSqlDatabase init + migrations
│       └── migrations/             # v1.sql, v2.sql ...
├── viewmodels/
│   ├── AppState.h/.cpp             # QML_SINGLETON
│   ├── LoggerListModel.h/.cpp      # QAbstractListModel
│   ├── SensorMonitoringTableModel.h/.cpp
│   ├── RecentEventsModel.h/.cpp
│   ├── DashboardController.h/.cpp  # QML facade
│   ├── SettingsController.h/.cpp
│   ├── LoggerFormViewModel.h/.cpp  # logic from LoggerFormLogic.js
│   └── LoggerDetailViewModel.h/.cpp # logic from LoggerDetailLogic.js
├── services/
│   ├── SensorStateService.h/.cpp
│   ├── SensorMerger.h/.cpp
│   └── QrProvisionService.h/.cpp
└── main.cpp
```
