# Handoff — Central Logger

## Repo này là gì

Ứng dụng desktop **quản lý tập trung Data Logger** qua Modbus TCP (FC03 + FC02 + FC01) + REST config API.  
Stack: **Qt 6 + C++ + QML + CMake** — greenfield, **không** port source/tests Python.

## Source of truth (đọc theo thứ tự)

| Ưu tiên | File | Ghi chú |
|---------|------|---------|
| 1 | [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md) | Schema SQLite (5 bảng) + RAM / merge sensor |
| 2 | [`docs/contracts/`](docs/contracts/) | Hợp đồng edge **frozen v1** (ưu tiên hơn khảo sát cũ) |
| 3 | [`AGENTS.md`](AGENTS.md) | Quy tắc agent khi implement |

**Không** dùng tài liệu Phase 1 làm spec kỹ thuật chính thức — xem mục dưới.

**Repo mới làm từ đầu:** không có `docs/phase2/`, không tiếp nối “Phase 2/3/4” của dự án PySide6 cũ. Khảo sát cũ chỉ nằm trong `THAM_KHAO_REPO_CU/` để đọc tham khảo.

## Tham khảo repo cũ (`docs/THAM_KHAO_REPO_CU/`)

Toàn bộ deliverable **Phase 1** (khảo sát app PySide6 `central-logger-app`) nằm tại:

[`docs/THAM_KHAO_REPO_CU/phase1/`](docs/THAM_KHAO_REPO_CU/phase1/)

| File | Dùng khi nào |
|------|----------------|
| [`01-feature-matrix.md`](docs/THAM_KHAO_REPO_CU/phase1/01-feature-matrix.md) | Danh sách FE-001…FE-035 (khảo sát cũ) — **MVP repo = FE-001…FE-016**; FE-017 deferred |
| [`03-scope-document.md`](docs/THAM_KHAO_REPO_CU/phase1/03-scope-document.md) | In/out scope MVP, users, risks (một số risk đã cập nhật trỏ `docs/contracts/`) |
| [`05-python-cpp-migration-map.md`](docs/THAM_KHAO_REPO_CU/phase1/05-python-cpp-migration-map.md) | Gợi ý lớp C++ / layout `src/` — **chỉnh theo contracts + thiet_ke_db hiện tại** |
| [`06-qml-logic-inventory.md`](docs/THAM_KHAO_REPO_CU/phase1/06-qml-logic-inventory.md) | Method ViewModel cần có (từ `.js` app cũ) |
| [`02-pain-points.md`](docs/THAM_KHAO_REPO_CU/phase1/02-pain-points.md) | Bài học kiến trúc (GIL, REST merge, Canvas…) |
| [`04-non-functional-requirements.md`](docs/THAM_KHAO_REPO_CU/phase1/04-non-functional-requirements.md) | Baseline hiệu năng (vd. chart SQL ~2.3 ms @100k rows) |
| [`07-manual-test-with-hardware.md`](docs/THAM_KHAO_REPO_CU/phase1/07-manual-test-with-hardware.md) | Checklist test thiết bị thật |

**Quy tắc khi đọc tham khảo:**

- **Không** copy/port Python source hay pytest từ repo cũ.
- Nếu **mâu thuẫn** với [`docs/contracts/`](docs/contracts/) hoặc [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md) → **theo contracts + thiet_ke_db** (vd. DI/DO: **FC02/FC01**, không REST live `/readings`; analog: FC03 multi-PDU).
- FE-014 (app cũ: REST merge live) → C++ v1: live chỉ Modbus; `/readings` = **debug** so sánh với hardware (contract REST §D4–D7).

## Kiến trúc mục tiêu (v1)

- **MVVM:** Repository/Service (C++) + ViewModel (`Q_PROPERTY` / `Q_INVOKABLE`) + View (QML)
- **Không** logic nghiệp vụ trong QML `.js`
- **Module QML:** `CentralLogger.Core` — `qt_add_qml_module`
- **Modbus poll:** FC03 (header + analog) → FC02 (DI) → FC01 (DO) — [`modbus-map-v1.md`](docs/contracts/modbus-map-v1.md)
- **REST:** `GET/POST /config`, reports; `/readings` **chỉ debug** (1 lần bấm, xem JSON gốc — không feed UI live)
- **DB:** SQLite qua **`QSqlDatabase` / `QSQLITE`** (`Qt6::Sql`) — [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md), ADR [`docs/adr/0001-db.md`](docs/adr/0001-db.md)
- **Platform:** Linux + Windows

## Quyết định đã chốt

| Chủ đề | Quyết định | ADR |
|--------|------------|-----|
| Persistence SQLite | `QSqlDatabase` + driver `QSQLITE`, CMake `Qt6::Sql` | [`docs/adr/0001-db.md`](docs/adr/0001-db.md) |
| Modbus transport | `QModbusTcpClient` (`Qt6::SerialBus`) cho FC01/FC02/FC03 — không raw `QTcpSocket` | ADR #2 (Task 4) |
| Chart rendering | **Qt Graphs** (`Qt6::Graphs`, `import QtGraphs` in QML) — Qt Charts deprecated 6.11; `GraphsView` + `LineSeries`/`BarSeries` | ADR #3 (Task 13) |
| Window chrome | **Cửa sổ OS mặc định** — không frameless, không system tray (FE-017 / Task 18 **dropped** 2026-05) | — |

## Quyết định kỹ thuật (chưa chốt — ghi ADR khi cần)

1. Monolith `DashboardController` vs tách `LoggerDetailViewModel`?
2. Composition root / DI (`ApplicationContext`) — manual inject, không DI framework?

## Risks (transport — xem contract)

| Rủi ro | Contract | Giảm thiểu |
|--------|----------|------------|
| **Modbus FC3 > 125 registers** khi `Na ≥ 15` nếu gộp header + analog một PDU | [`modbus-map-v1.md`](docs/contracts/modbus-map-v1.md) §2 | Header 10 reg + chunk analog ≤ 120 reg/PDU; DI/DO qua FC02/FC01 |
| **Poll kéo dài / timeout** (FC03 + FC02 + FC01) | Cùng file §5 | Tổng PDU ≤ `timeout_s`; SerialBus FC 01/02/03 |
| **Firmware chưa expose FC02/FC01** | Cùng file §3–4 | `Ndi`/`Ndo` (HR5/HR6) khớp `edge_sensor_id` |
| **Catalog thiếu `sensor_type`** | [`rest-config-contract-v1.md`](docs/contracts/rest-config-contract-v1.md) | `GET /config` sau add logger |

## Trạng thái repo

- **Task 1 (data layer):** SQLite schema + repositories cho `logger_info` / `logger_sensor` / `sensor_reading` / `system_event` / `app_settings`; test `test_database_repositories`.
- **Task 2 (app shell):** Module `CentralLogger.Core` (`AppState`, `SettingsController`) + `CentralLogger.Components` (navigation rail, top bar, page header) + `CentralLogger.App` shell, Material light/dark theme.
- **Task 3 (logger CRUD):** `LoggerListModel` (`QAbstractListModel`) + `DashboardController` facade với add/update/remove/get + form dialog; test `test_dashboard_controller`.
- **Task 4 (Modbus poll + auto-create catalog):**
  - `src/network/` static lib với `ModbusMapParser` (header-only), `ModbusPollPlan::planPollReads`, `ModbusService` worker (`QThread` + `QModbusTcpClient`), `ModbusBridge` persist trên main thread.
  - `LoggerListModel` thêm roles `online` / `polling` / `anyAlarm` và `updateLoggerRow` patching từng hàng (không reset model mỗi chu kỳ).
  - `DashboardController::startModbusPolling` / `stopModbusPolling` / `onSnapshotApplied`; CRUD tự sync registry sang worker.
  - `main.cpp` dựng `QThread` ModbusWorker, queued connect `pollFinished` ↔ `applySnapshot` ↔ `snapshotApplied`, shutdown trong `aboutToQuit`.
  - Test mới `test_modbus_map_parser` (header, ABCD, FC02 unpack, chunking 14/15/16/30 + DI/DO).
- **Task 5 (REST config fetch/apply + readings debug):**
  - `src/network/rest/` thêm `RestConfigParser` (header-only: `parseConfigResponse`, `parseApplyResponse`, `formatRestError`, `prettyJson`) và `RestConfigService` (`QNetworkAccessManager`, `GET/POST /api/v1/config`, `GET /api/v1/readings` chỉ một-shot debug).
  - `LoggerDetailViewModel` (singleton-by-registry) trong `CentralLogger.Core`: `rawConfig` + `currentRevision` RAM, `lastRevision` từ DB, fetch → upsert `logger_sensor` + persist `last_revision`, apply dùng `effectiveConfigRevision`, debug readings hiện raw JSON.
  - `LoggerDetailView.qml`: toolbar Fetch / Apply / Debug + `BusyIndicator` + dialog scrollable readonly JSON, debug button disable khi `apiToken` rỗng.
  - `main.cpp` dựng `RestConfigService` trên main thread, `LoggerDetailViewModel::registerServices(...)` trước khi load QML.
  - Test mới `test_rest_config_parser` (sensor root vs nested, revision, apply, 401/409/422/404 error mapping, pretty JSON).
- **Task 6 (SensorMerger + SensorLiveRow — pure merge / merge thuần FE-009 backend):**
  - `src/core/sensors/`: `SensorLiveRow` (struct hàng hiển thị), `SensorMerger::buildRows(loggerId, PollSnapshot, catalog)` — **không** DB, **không** REST `/readings`, **không** QML.
  - Nguồn hàng: mọi `snap.analogs` + catalog `DI`/`DO` theo bit index ([`modbus-map-v1.md`](docs/contracts/modbus-map-v1.md) §3–4); catalog-only không có snapshot → bỏ qua (ưu tiên snapshot).
  - Giá trị: ANALOG float + `unit`; DI/DO `ON`/`OFF`; `displayStatus` từ Modbus flags + HR1 `rtuConnected` + attach-DI (`00`–`03`/custom → nhãn Monitoring/Error/…) + ngưỡng (`WAIT`, `OK`, `ALARM`, `ERR`, `STALE`). `alarmType` = `min`|`max`|`min+max` (badge phụ). Catalog: `parent_edge_sensor_id`, `di_type` từ `GET /config` (schema v2). **Attach-DI live** cần child DI trên FC02 — verify Task 4 hardware.
  - Test `test_sensor_merger` (catalog + analog + DI, snapshot-only, catalog-only omitted, threshold/stale, failed snapshot).
- **Task 7 (SensorMonitoringTableModel + snapshot cache / bảng live + cache RAM):**
  - `SensorSnapshotCache`: `QHash<loggerId, QVector<SensorLiveRow>>` — gọi `SensorMerger` khi poll OK; poll fail giữ bản cache cũ.
  - `SensorMonitoringTableModel` (`QAbstractTableModel`, 5 cột + roles `sensorId`, `name`, `value`, `unit`, `displayStatus`, …); `setRows` / `setLoggerId`.
  - `ModbusBridge::snapshotApplied` mang full `PollSnapshot` + `sensorCount` (thay signature cũ chỉ flags).
  - `DashboardController`: `sensorTable`, `sensorCache`; `onSnapshotApplied` → cache + refresh table khi `loggerId` khớp; signal `loggerSnapshotUpdated(loggerId)`.
  - Test `test_sensor_monitoring_table_model`.
- **Task 8 (LoggerDetailViewModel — expose sensor table / VM cho QML):**
  - `Q_PROPERTY(SensorMonitoringTableModel *sensorTable READ … CONSTANT)` — dùng chung instance từ `DashboardController`.
  - `registerServices(db, rest, appState, dashboard)`; connect `loggerSnapshotUpdated` → `onLoggerSnapshotUpdated` → `refreshSensorTableFromCache()`.
  - `setLoggerId`: hydrate bảng từ cache (poll có thể xảy ra trước khi mở detail).
  - `Q_PROPERTY` live state: `online`, `polling`, `anyAlarm`, `rtuConnected` (đọc từ `LoggerListModel`, `liveStateChanged`).
  - REST Task 5 giữ nguyên (`rawConfig`, fetch/apply/debug). Test `test_logger_detail_view_model` (+ live state, `rtuConnected` role).
- **Task 9 (Logger Detail UI — badges + sensor table / FE-009, FE-011):**
  - [`LoggerDetailView.qml`](src/app/qml/views/LoggerDetailView.qml): hàng chip `StatusChip` (Online, Polling, RTU, Alarm); cột Status bảng sensor qua `SensorStatusColumn`; theme `OperationalStatus` + `AttachDiType`; C++ `AttachDiTypeHelper`.
  - Giữ toolbar REST Task 5 + dialog readings debug + pane `rawConfig` (RAM, thu nhỏ phía dưới).
  - `LoggerListModel`: role `rtuConnected` (HR1 bit1); `updateLoggerRow(..., rtuConnected)` từ `snapshot.header.isRtuConnected()`.
  - Manual gợi ý: logger online → bảng đổi theo poll; Fetch config → cập nhật name/unit/type catalog (metadata), không merge `/readings` vào bảng live.
- **Task 10 (`RecentEventsModel` — top 20 `system_event` / FE-003 backend):**
  - `src/core/events/`: `RecentEventsModel` (`QAbstractListModel`, roles `id` / `loggerId` / `loggerName` / `eventType` / `message` / `level` / `createdAt`, `setLimit`).
  - `Data::EventRepository::listRecentWithLoggerName` — LEFT JOIN `logger_info` để model render station name không cần N+1.
  - Test `test_recent_events_model` (`:memory:`): order desc, role names, app-wide null logger, limit override.
- **Task 11 (Dashboard recent events list + nav / FE-003 UI):**
  - `DashboardController::recentEvents` (`Q_PROPERTY`, owned), `reloadRecentEvents()`; CRUD path (`afterMutation`) tự reload model.
  - `DashboardView.qml`: pane "Recent events" với `ListView` model = `DashboardController.recentEvents`, level accent (`critical|warning|info`), tap → `selectLogger(loggerId)` (chỉ khi không phải app-wide event).
- **Task 19 (Modbus-driven online/offline events / FE-003 bổ sung, FE-008):**
  - `DashboardController::onSnapshotApplied` edge-trigger ghi `system_event`: lưu `m_lastStatus` per logger, snapshot đầu tiên chỉ seed, các transition `online↔offline` ghi `Online` (level `info`) / `Offline` (level `warning`) với message kèm `stationCode`.
  - Sau insert: `m_recentEvents.reload()`; `removeLogger` dọn `m_lastStatus`.
  - Test bổ sung trong `test_recent_events_model`: seed-không-fire, edge-trigger transition, không spam ở các poll trùng trạng thái, CRUD reload.
- **Task 12 (ChartQueryService — SQL ingestion 24h / FE-002 data):**
  - `src/core/charts/ChartQueryService` — `ingestionLast24h(bucketMinutes)` truy vấn `sensor_reading` theo bucket 5 phút, trả `QVector<IngestionPoint>{label, count}`.
  - Test `test_chart_query_service` (`:memory:`): empty, single/multi bucket, ascending sort, custom bucket size, 24h filter, label format.
- **Task 13 (Dashboard ingestion chart / FE-002 UI):**
  - `DashboardController`: `Q_PROPERTY(QVariantList ingestionChartData)`, `refreshIngestionChart()` slot, `ingestionChartChanged` signal.
  - `DashboardView.qml`: `import QtGraphs`, `GraphsView` + `LineSeries` hiển thị ingestion data; empty state "No readings yet".
  - HANDOFF ADR #3: Qt Graphs thay Qt Charts (deprecated 6.11).
- **Task 16 (Data retention purge / FE-016):**
  - `DashboardController::purgeOldData()`: đọc `data_retention_days` từ `SettingsController`, gọi `SensorReadingRepository::purgeOlderThan`, refresh ingestion chart khi deleted > 0.
  - Triggers: app start (`main.cpp`), `SettingsController::saved` signal, `QTimer` 3600s.
  - Signal `retentionPurgeCompleted(int deletedCount)`.
  - Test `test_retention_purge`: purge old, keep recent, empty table, bulk, settings integration.
- **Task 14 (PollHistoryStore — RAM trending data / FE-010 data):**
  - `src/core/charts/PollHistoryStore` — in-memory ring buffer (max 24 points per analog `edgeSensorId` per logger), fed from `PollSnapshot` on successful polls.
  - `DashboardController`: `PollHistoryStore m_pollHistory`, `pollHistory()` accessor; `onSnapshotApplied` → `m_pollHistory.append(snapshot)`; `removeLogger` → `m_pollHistory.remove(id)`.
  - API: `seriesForLogger(loggerId)` → `QVariantList` of `{edgeSensorId, label, points: [{x, y, time}]}`.
  - Test `test_poll_history_store`: empty, single/multi snapshot, ring buffer cap 24, failed/invalid ignored, multi-logger, remove/clear, series format.
- **Task 15 (Logger Detail trending chart / FE-010 UI):**
  - `LoggerDetailViewModel`: `Q_PROPERTY(QVariantList trendingSeries)`, `refreshTrendingSeries()` called on `setLoggerId` and `onLoggerSnapshotUpdated`.
  - `LoggerDetailView.qml`: `import QtGraphs`, `GraphsView` + dynamic `LineSeries` per analog sensor below sensor table; multi-color palette; auto Y-axis range; empty state label.
- **Task 17 (Logger search filter / FE-004):**
  - `LoggerSearchProxyModel` (`QSortFilterProxyModel`): substring filter on `stationCode`, `name`, `host` — case-insensitive.
  - `LoggersView.qml`: `TextField` search bar, `model: searchProxy` thay vì trực tiếp `DashboardController.loggers`; empty state hiện "No loggers match…" khi filter không match.
- **Task 22 / 23 (Config chỉ Add/Edit form):**
  - `LoggerFormDialog`: **Connect & Load Config** (Add luôn hiện; Edit hiện khi chưa load config). Edit **auto** `loadConfigForForm` khi mở.
  - Field REST + Central poll: một ô **Poll interval (s)** từ GET `poll_interval`; Save → POST device + `central_poll_interval_s` (Modbus). Field DB-only: host, modbus port, token, note, …
  - **Save** → `saveLoggerFromForm`: transaction DB + POST patch (`buildEditPatch`); fail bất kỳ bước → rollback, dialog mở. Save chặn nếu chưa Connect/load config.
  - `DashboardController`: RAM probe/fetch chung (`m_probedConfigObject`, revision, sensors).
  - **Logger Detail**: không Fetch/Apply config (chỉ debug readings + download report).
- **Task 20 (Download latest report / FE-021):**
  - `RestConfigService::downloadLatestReport(loggerId, savePath)` — `GET /api/v1/reports/latest`, Bearer auth, binary/text response written to user-chosen file.
  - Signal `reportDownloaded(loggerId, ok, savePath, errorMessage)`. Guard `m_reportInFlight` chặn request chồng.
  - `LoggerDetailViewModel::downloadReport(savePath)` — `Q_INVOKABLE`, `reportBusy` property, pre-validates token + state.
  - `LoggerDetailView.qml`: nút **Download report** (enabled khi `hasApiToken && online`), `FileDialog` (Save mode, `.txt`), success label hiện đường dẫn file đã lưu.
  - Test `test_rest_config_service_report`: no DB, empty path, logger not found, no token, duplicate in-flight.
- **Task 24 (Responsive detail / FE-023):**
  - `LoggerDetailView.qml`: Responsive `GridLayout` — hai cột (bảng sensor | chart + config) khi `width > 950px`, xếp dọc khi hẹp hơn.
  - **Shell nav (cập nhật sau Task 24):** `AppNavigationRail.qml` — rail **80px cố định** (icon trên, label dưới), logo `resources/icons/brand_4m_technologies_blue.svg`, không hamburger / không collapse. Thay `AppSidebar` (256↔64). Không Drawer.
- [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md) — thiết kế DB
- [`docs/contracts/`](docs/contracts/) — hợp đồng edge v1 (Modbus FC01/02/03)
- [`docs/THAM_KHAO_REPO_CU/phase1/`](docs/THAM_KHAO_REPO_CU/phase1/) — khảo sát app cũ (tham khảo)

### Manual slice Task 4 (chưa chạy — chờ edge / simulator)

Theo checklist [`task_4_modbus_poll`](#) §8 + [`07-manual-test-with-hardware.md`](docs/THAM_KHAO_REPO_CU/phase1/07-manual-test-with-hardware.md): add logger với host/port khớp firmware v1, quan sát `status=online`, `sensor_reading` có float + 0/1 cho DI/DO, tắt edge → `offline`. Build/`ctest` không phụ thuộc bước này.

## Prompt cho Cursor Agent

File raw text (copy đủ): [`docs/plan/agent-prompt.txt`](docs/plan/agent-prompt.txt)

Thứ tự implement gợi ý: **Task 1** DB → **2** shell QML → **3** logger CRUD → **4** Modbus → **5** REST → **6–9** sensor live (merge → model → VM → QML).

**MVP Core — hoàn tất:** Tasks **1–17**, **19** (FE-001…FE-016 trừ FE-014; FE-004 = Task 17). **Task 18** (FE-017) dropped — cửa sổ OS chuẩn.

**Nice-to-have đã xong:** Tasks **20**, **22**, **23**, **24** (download report FE-021; probe trước Save FE-022; edit form + `stationCode` immutable + sensor catalog upsert & applyConfig on save FE-006; responsive detail FE-023 + navigation rail 80px).

**Done / Đã xong (tóm tắt):** MVP ở trên + probe config (FE-022) + edit form complete (FE-006) + download report (FE-021) + responsive UI (FE-023).

- **Task 20 (Download latest report / FE-021):**
  - `RestConfigService::downloadLatestReport` — `GET /api/v1/reports/latest`, Bearer, ghi bytes ra file (không parse JSON).
  - `m_reportInFlight` guard; signal `reportDownloaded(loggerId, ok, savePath, errorMessage)`.
  - `LoggerDetailViewModel::downloadReport` + `reportBusy`; `LoggerDetailView.qml` — FileDialog Save, nút bật khi `hasApiToken && online`.
  - Test `test_rest_config_service_report` (5 cases). Không đụng live table / `/readings` poll.
- **Task 24 (Responsive layout / FE-023):**
  - `LoggerDetailView.qml` responsive: splits side-by-side (left: sensor table, right: chart + config) when width > 950px, stacks vertically when narrower.
  - `AppNavigationRail.qml`: fixed **80px** Material-style rail (`NavItem` icon + label below); `Main.qml` `navigationRailWidth: 80`; no `sidebarOpen` / hamburger.

**Out of scope (cố ý):**

| Mục | Lý do |
|-----|--------|
| FE-014 | Live chỉ Modbus; `GET /readings` debug one-shot |
| FE-017 / Task 18 | Close thoát app; không tray/frameless |
| FE-020 / Task 21 | Không decode QR trong app — copy-paste text provision |

**CI (2026-05):** `.github/workflows/ci.yml`, `dev-build.yml`, `build-release.yml` trên `main`. Linux: CPack DEB (`cmake/CPackOptions.cmake`, `packaging/linux/`). Windows: QTIFW (`packaging/windows/`). Nhánh `dev` giữ cho feature; workflow không trigger trên `dev` trừ khi mở rộng sau.

**Next / Tiếp theo:** Manual Task 4 (edge/simulator) khi có hardware.

**UI modernization (done 2026-05):** [`docs/plan/ui-modernization-agent-prompts.md`](docs/plan/ui-modernization-agent-prompts.md) — **UI-M1** `Loader` navigation; **UI-M2** sensor `TableView` + `HorizontalHeaderView`; **UI-M3/M3b** loggers `QAbstractTableModel` (7 cột) + `headerData()`/`tr()` (không header `Pane` thủ công). Cột Status: `DisplayRole` + QML `display` = Online/Offline (C++ `tr()`).

**M3 UI Migration (done 2026-05, pane parity 2026-05):** Material Design 3 on Qt Quick Controls Material — Teal primary + Indigo accent, no dynamic color.
- **Theme:** `AppColors`, `AppTypography`, `AppTheme` layout + shape tokens (`cardRadius`, `chipRadius`, `listItemRadius`).
- **Components:** `ElevatedPane`, `SectionHeader`, `IconToolButton`, `StatCard`, `TableHeaderCell`, `TableCellBackground`, `EmptyStatePlaceholder`, rail `NavItem` with `accentContainer` / `onAccentContainer`.
- **Views:** All data views use `ElevatedPane` (Dashboard, Settings, Loggers, Logger Detail); Loggers search `Outlined`; section headers + typography tokens unified.
- **Charts:** `ChartGraphsTheme` — plot + view background `surfaceContainerLow`; live on rail theme toggle.
- **Docs:** [`docs/ui/material3-component-guidelines.md`](docs/ui/material3-component-guidelines.md) (includes manual QA checklist), walkthrough [`docs/report/2026-05-27_m3_ui_migration_walkthrough.md`](docs/report/2026-05-27_m3_ui_migration_walkthrough.md).
- Navigation: `Loader` + `currentView` (no `StackView`).

**Shell PR2 + navigation rail:** `SplitView` + `ToolBar` top bar; rail = [`AppNavigationRail.qml`](src/components/AppNavigationRail.qml) (thay collapsible `AppSidebar`). `Qt6::Svg` cho logo brand.

**Dropped / bỏ qua:**

| Task | FE | Lý do |
|------|-----|--------|
| 18 | FE-017 | Tray + frameless — cửa sổ OS chuẩn |
| 21 | FE-020 | QR scan trong app — provision thủ công: Data Logger xuất text → copy-paste vào form (không import ảnh QR) |