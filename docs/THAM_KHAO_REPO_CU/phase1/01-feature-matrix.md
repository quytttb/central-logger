# Feature Matrix — Central Logger App

Song ngữ: mô tả Tiếng Việt, cột kỹ thuật Tiếng Anh.

**Cột:**
- `ID` — mã tham chiếu
- `Feature_VI / Feature_EN` — tên chức năng
- `Priority` — Core / Nice / Future
- `UI_Surface` — QML View + component
- `Backend_Entry` — Slot/Signal/Property Python
- `Data_Tables` — bảng SQLite
- `External_Deps` — phụ thuộc ngoài (Modbus, REST, ZBar…)
- `Primary_Flow` — bước user chính
- `Pain_Point` — vấn đề hiện tại (ref → 02-pain-points.md)
- `Notes_for_Cpp` — gợi ý layer C++ (Model / Service / ViewModel / View)

---

## Core — MVP phải có

### FE-001 — Fleet statistics / Thống kê fleet tổng quan

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `DashboardView.qml` → `StatCard` ×3 |
| Backend_Entry | `AppState` singleton (`@QmlSingleton`): `totalLoggers`, `onlineLoggers`, `alarmCount`, `statusText` |
| Data_Tables | `logger_info` (derived in-memory) |
| External_Deps | — |
| Primary_Flow | 1. App start → `DashboardController.start()` → `_sync_header_stats()` → `appStatsChanged` signal → `AppState` properties update. 2. Dashboard renders 3 stat cards. |
| Pain_Point | — |
| Notes_for_Cpp | `AppState` C++ `QObject` singleton (`QML_SINGLETON`); update from `ModbusBridge` thread via `QMetaObject::invokeMethod` (Qt::QueuedConnection) |

---

### FE-002 — Ingestion / network traffic chart / Biểu đồ lưu lượng thu thập 24h

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `DashboardView.qml` → `NetworkTrafficChart.qml` → `BaseChart.qml` (Canvas paint) |
| Backend_Entry | `DashboardController.ingestionChartJson` (Q_PROPERTY), `ingestionChartJsonChanged` signal |
| Data_Tables | `sensor_reading` (GROUP BY 5-min buckets, last 24h) |
| External_Deps | — |
| Primary_Flow | 1. `ingestionChartJson` accessed → lazy build via `chart_queries.build_ingestion_chart_24h(bucket_minutes=5)`. 2. Chart component reads JSON series + labels. 3. Invalidated on new Modbus snapshot or purge. |
| Pain_Point | P-01 (Python SQL on main thread if chart property read from QML paint); P-06 (Canvas custom paint is verbose) |
| Notes_for_Cpp | `ChartService` in C++ calls SQLite with `QSqlQuery`; expose `Q_PROPERTY QString ingestionChartJson`; or emit `QAbstractSeries` for QtCharts alternative (Phase 2 ADR decision) |

---

### FE-003 — Recent events list + drill-down / Danh sách sự kiện gần đây

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `DashboardView.qml` → `RecentEventsList.qml`; click → `selectedLoggerId` → `logger-detail` view |
| Backend_Entry | `RecentEventsModel` (`QAbstractListModel`); `DashboardController.eventsChanged` signal; `EventJournal.log_event / log_event_dedup` |
| Data_Tables | `system_event` (last 20, ordered `created_at DESC`) |
| External_Deps | — |
| Primary_Flow | 1. Modbus snapshot → `EventJournal.log_event(logger_id, name, type, msg, level)` → writes `SystemEvent` row → `eventsChanged.emit()`. 2. `RecentEventsModel` re-fetches top 20. 3. User clicks event → navigate to logger detail. |
| Pain_Point | P-07 (dedup logic in Python; may lose events under high frequency) |
| Notes_for_Cpp | `RecentEventsModel` → `QAbstractListModel` C++; roles: id, loggerName, eventType, message, level, createdAt; `EventJournal` → `EventService` C++ class |

---

### FE-004 — Logger list + search filter / Danh sách logger + tìm kiếm

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggersView.qml` → `LoggerTableRow.qml`, `ListHeaderCell.qml`; top bar search → auto-navigate |
| Backend_Entry | `LoggerListModel` (`QAbstractListModel`); roles: name, host, port, unitId, online, polling, rtuConnected, anyAlarm, sensorCount, lastUpdate, lastError |
| Data_Tables | `logger_info` (initial load); runtime state in-memory |
| External_Deps | — |
| Primary_Flow | 1. `LoggersView` binds `loggersModel` filtered by `searchQuery`. 2. Rows show badges per status bit. 3. Empty state → "Add Logger" prompt. |
| Pain_Point | — |
| Notes_for_Cpp | `LoggerListModel` → `QAbstractListModel` C++; add `QSortFilterProxyModel` for search |

---

### FE-005 — Add logger / Thêm logger mới

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggersView.qml` FAB → `LoggerFormDialog` (`mode: "add"`) |
| Backend_Entry | `DashboardController.addLogger(name, host, port, unit_id, poll_interval_s, api_port, api_token, enabled, timeout_s, note, api_base_url)` |
| Data_Tables | `logger_info` (INSERT) |
| External_Deps | ZBar/pyzbar optional (QR scan in dialog) |
| Primary_Flow | 1. Open dialog → fill fields (or Scan QR). 2. Save → `addLogger` slot → `logger_ops.insert_logger` → DB row. 3. If enabled → `ModbusBridge.add_runtime_logger`. 4. `EventJournal.log_event` "Logger added". 5. `appStatsChanged`. |
| Pain_Point | P-02 (ZBar native dep; dialog logic partly in `LoggerFormLogic.js`) |
| Notes_for_Cpp | `loggerOps.insertLogger()` C++ method on `LoggerRepository`; form validation in ViewModel |

---

### FE-006 — Edit logger / Chỉnh sửa logger

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailHeader.qml` edit button → `LoggerFormDialog` (`mode: "edit"`) |
| Backend_Entry | `DashboardController.updateLoggerConnection(...)` + `updateLoggerApi(...)` + `applyConfig(logger_id, expected_revision, config_json)` |
| Data_Tables | `logger_info` (UPDATE); `sensor_reading` (indirect, revision tracking) |
| External_Deps | REST `POST /api/v1/config` (device config patch, only if probe loaded) |
| Primary_Flow | 1. Detail header → Edit. 2. Dialog pre-fills from `getLoggerFormData(id)`. 3. Optional: Connect & Load Config (probe) → device fields unlock. 4. Save → `buildEditPatch` (JS) → separate slots for connection / cloud / device config. 5. Revision conflict → user message. |
| Pain_Point | P-03 (patch logic split between `LoggerFormLogic.js` + 3 Python slots); P-05 (revision conflict UX complex) |
| Notes_for_Cpp | `LoggerFormLogic.js` → split into `LoggerFormViewModel` C++ (`buildEditPatch`, `parseProbeSuccess`); keep REST error strings human-readable |

---

### FE-007 — Delete logger / Xóa logger

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailHeader.qml` → `ConfirmDialog` |
| Backend_Entry | `DashboardController.removeLogger(logger_id)` |
| Data_Tables | `logger_info` (DELETE CASCADE → `sensor_reading`, `system_event` FK) |
| External_Deps | — |
| Primary_Flow | 1. Delete icon → `ConfirmDialog`. 2. Confirm → `removeLogger(id)` → stop Modbus task, pop REST cache, delete DB, remove from model, clear sensors. 3. Navigate back to Loggers view. |
| Pain_Point | — |
| Notes_for_Cpp | `LoggerRepository.deleteLogger(id)` + `ModbusService.stopLogger(id)` |

---

### FE-008 — Modbus TCP polling / Thu thập dữ liệu Modbus

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | Status badges in `LoggersView` + `LoggerOverviewGrid.qml` |
| Backend_Entry | `ModbusManager` + `LoggerModbusClient` (asyncio thread); `ModbusBridge.apply_snapshot_ui` → `LoggerListModel` update + `SensorState` merge + DB write |
| Data_Tables | `sensor_reading` (INSERT per snapshot), `system_event` |
| External_Deps | Modbus TCP port 5020 (default); `pymodbus.AsyncModbusTcpClient` |
| Primary_Flow | 1. Per-logger asyncio task polls every `poll_interval_s`. 2. Read N×8 holding registers → `modbus_map.parse_header` + `parse_sensor_array`. 3. `ReadOutcome` → emit `_snapshotForUi` (queued). 4. Main thread: DB insert, model update, `sensorsUpdated` signal. |
| Pain_Point | P-01 (Python GIL + asyncio complexity); P-08 (poll_interval drift under load) |
| Notes_for_Cpp | `ModbusService` C++ (QModbusTcpClient or raw socket + `QThread`); `ModbusMapParser` header-only library |

---

### FE-009 — Sensor monitoring table / Bảng theo dõi sensor live

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailView.qml` → `SensorMonitoringTable.qml` |
| Backend_Entry | `SensorMonitoringTableModel` (`QAbstractTableModel`); `DashboardController.sensorsUpdated` signal; 5 columns: SensorId, Name, Value, Unit, DisplayStatus |
| Data_Tables | `sensor_reading` (latest per sensor_id via in-memory cache) |
| External_Deps | REST `/readings` (DI/DO catalog merge) |
| Primary_Flow | 1. `sensorsUpdated(loggerId, jsonStr)` → `SensorMonitoringTableModel.updateFromJson(payload)`. 2. Table renders value (digital: ON/OFF; analog: float + unit). 3. `SensorStatusBadge` renders display_status. |
| Pain_Point | P-04 (Python↔QML data crossing on every poll; `updateFromJson` parses JSON each time) |
| Notes_for_Cpp | `SensorMonitoringTableModel` C++ `QAbstractTableModel`; update via `beginResetModel` / `endResetModel` or `dataChanged` per row |

---

### FE-010 — Sensor trending chart / Biểu đồ xu hướng sensor

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailView.qml` → `SensorTrendingChart.qml` → `BaseChart.qml` (Canvas paint) |
| Backend_Entry | `DashboardController.pollTrendingChartJson(loggerId)` slot; `pollTrendingChartJsonChanged(int)` signal; `chart_queries.build_sensor_trending_poll_chart` |
| Data_Tables | In-memory `SensorState.poll_history[logger_id]` (deque, max 24 points) |
| External_Deps | — |
| Primary_Flow | 1. Every Modbus poll → `SensorState.poll_history` append. 2. `pollTrendingChartJsonChanged` → chart re-reads `pollTrendingChartJson(id)`. 3. Analog sensors only; colors from `SensorPalette`. |
| Pain_Point | P-06 (Canvas custom paint; multi-series palette management verbose) |
| Notes_for_Cpp | `SensorTrendingService` C++ builds JSON series; or use `QtCharts::QLineSeries` as model (Phase 2 ADR) |

---

### FE-011 — Logger overview badges / Trạng thái tổng quan logger

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailView.qml` → `LoggerOverviewGrid.qml`; badges: poll / RTU / alarm / online |
| Backend_Entry | `LoggerListModel` roles: `onlineRole`, `pollingRole`, `rtuConnectedRole`, `anyAlarmRole`; `snapshotApplied(loggerId, online, timestamp)` |
| Data_Tables | In-memory `LoggerItem` state |
| External_Deps | — |
| Primary_Flow | 1. Modbus snapshot → `LoggerListModel.update_from_snapshot` → `dataChanged` signal. 2. Detail view binds `row.online`, `row.polling`, etc. |
| Pain_Point | — |
| Notes_for_Cpp | Part of `LoggerListModel` C++ roles; `snapshotApplied` → `Q_SIGNAL void snapshotApplied(int loggerId, bool online, QString ts)` |

---

### FE-012 — REST fetch config / Tải cấu hình từ thiết bị qua REST

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerDetailView.qml` → fetch triggered on detail open or manually |
| Backend_Entry | `DashboardController.fetchConfig(loggerId)` → `RestCoordinator.schedule_rest(id, "get_config")` → `configFetched(loggerId, ok, payloadJson)` |
| Data_Tables | `logger_info` (`last_revision` update) |
| External_Deps | REST `GET /api/v1/config` (Bearer token); httpx async |
| Primary_Flow | 1. Trigger `fetchConfig(id)`. 2. Async task → httpx GET. 3. `configFetched` → `LoggerDetailLogic.mergeConfigFetched(detail, payloadJson)` → merge `configForm`, `cloudForm`, `currentRevision`. |
| Pain_Point | P-05 (revision conflict handling in JS; complex merge logic) |
| Notes_for_Cpp | `RestConfigService::fetchConfig(int id)` → `QNetworkAccessManager`; result via signal to ViewModel |

---

### FE-013 — REST apply config / Áp dụng cấu hình lên thiết bị

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `LoggerFormDialog` edit mode → save device config patch |
| Backend_Entry | `DashboardController.applyConfig(loggerId, expectedRevision, configJson)` → `RestCoordinator.schedule_rest(id, "apply_config")` → `configApplied(loggerId, ok, payloadJson)` |
| Data_Tables | `logger_info` (`last_revision` update on success) |
| External_Deps | REST `POST /api/v1/config` with `{"expected_revision": N, "config": {...}}` |
| Primary_Flow | 1. `buildEditPatch` (JS) → `configPatch` diff. 2. `applyConfig(id, revision, jsonStr)`. 3. `configApplied` → merge `currentRevision`. 4. On 409 conflict → "Connect again, then save" message. |
| Pain_Point | P-05 (revision conflict UX); P-03 (patch diff logic in JS) |
| Notes_for_Cpp | `RestConfigService::applyConfig(id, expectedRevision, QJsonObject patch)` |

---

### FE-014 — REST readings merge / Đọc số liệu live từ REST + merge với Modbus

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `SensorMonitoringTable` (DI/DO sensors appear via REST only) |
| Backend_Entry | `RestCoordinator._request_readings_if_needed` → `GET /readings`; `sensor_catalog.extract_sensors_from_readings_raw`; `SensorState.cache_sensors` |
| Data_Tables | In-memory merge (not persisted) |
| External_Deps | REST `GET /api/v1/readings` (Bearer); analog merged with Modbus; DI/DO from REST only |
| Primary_Flow | 1. Modbus snapshot → `SensorState` triggers REST readings if catalog has DI/DO. 2. REST response → `extract_sensors_from_readings_raw` → merge with last Modbus snapshot. 3. `sensorsUpdated` emitted. |
| Pain_Point | P-07 (merge logic in `rest_coordinator` + `sensor_catalog` is intricate; REST errors surface in table header note) |
| Notes_for_Cpp | `RestReadingsService` + `SensorMerger` C++ |

---

### FE-015 — App settings / Cài đặt ứng dụng

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `SettingsView.qml` |
| Backend_Entry | `SettingsController` (`@QmlElement`): `theme`, `systemTimezone`, `dataRetentionDays`, `maintenanceMode`, `save()` slot |
| Data_Tables | `app_settings` (row id=1) |
| External_Deps | — |
| Primary_Flow | 1. SettingsView loads current values on appear. 2. User changes fields. 3. Save → `SettingsController.save()` → UPDATE `app_settings`. 4. Theme propagates to root `window.isDark`. 5. Retention days used by `purgeOldData`. |
| Pain_Point | — |
| Notes_for_Cpp | `SettingsController` C++ `QObject`; `AppSettings` as simple struct; `QSettings` or SQLite row id=1 |

---

### FE-016 — Data retention purge / Xóa dữ liệu cũ theo ngày lưu trữ

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | Settings save triggers purge; also on app start and every 1h timer |
| Backend_Entry | `DashboardController.purgeOldData()` → `db.retention.purge_old_data()`; `_retention_timer` (QTimer 3600s) |
| Data_Tables | `sensor_reading` DELETE WHERE `recorded_at < cutoff` |
| External_Deps | — |
| Primary_Flow | 1. `purgeOldData()` reads `app_settings.data_retention_days`. 2. DELETE `sensor_reading` older than cutoff. 3. If deleted > 0 → invalidate ingestion chart. |
| Pain_Point | — |
| Notes_for_Cpp | `RetentionService::purge()` C++ on `QThread`; `QTimer` in `DashboardController` C++ |

---

### FE-017 — System tray + frameless window chrome / Khay hệ thống + cửa sổ không viền

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Core** |
| UI_Surface | `AppTopBar.qml` (minimize, close); `FrameResizeHandles.qml`; `system_tray.py` → `TrayCtl` context prop |
| Backend_Entry | `TrayCtl` (context property, not `@QmlElement`): `quitApp()` slot; `ApplicationWindow.flags: Qt.FramelessWindowHint` |
| Data_Tables | — |
| External_Deps | OS system tray (Linux: libappindicator) |
| Primary_Flow | 1. Close button → tray (hide window). 2. Tray icon → restore or quit. 3. Resize handles drag → `startSystemResize`. |
| Pain_Point | — |
| Notes_for_Cpp | `QSystemTrayIcon` C++; `Qt::FramelessWindowHint` on `QQuickWindow`; resize via `QWindow::startSystemResize` |

---

## Nice-to-have

### FE-020 — QR provision scan / Quét mã QR để ghép đôi thiết bị

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Nice** |
| UI_Surface | `LoggerFormDialog` → "Scan QR…" button (hidden if unavailable) |
| Backend_Entry | `DashboardController.importProvisionFromQrImageWithDialog()` → `QFileDialog.getOpenFileName` → `qr_provision.import_provision_from_qr_image(path)` → `pyzbar.decode` |
| Data_Tables | — |
| External_Deps | `libzbar0` (Linux) or ZBar DLLs (Windows native bundle); `pyzbar`, `Pillow` |
| Primary_Flow | 1. Scan QR → file picker. 2. Decode PNG/JPG → JSON schema `central-logger-provision/v1`. 3. Fill host, token, ports, station fields. |
| Pain_Point | P-02 (ZBar DLL Windows; pyzbar Python binding; native bundle complexity) |
| Notes_for_Cpp | Replace with `QZXing` or `ZBar` Qt wrapper; no Python binding needed |

---

### FE-021 — Download latest report / Tải báo cáo mới nhất từ thiết bị

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Nice** |
| UI_Surface | `LoggerDetailHeader.qml` download icon (online + token required) |
| Backend_Entry | `DashboardController.downloadLatestReportWithDialog(loggerId)` → `QFileDialog.getSaveFileName` → `RestCoordinator.schedule_rest(id, "download_report", save_path=...)` → `reportDownloaded(id, ok, msg)` |
| Data_Tables | — |
| External_Deps | REST `GET /api/v1/reports/latest` (Bearer); writes binary to local path |
| Primary_Flow | 1. Click download. 2. Save dialog. 3. httpx GET → stream to file. 4. `reportDownloaded` signal → snackbar. |
| Pain_Point | — |
| Notes_for_Cpp | `QNetworkReply` save-to-file pattern |

---

### FE-022 — Connect & Load Config probe / Kết nối và tải cấu hình thiết bị trước khi lưu

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Nice** |
| UI_Surface | `LoggerFormDialog` → "Connect & Load Config" button |
| Backend_Entry | `DashboardController.probeEdgeConfig(host, api_port, api_token, api_base_url)` → `edgeConfigProbed(ok, payloadJson)` |
| Data_Tables | — |
| External_Deps | REST `GET /api/v1/config` (one-shot, not persisted until Save) |
| Primary_Flow | 1. User enters host + token. 2. Probe → `_probe_edge_async`. 3. Success → `LoggerFormLogic.parseProbeSuccess` → device fields unlock. 4. Error → `humanizeProbeError` message. |
| Pain_Point | P-03 (error humanization in JS; must run after Modbus loop started) |
| Notes_for_Cpp | `RestConfigService::probe(...)` one-shot, result to ViewModel |

---

### FE-023 — Responsive layout + collapsible sidebar / Bố cục responsive + thanh bên thu gọn

| Thuộc tính | Giá trị |
|------------|---------|
| Priority | **Nice** |
| UI_Surface | `AppSidebar.qml` (256px ↔ 64px); `LoggerDetailView` splits at `width > 950px` |
| Backend_Entry | `window.sidebarCollapsed` QML state |
| Data_Tables | — |
| External_Deps | — |
| Primary_Flow | Toggle sidebar → animate width. Wide screen → detail view shows panels side-by-side. |
| Pain_Point | — |
| Notes_for_Cpp | Pure QML; keep same logic |

---

## Future / out of MVP

| ID | Feature_EN | Feature_VI | Lý do defer |
|----|------------|------------|------------|
| FE-030 | Multi-user / roles | Đa người dùng / phân quyền | Hiện tại single-user desktop; cần auth layer riêng |
| FE-031 | Export Excel/PDF | Xuất Excel/PDF | Chưa có yêu cầu format cụ thể |
| FE-032 | PostgreSQL backend | Dùng PostgreSQL thay SQLite | SQLite đủ cho single-machine; migration nếu multi-site |
| FE-033 | macOS target | Hỗ trợ macOS | Linux + Windows đủ cho v1 |
| FE-034 | Dark mode per-logger color | Màu riêng mỗi logger | Nice-to-have cosmetic |
| FE-035 | Offline alert export | Xuất cảnh báo offline | Future reporting feature |
