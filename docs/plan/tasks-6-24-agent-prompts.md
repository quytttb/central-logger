# Tasks 6–24 — Agent prompts (sau nền móng 1–5)

Tài liệu này chứa **19 task** độc lập (Task 6 → 24), bỏ qua Tasks **1–5** đã hoàn thành.  


## Quy ước chung (mọi task)

- **SoT:** [`thiet_ke_db.md`](../thiet_ke_db.md), [`contracts/`](../contracts/), [`AGENTS.md`](../../AGENTS.md)
- **MVVM:** logic C++; QML chỉ bind
- **Không** port Python / pytest cũ
- **Không** sửa frozen contracts trừ khi user duyệt
- **Không** commit trừ khi user yêu cầu
- **Cấm FE-014:** không poll/merge `GET /readings` vào bảng live (chỉ debug Task 5)

## Đã có sẵn (không tạo task trùng)

| FE | Trạng thái |
|----|------------|
| FE-001 | `AppState` + Dashboard StatCards |
| FE-004 | List loggers (chưa search → Task 17) |
| FE-005, FE-007 | Task 3 CRUD |
| FE-008 | Task 4 Modbus |
| FE-012, FE-013 | Task 5 REST config + debug readings |
| FE-015 | Settings form |
| FE-011 (một phần) | Roles `online` / `polling` / `anyAlarm` trên list |

**Không làm:** FE-014 (REST merge `/readings` vào live).

## Bảng tra nhanh

| Task | Tên | FE | Làm sau |
|------|-----|-----|---------|
| 6 | SensorMerger | FE-009 | 5 |
| 7 | SensorMonitoringTableModel | FE-009 | 6 |
| 8 | LoggerDetailViewModel sensor | FE-009 | 7 |
| 9 | Logger Detail UI table/badges | FE-009, 011 | 8 |
| 10 | RecentEventsModel | FE-003 | 2 |
| 11 | Dashboard events UI | FE-003 | 10 |
| 12 | ChartQueryService SQL | FE-002 | 4 |
| 13 | Dashboard Graphs ingestion | FE-002 | 12 |
| 14 | PollHistoryStore | FE-010 | 6 |
| 15 | Detail Graphs trending | FE-010 | 14, 9 |
| 16 | Retention purge | FE-016 | 13 |
| 17 | Logger search | FE-004 | 3 |
| 18 | Tray + frameless | FE-017 | 2 | **Dropped** — xem `HANDOFF.md` |
| 19 | Modbus system events | FE-003/008 | 11, 4 |
| 20 | Download report | FE-021 | 5 |
| 21 | QR provision | FE-020 | 3 | **Dropped** — copy-paste text, xem HANDOFF |
| 22 | Config probe | FE-022 | 5 |
| 23 | Edit form full | FE-006 | 22, 5 |
| 24 | Responsive UI | FE-023 | 18 |

## Cách giao agent mỗi lần

```
Implement Task N as specified below. Do NOT edit any .plan.md files.
Mark plan todos in_progress then completed.
Requirements: docs/thiet_ke_db.md, docs/contracts/, AGENTS.md.
Build and ctest must pass before done.
No FE-014: never merge GET /readings into live UI.

[Paste block "Prompt giao agent" của Task N bên dưới]
```

---

# MVP — Core

## Task 6 — SensorMerger (merge catalog + Modbus snapshot)

**FE:** FE-009 (backend)  
**Làm sau:** Task 5  
**Phụ thuộc repo:** Task 4 (`PollSnapshot`), Task 1 (`SensorCatalogRepository`)

### Mục tiêu

Thư viện merge thuần: catalog SQLite + snapshot Modbus → danh sách hàng hiển thị (`SensorLiveRow`). Chưa QML, chưa model.

### Out of scope

- `QAbstractTableModel`, QML, Qt Graphs
- REST `/readings`
- Ghi thêm DB (catalog đã có từ Task 4/5)

### Deliverables

| File | Nội dung |
|------|----------|
| `src/core/sensors/SensorLiveRow.h` | struct: `edgeSensorId`, `name`, `sensorType`, `unit`, `value`, `valid`, `alarm`, `stale`, `displayStatus`, `timestamp` |
| `src/core/sensors/SensorMerger.h` / `.cpp` | `QVector<SensorLiveRow> buildRows(qint64 loggerId, const PollSnapshot &snap, const QVector<LoggerSensor> &catalog)` |
| Logic | Analog: float + unit; DI/DO: ON/OFF từ bit; `displayStatus` từ flags + optional threshold catalog |
| `src/core/CMakeLists.txt` | Thêm sources |

### Tests

`tests/core/test_sensor_merger.cpp`:

- Catalog 2 analog + 1 DI + snapshot khớp → đúng số hàng, value, status
- Sensor trong snapshot không có catalog → vẫn hiện (name fallback `Sensor #id` hoặc type#id)
- Catalog-only không có trong snapshot → có thể ẩn hoặc hiện stale (theo `thiet_ke_db.md` §3.3 — ưu tiên có snapshot)

### Gating

- [ ] `ctest` pass (`test_sensor_merger` + regression data/network/core)
- [ ] Build thành công
- [ ] Không include `/readings` trong module

### Prompt giao agent

```
Task 6: SensorMerger + SensorLiveRow (src/core/sensors/).

Implement pure merge: logger_sensor catalog rows + Network::PollSnapshot → QVector<SensorLiveRow>.
Analog float+unit; DI/DO ON/OFF; displayStatus from Modbus flags.
Unit tests in tests/core/test_sensor_merger.cpp.
No QML, no QAbstractTableModel, no REST /readings.
SoT: docs/thiet_ke_db.md §3.3, docs/contracts/modbus-map-v1.md.
Do not edit plan files. Build + ctest must pass.
```

---

## Task 7 — SensorMonitoringTableModel + snapshot cache

**FE:** FE-009 (model)  
**Làm sau:** Task 6

### Mục tiêu

`QAbstractTableModel` 5 cột + cache snapshot/rows per `loggerId`; cập nhật khi poll thành công.

### Out of scope

- QML TableView (Task 9)
- Charts

### Deliverables

| File | Nội dung |
|------|----------|
| `SensorMonitoringTableModel.h/.cpp` | Roles: `sensorId`, `name`, `value`, `unit`, `displayStatus`; `updateForLogger(qint64 id)` gọi Merger |
| `SensorSnapshotCache` (có thể cùng file) | `QHash<qint64, PollSnapshot>` hoặc merged rows — lưu snapshot mới nhất |
| Hook | `DashboardController::onSnapshotApplied` → cache + `dataChanged` model |

### Tests

`tests/core/test_sensor_monitoring_table_model.cpp`: seed cache + update → `rowCount`, `data(index, role)`.

### Gating

- [ ] ctest + build
- [ ] Hook không block UI thread (chỉ DB read catalog trên main — OK)

### Prompt giao agent

```
Task 7: SensorMonitoringTableModel (QAbstractTableModel, 5 columns) + per-logger snapshot cache.

Use SensorMerger from Task 6. Wire DashboardController::onSnapshotApplied to update cache and refresh model.
Tests: tests/core/test_sensor_monitoring_table_model.cpp.
No QML yet. No /readings.
```

---

## Task 8 — LoggerDetailViewModel: expose sensor table

**FE:** FE-009 (ViewModel)  
**Làm sau:** Task 7

### Mục tiêu

`LoggerDetailViewModel` cung cấp model bảng sensor cho QML; refresh khi đổi `loggerId` và khi poll snapshot cho đúng logger.

### Out of scope

- QML layout (Task 9)
- Chart

### Deliverables

| File | Nội dung |
|------|----------|
| `LoggerDetailViewModel.h/.cpp` | `Q_PROPERTY(SensorMonitoringTableModel* sensorTable READ ... CONSTANT)` |
| | `setLoggerId` → load catalog baseline + `sensorTable->updateForLogger(id)` |
| | Slot `onLoggerSnapshotUpdated(qint64 id)` connect từ DashboardController |

### Gating

- [ ] Build; VM property readable từ QML

### Prompt giao agent

```
Task 8: Extend LoggerDetailViewModel to expose SensorMonitoringTableModel to QML.

On setLoggerId and on Modbus snapshot for this loggerId, refresh table via Task 7 model.
Keep existing REST properties (rawConfig, fetch/apply/debug) from Task 5.
Add signal from DashboardController if needed: loggerSnapshotUpdated(loggerId).
No QML file changes except if needed for registration.
```

---

## Task 9 — Logger Detail UI: badges + sensor table

**FE:** FE-009, FE-011  
**Làm sau:** Task 8

### Mục tiêu

Hoàn thiện màn Logger Detail: overview badges + bảng live; giữ REST toolbar Task 5.

### Out of scope

- Trending chart (Task 15)
- Edit form đầy đủ (Task 23)

### Deliverables

| File | Nội dung |
|------|----------|
| `LoggerDetailView.qml` | Row badges: online, polling, anyAlarm, rtuConnected |
| | `TableView` + header: Sensor, Name, Value, Unit, Status |
| | `model: detailVm.sensorTable` |
| | Giữ Fetch/Apply/Debug + rawConfig pane |
| `LoggerListModel` (optional) | Role `rtuConnected` từ HR1 — `updateLoggerRow` |

### Gating

- [ ] Build app
- [ ] Manual: logger online → bảng đổi theo poll; Fetch config → name/unit/type cập nhật

### Prompt giao agent

```
Task 9: LoggerDetailView.qml — overview badges (online, polling, anyAlarm, rtuConnected) + TableView bound to LoggerDetailViewModel.sensorTable.

Keep REST toolbar and debug dialog from Task 5. Optionally add rtuConnected to LoggerListModel/updateLoggerRow from Modbus HR1.
Manual: live table updates on poll. FE-009, FE-011.
```

---

## Task 10 — RecentEventsModel (C++)

**FE:** FE-003  
**Làm sau:** Task 2

### Mục tiêu

Model danh sách 20 event gần nhất từ `system_event`.

### Out of scope

- QML ListView (Task 11)
- Ghi event từ Modbus (Task 19)

### Deliverables

| File | Nội dung |
|------|----------|
| `RecentEventsModel.h/.cpp` | `QAbstractListModel`; roles: `id`, `loggerId`, `loggerName`, `eventType`, `message`, `level`, `createdAt` |
| | `reload()` → `EventRepository::listRecent(20)` |

### Tests

`tests/core/test_recent_events_model.cpp`

### Gating

- [ ] ctest + build

### Prompt giao agent

```
Task 10: RecentEventsModel — QAbstractListModel, top 20 from EventRepository::listRecent(20).
Roles: id, loggerId, loggerName, eventType, message, level, createdAt.
Unit test with in-memory DB. No QML in this task.
```

---

## Task 11 — Dashboard: recent events list + navigation

**FE:** FE-003  
**Làm sau:** Task 10

### Mục tiêu

Hiển thị events trên Dashboard; click mở Logger Detail.

### Deliverables

| File | Nội dung |
|------|----------|
| `DashboardView.qml` | Section "Recent events" + `ListView` delegate |
| | `onClicked` → `root.selectLogger(loggerId)` |
| `DashboardController` | `Q_PROPERTY(RecentEventsModel* recentEvents)` + `reloadRecentEvents()` sau CRUD |

### Gating

- [ ] Click event → `logger-detail`, đúng `selectedLoggerId`
- [ ] List refresh sau thêm logger

### Prompt giao agent

```
Task 11: DashboardView recent events ListView using RecentEventsModel (Task 10).
Click navigates via selectLogger(loggerId). Reload model after logger CRUD in DashboardController.
FE-003.
```

---

## Task 12 — ChartQueryService (SQL ingestion 24h)

**FE:** FE-002 (data layer)  
**Làm sau:** Task 4

### Mục tiêu

Truy vấn SQLite aggregate bucket 5 phút, 24h — chưa vẽ chart.

### Out of scope

- Qt Graphs QML (Task 13)
- Trending RAM (Task 14)

### Deliverables

| File | Nội dung |
|------|----------|
| `src/core/charts/ChartQueryService.h/.cpp` | `struct IngestionPoint { QString label; int count; };` `QVector<IngestionPoint> ingestionLast24h(int bucketMinutes=5)` |
| SQL | `COUNT(*)` GROUP BY time bucket trên `sensor_reading.recorded_at`; filter last 24h |

### Tests

`tests/core/test_chart_query_service.cpp`

### Gating

- [ ] ctest; query hợp lý trên DB test

### Prompt giao agent

```
Task 12: ChartQueryService — SQL aggregation for sensor_reading last 24h in 5-minute buckets.
Return QVector of label + count. Unit test with :memory: DB.
No QML. FE-002 data half. See thiet_ke_db and NFR in THAM_KHAO phase1.
```

---

## Task 13 — Dashboard ingestion chart (Qt Graphs)

**FE:** FE-002 (UI)  
**Làm sau:** Task 12

### Mục tiêu

Vẽ biểu đồ ingestion trên Dashboard bằng **Qt Graphs**.

### Out of scope

- Trending (Task 15)

### Deliverables

| File | Nội dung |
|------|----------|
| `DashboardController` | Property/signal `ingestionChartChanged` + `refreshIngestionChart()` |
| `DashboardView.qml` | `import QtGraphs`; `GraphsView` + `LineSeries` |
| `HANDOFF.md` | ADR: Qt Graphs thay Qt Charts |
| Empty state | "No readings yet" |

### Gating

- [ ] Build; offscreen không crash
- [ ] Manual: DB có readings → chart có điểm

### Prompt giao agent

```
Task 13: Dashboard ingestion chart UI using Qt Graphs (import QtGraphs).
Consume ChartQueryService from Task 12. Expose refresh via DashboardController property/signal.
Update HANDOFF ADR for Qt Graphs. FE-002. Empty state when no data.
```

---

## Task 14 — PollHistoryStore (RAM trending data)

**FE:** FE-010 (data)  
**Làm sau:** Task 6

### Mục tiêu

Vòng đệm RAM ~24 điểm poll cho **analog** sensors, per logger.

### Out of scope

- Graphs QML (Task 15)

### Deliverables

| File | Nội dung |
|------|----------|
| `PollHistoryStore.h/.cpp` | `append(loggerId, PollSnapshot)` — chỉ analog; max 24 points per series |
| API | `QVariantList seriesForLogger(qint64 id)` cho QML |

### Tests

`tests/core/test_poll_history_store.cpp`

### Gating

- [ ] ctest + build

### Prompt giao agent

```
Task 14: PollHistoryStore — in-memory deque max 24 points per analog edge_sensor_id per logger.
Append from PollSnapshot on successful poll. API to export series for Qt Graphs (Task 15).
Unit tests. FE-010 data layer.
```

---

## Task 15 — Logger Detail trending chart (Qt Graphs)

**FE:** FE-010 (UI)  
**Làm sau:** Task 14, Task 9

### Mục tiêu

Biểu đồ trending multi-series trên Logger Detail.

### Deliverables

| File | Nội dung |
|------|----------|
| `LoggerDetailViewModel` | Property `trendingSeries` + refresh on snapshot |
| `LoggerDetailView.qml` | `GraphsView` dưới sensor table |

### Gating

- [ ] Manual: vài poll → line di chuyển
- [ ] Chỉ analog

### Prompt giao agent

```
Task 15: Logger Detail trending chart with Qt Graphs using PollHistoryStore (Task 14).
Wire append on Modbus snapshot success. Multi-series analog only. FE-010.
```

---

## Task 16 — Data retention purge

**FE:** FE-016  
**Làm sau:** Task 13

### Mục tiêu

Xóa readings cũ theo `data_retention_days`; timer + hooks settings.

### Deliverables

| File | Nội dung |
|------|----------|
| `purgeOldData()` | `SensorReadingRepository::purgeOlderThan(cutoff)` |
| Triggers | App start; `SettingsController::save()`; `QTimer` 3600s |
| | Invalidate ingestion chart khi deleted > 0 |

### Tests

Insert old reading → purge removes.

### Gating

- [ ] ctest + build

### Prompt giao agent

```
Task 16: Data retention purge (FE-016). purgeOldData from app_settings.data_retention_days.
Run on startup, on settings save, and QTimer hourly. Invalidate ingestion chart (Task 13) when rows deleted.
Qt Test for purge.
```

---

## Task 17 — Logger list search filter

**FE:** FE-004  
**Làm sau:** Task 3

### Mục tiêu

Tìm kiếm logger trên Loggers view.

### Deliverables

| File | Nội dung |
|------|----------|
| `LoggerSearchProxyModel` | `QSortFilterProxyModel` trên `DashboardController.loggers` |
| `LoggersView.qml` | `TextField` search |

### Gating

- [ ] Filter stationCode, name, host

### Prompt giao agent

```
Task 17: Logger list search — QSortFilterProxyModel on LoggerListModel + search TextField in LoggersView.qml.
Filter stationCode, name, host. FE-004.
```

---

## Task 18 — System tray + frameless window

> **Trạng thái: DROPPED (2026-05).** Không implement — giữ cửa sổ OS chuẩn. Agent **không** làm task này trừ khi user yêu cầu lại.

**FE:** FE-017  
**Làm sau:** Task 2

### Mục tiêu

Frameless window + tray minimize/restore/quit.

### Deliverables

| File | Nội dung |
|------|----------|
| `main.cpp` | `QSystemTrayIcon`, menu Show / Quit |
| `Main.qml` | `flags: Qt.Window | Qt.FramelessWindowHint` |
| `AppTopBar.qml` | Close → hide window |

### Gating

- [ ] Tray show/restore/quit (Linux)

### Prompt giao agent

```
Task 18: QSystemTrayIcon in main.cpp + frameless ApplicationWindow in Main.qml.
AppTopBar close hides to tray. FE-017. Document platform notes in HANDOFF.
```

---

## Task 19 — Modbus-driven system events

**FE:** FE-003 (bổ sung), FE-008  
**Làm sau:** Task 11, Task 4

### Mục tiêu

Ghi `system_event` khi logger online/offline đổi (edge-trigger, không spam mỗi poll).

### Deliverables

| File | Nội dung |
|------|----------|
| Event logging | Transition offline↔online; optional alarm edge |
| | `EventRepository::insert` + `RecentEventsModel::reload()` |

### Gating

- [ ] Tắt/bật edge → một event mỗi lần chuyển trạng thái

### Prompt giao agent

```
Task 19: Log system_event on logger online/offline transitions (edge-triggered, not every poll).
Optional station alarm flag change. Refresh RecentEventsModel after insert.
Uses EventRepository. Complements Task 11 UI.
```

---

# Nice-to-have (Tasks 20–24)

## Task 20 — Download latest report (FE-021)

**Làm sau:** Task 5  
**Contract:** [`contracts/rest-config-contract-v1.md`](../contracts/rest-config-contract-v1.md)

### Mục tiêu

Tải file báo cáo từ edge, save dialog.

### Deliverables

- `RestConfigService::downloadLatestReport(loggerId, savePath)`
- Signal `reportDownloaded`
- Button Logger Detail (token + online)

### Prompt giao agent

```
Task 20 (Nice): REST GET /api/v1/reports/latest — save to file via QFileDialog.
Extend RestConfigService. Button on Logger Detail. Bearer token. FE-021.
No impact on live sensor table or /readings debug rules.
```

---

## Task 21 — QR provision scan (FE-020)

> **Trạng thái: DROPPED.** Provision thủ công: Data Logger xuất text → user copy-paste vào form (Localsend/Zalo). Không ZBar/QZXing trong app.

**Làm sau:** Task 3  
**Contract:** [`contracts/provision-qr-v1.md`](../contracts/provision-qr-v1.md)

### Mục tiêu

Quét QR từ ảnh → điền form Add Logger.

### Deliverables

- `ProvisionQrDecoder` (QZXing hoặc ZBar)
- `LoggerFormDialog`: "Scan QR…" → file dialog → fill fields

### Prompt giao agent

```
Task 21 (Nice): QR provision from image file for LoggerFormDialog per docs/contracts/provision-qr-v1.md.
C++ decoder (QZXing or ZBar), no Python. Optional CMake flag. FE-020.
```

---

## Task 22 — Connect & Load Config probe (FE-022)

**Làm sau:** Task 5

### Mục tiêu

Probe `GET /config` trong form Add/Edit trước Save.

### Deliverables

- `RestConfigService::probeConfig(host, apiPort, token)`
- `humanizeProbeError` (C++)
- RAM only until Save

### Prompt giao agent

```
Task 22 (Nice): RestConfigService::probeConfig for LoggerFormDialog Connect & Load Config (FE-022).
One-shot GET /config. humanizeProbeError in C++. RAM only until user saves logger.
```

---

## Task 23 — Edit logger form đầy đủ (FE-006)

**Làm sau:** Task 22, Task 5, Task 3

### Mục tiêu

Edit: connection + API + device config patch khi đã probe/fetch.

### Deliverables

- `buildEditPatch` (C++)
- `updateLogger` + optional `applyConfig`
- `stationCode` immutable on edit

### Prompt giao agent

```
Task 23 (Nice): Full edit logger FE-006 — buildEditPatch in C++, split connection vs device config apply via RestConfigService.
LoggerFormDialog edit mode from Detail/Loggers. stationCode immutable when editing.
```

---

## Task 24 — Responsive layout + navigation rail (FE-023)

**Làm sau:** Task 18

### Mục tiêu

Detail responsive; shell nav = **navigation rail 80px cố định** (không sidebar collapse, không Drawer).

### Deliverables (trạng thái repo)

- `LoggerDetailView.qml`: `width > 950` → split ngang (bảng | chart + config).
- `AppNavigationRail.qml`: rail **80px**, icon trên / label dưới, logo `resources/icons/brand_4m_technologies_blue.svg`.
- `Main.qml`: `navigationRailWidth: 80`; không `sidebarOpen` / hamburger.
- **Đã thay thế:** `AppSidebar.qml` (256↔64 collapsible) — không còn trong `src/`.

### Prompt giao agent (lịch sử)

```
Task 24 (Nice): Responsive LoggerDetailView + fixed AppNavigationRail 80px (FE-023). QML only.
```

---

# Future — không lên task v1

| FE | Ghi chú |
|----|---------|
| FE-030 | Multi-user / roles |
| FE-031 | Export Excel/PDF |
| FE-032 | PostgreSQL |
| FE-033 | macOS |
| FE-034–035 | Cosmetic / export alerts |
