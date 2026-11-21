# Walkthrough — Loggers Data | Graph refactor

> Trạng thái: **đã hoàn tất mã & kiểm thử local Phase 0–6** (archive 2026-11-20). TT10 đã commit riêng;
> kit `2080c2b` đã checkout trong hai app.
> Plan: [`../plan/loggers-data-graph-refactor.md`](../plan/loggers-data-graph-refactor.md) ·
> TODO: [`../plan/loggers-data-graph-todo.md`](../plan/loggers-data-graph-todo.md)

Mỗi phase ghi: ngày, commit, file đổi, lệnh build/test + kết quả, sai lệch so
với plan và lý do.

---

## Phase 0 — Chuẩn bị

**2026-11-18 — Đã duyệt plan.** `config.sensors[]` là patch ngưỡng, giữ API
v1. Giữ “Show graph”. Persist Cards/List trong schema v8. Khi offline chỉ
xem ngưỡng; không lưu nháp vào DB hay gửi lại tự động.

- Theo yêu cầu user, đã commit riêng TT10/Modbus map v2: Central `f10e2d0`,
  Data Logger `5d91023`. Các thay đổi quản lý tiến trình OSG/đóng gói Windows,
  dọn docs và hai rename docs đã stage ở Central được giữ ngoài hai commit.
- Trước khi tách commit, build Debug với Qt 6.11.1 tại
  `build/loggers-phase0` trong từng repo, chạy
  `ctest --test-dir build/loggers-phase0 --output-on-failure` với
  `QT_QPA_PLATFORM=offscreen`.
- **Central:** build thành công, 25/25 mục CTest pass.
- **Data Logger:** build thành công, CTest trả 30/30; riêng
  `integration_modbus_test` tự skip vì không tìm thấy `modbus_rtu_simulator.py`.
  Chưa chạy phần cứng.

## Phase 1 — Kit dùng chung

Chuẩn bị riêng tại worktree `../logger-ui-kit-loggers-refactor`, base
`24ef015`: chuyển `ThemedTabButton`, `IoColors` và tách `SensorValueCard`
từ Monitor. Cập nhật `README.md` của kit: dữ liệu, trạng thái và hành động
được app truyền vào. Kit phân biệt DI/DO không có dữ liệu với OFF.

- `cmake --build build/loggers-kit --parallel 2`: pass.
- `ctest --test-dir build/loggers-kit --output-on-failure`: pass, gồm chọn
  tab, accessible label, hiển thị analog/ngưỡng/alarm, DI/DO chưa biết và
  cỡ chữ kiosk/desktop. Chạy lại với style Material cũng pass.
- `cmake --build build/loggers-kit --target qmllint`: pass.
- Kit đã commit local `2080c2b` trên nhánh `refactor/loggers-data-graph`.
  Đã fetch local và checkout commit đó trong `shared/logger-ui-kit` của hai
  app. Các gitlink app chưa commit. Không push; bản clone khác chưa thấy
  commit kit trừ khi lấy từ repo local.
- Data Logger đã chuyển Monitor sang `SensorValueCard`, giữ callback
  Hiệu chuẩn ở app và bỏ bản local của `IoColors`, `ThemedTabButton` cùng
  module `DataLogger.Theme` đã rỗng. Thay đổi app chưa commit.
- Central với kit `2080c2b`: build thành công, **25/25** mục CTest pass.
- Data Logger với kit `2080c2b`: build và `qmllint` thành công, CTest báo
  **30/30**; `integration_modbus_test` tự skip vì thiếu simulator RTU.
  Log trong `build/loggers-phase0`: `phase1-build.log`, `phase1-qmllint.log`
  và `Testing/Temporary/LastTest.log`. Chưa so ảnh kiosk 1024×600.

## Phase 2 — Contract ngưỡng

**2026-11-18 — Hoàn tất Phase 2 (chưa commit theo yêu cầu).**

- **Data Logger (`../data-logger`):**
  - `SensorDao::updateThresholds`: chỉ cập nhật `min_threshold`/`max_threshold` trên hàng `ANALOG` đang `active=1`.
  - `RestConfigUpdater::buildConfigSnapshot` + `RestConfigUpdater::apply`:
    - `GET /api/v1/config` đọc `app_config` và `sensor` trong cùng một snapshot transaction, trả `thresholds_writable: true`, `config` object + các trường phẳng cũ, `active`, và `min_threshold`/`max_threshold` (`null` khi chưa đặt) cho cảm biến `ANALOG`.
    - `POST /api/v1/config` xử lý envelope v1 (`api_version: 1`, `request_id`, `expected_revision`, `config`) với kiểm tra `expected_revision` (lệch → 409), patch `config.sensors[]` theo R1–R5 (sai ID / inactive / DI-DO / key lạ / trùng ID / `min >= max` sau ghép → 422 + rollback), tăng `config_revision` (+1) và ghi `audit_log` (`sensor.remote_thresholds` whitelist `{sensor_id, from, to}`) trong cùng transaction. Giữ nguyên tương thích đường POST phẳng cũ cho app-config (tuyệt đối không ghi ngưỡng).
  - `RestApiService`: trả `applied_revision` (cùng `revision`) khi POST 200; map 400/409/422/500 kèm `detail` + `error`; lưu cổng thực tế khi bind port `0`; sửa lỗi giải phóng kép `m_tcpServer` trong `RestApiService::stop()` (do `QHttpServer::bind` đã reparent `m_tcpServer` vào `m_server`, dùng `QPointer` và đóng socket trước khi xóa `m_server`).
  - `src/app/main.cpp`: gọi thêm `sensorList->refresh()` cùng `monitorCtrl->refreshSensors()` khi `RestApiService::configApplied` phát tín hiệu để đồng bộ UI kiosk và nạp lại ngưỡng cho worker mà không cần khởi động lại app.
  - `tests/rest_config_test.cpp`: mở rộng kiểm thử đường POST phẳng, patch ngưỡng hợp lệ (`null` xóa / vắng key giữ nguyên), `sensor_id` lạ/inactive/DI → 422 rollback, `min >= max` (trực tiếp + sau khi ghép) / key lạ / trùng ID → 422, envelope gộp app-config + sensors, HTTP GET/POST thực tế qua `RestApiService` và xung đột revision 409 khi sửa từ kiosk `SensorListModel`.
- **Central Logger:**
  - `adapters/common/EdgeSource.h`, `RestV1Server.cpp`, `adapters/osg/test_osg_adapter.cpp`: thêm `minThreshold`/`maxThreshold` tùy chọn trên `AnalogPoint`, xuất vào `configResponse()` nếu có, không bật `thresholds_writable`, giữ phản hồi 409/422 `"Adapter is read-only; no changes were applied."` cho `sensors[]`.
  - `RestConfigParser.h`, `RestConfigService.{h,cpp}`, `test_rest_config_parser.cpp`, `test_rest_config_service.cpp`: parse `thresholds_writable`, phân biệt vắng key vs `null`, thêm `SensorThresholdPatch` + `buildSensorThresholdsPatch` + `RestConfigService::applySensorThresholds` (dùng chung guard `Endpoint::Apply`, phát `sensorThresholdsApplied`), trích thông điệp `detail`/`error` cho 422.
  - Tài liệu: cập nhật `docs/contracts/rest-config-contract-v1.md`, `adapters/osg/README.md`, `../data-logger/AGENTS.md`, `../data-logger/README.md`.
- **Build & Test:**
  - `data-logger`: `cmake --build ../data-logger/build/loggers-phase0 --target app rest_config_test` + `ctest -R '^rest_config_test$'` → pass (9/9 test functions).
  - `central_logger`: `cmake --build build/loggers-phase0` + `ctest --test-dir build/loggers-phase0` → **25/25** pass.

## Phase 3 — Backend Central

**2026-11-18 — Hoàn tất Phase 3 (chưa commit theo yêu cầu).**

- **Schema v8 & Settings (`src/utils/{Version,DbConstants,UiConstants}.h`, `001_initial.sql`, `AppSettings.h`, `SettingsRepository.cpp`, `SettingsController.{h,cpp}`, `docs/thiet_ke_db.md`):**
  - Bump `kSchemaVersion = 8`, thêm cột `app_settings.loggers_view_mode TEXT NOT NULL DEFAULT 'cards'` (chuẩn hóa `'cards'` / `'list'`).
  - Bổ sung `SettingsController::loggersViewMode` và `saveLoggersViewMode()`; cập nhật `docs/thiet_ke_db.md` đồng bộ ERD, bảng chi tiết và luồng merge sensor.
- **Live Models & Merger (`SensorLiveRow.h`, `SensorMerger.cpp`, `SensorStatusChip.qml`, `SensorSnapshotCache.{h,cpp}`, `LoggerLiveListModel.{h,cpp}`, `LoggerListModel.{h,cpp}`, `DashboardController.{h,cpp}`):**
  - `SensorSnapshotCache` lưu `measuredAt` theo từng logger, xóa khi `markOffline`.
  - `SensorMerger` tách hoàn toàn `alarmType` (chỉ bật khi `sample.qualityAlarm` từ thiết bị bật; ngưỡng mirror chỉ giải thích hướng `min`/`max`/`min+max` hoặc `device`) khỏi mã TT10 `displayStatus`; bỏ các helper tính alarm cục bộ cũ.
  - `DashboardController` cung cấp `liveSections` (`QVariantList` cho Cards) và `liveList` (`LoggerLiveListModel` 6 cột `Logger · Sensor · Value · Unit · Status · Limit`, hỗ trợ `filterText` và cập nhật tại chỗ qua `dataChanged` không reset scroll), đồng thời thêm `isLoggerOnline()` và `markLoggerDisabled()`.
  - `SensorCatalogRepository::upsert`: chuẩn hóa `QString` null thành chuỗi rỗng `""` cho `name`/`unit` để cảm biến DI/DO không có trường `"unit"` trong JSON `GET /config` không vi phạm `NOT NULL`.
- **Form & Threshold Push (`LoggerFormController.{h,cpp}`, `ThresholdController.{h,cpp}`, `RestConfigService.cpp`, `src/app/main.cpp`):**
  - `LoggerFormController`: giữ `existing->enabled` khi `updateLogger`, thêm `setLoggerEnabled(id, enabled)`, trả `"enabled"` trong `getLoggerFormData()`, thêm overload `saveLoggerFromForm(..., enabled, note)`.
  - `RestConfigService::fetchConfig`: gộp (coalesce) các lời gọi `GET /config` đồng thời trên cùng một `loggerId` thay vì báo lỗi giả khi `LoggerFormController` và `ThresholdController` cùng lắng nghe.
  - `ThresholdController`: nối `RestConfigService` + `DashboardController`, kiểm tra chặt `enabled + online + thresholds_writable + edgeRevision >= 0` (`canEdit`), đẩy `POST /api/v1/config` (`config.sensors[]`) rồi gọi `GET /api/v1/config` xác nhận trước khi mirror vào `logger_sensor` và cập nhật `last_revision`. Khóa hoàn toàn khi offline/disabled/read-only, không lưu nháp cục bộ và không xếp hàng gửi bù.
- **Tests & Kết quả:**
  - Mở rộng `test_database_migrations` (backup DB v7 → `.bak`, tạo mới v8), `test_database_repositories` (`loggers_view_mode`), `test_sensor_merger` (alarm từ edge bit độc lập TT10), `test_dashboard_controller` (`liveSections`/`liveList` 2 logger online+offline, `setLoggerEnabled`, persist `loggersViewMode`), `test_threshold_controller` (online push + GET xác nhận, offline/disabled/read-only chặn sửa & POST, 409/422/rớt mạng khi xác nhận không ghi DB).
  - `cmake --build build/loggers-phase0 --parallel 4 && QT_QPA_PLATFORM=offscreen ctest --test-dir build/loggers-phase0 --output-on-failure -j4`: **25/25** mục CTest pass (1.03s).

## Phase 4 — LoggersView Data | Graph

**2026-11-18 — Hoàn tất Phase 4 (tích hợp cùng Phase 5, chưa commit theo yêu cầu).**

- **Models & Controllers (`LoggerLiveCardsModel.{h,cpp}`, `DashboardController.{h,cpp}`, `RestConfigService.cpp`, `src/app/main.cpp`):**
  - Thêm `LoggerLiveCardsModel` (`QAbstractListModel`, cập nhật từng dòng tại chỗ qua `dataChanged` khi danh sách `loggerId` không đổi để poll không bao giờ reset vị trí cuộn của `ListView`) và `DashboardController::setLiveFilterText` lọc đồng nhất cả Cards lẫn List từ C++.
  - Bỏ `DashboardController::sensorTable`, `LoggerDetailViewModel`, `SensorMonitoringTableModel` cùng các bài test cũ `test_logger_detail_view_model` / `test_sensor_monitoring_table_model` (chuyển các kiểm thử `SensorSnapshotCache`, `LoggerOverviewTableModel` và `RtuConnectedRole` sang `test_dashboard_controller`).
  - Xóa UI download report và `fetchReadingsDebug`; giữ `RestConfigService::fetchReadingsDebug` trong C++ với log `qInfo` phục vụ chẩn đoán dev.
- **Components & Views (`LoggerLiveSection.qml`, `LiveSensorListTable.qml`, `LoggersView.qml`, `DashboardView.qml`, `Main.qml`, `AppNavigationRail.qml`):**
  - `LoggerLiveSection.qml`: header trạng thái online/offline/disabled, tên + mã trạm, số cảm biến, thời điểm cập nhật, chip Alarm, nút mở nhanh biểu đồ và lưới `LoggerKit.Components.SensorValueCard` (`compact: true`, tối đa 4 cột — 8 cảm biến analog hiển thị 4×2; làm mờ + `—` khi offline; `Repeater` gắn với số lượng cảm biến để không hủy/tạo lại card mỗi kỳ poll).
  - `LiveSensorListTable.qml`: bảng phẳng 6 cột (`Logger · Sensor · Value · Unit · Status · Limit`) dùng lại `SensorStatusColumn` và `IoColors`.
  - `LoggersView.qml`: top bar `TabBar` (`Data | Graph` dùng `ThemedTabButton`), ô tìm kiếm, nút chuyển `Cards` / `List` (gọi `SettingsController.saveLoggersViewMode`), badge `"%1 logger(s)"`; tab `Graph` nhận toàn bộ khối biểu đồ thời gian thực + bảng `"Other loggers"` từ Dashboard, giữ nguyên nhãn `"Show graph"` và chọn logger cục bộ qua `DashboardController.graphLoggerId` (không sửa `SettingsController.primaryLoggerId`).
  - `DashboardView.qml` & `Main.qml`: bỏ hẳn `summaryMode`, nút `Detail`/`Summary` và hai route `logger-detail` / `thresholds`; click sự kiện logger mở thẳng `Loggers > Graph` (`openLoggerGraph(loggerId)`). Xóa `LoggerDetailView.qml` và `ThresholdsView.qml`.

## Phase 5 — Settings > Loggers

**2026-11-18 — Hoàn tất Phase 5 (chưa commit theo yêu cầu).**

- **Components & Views (`SettingsGeneralPane.qml`, `ThresholdEditor.qml`, `LoggersSettingsPane.qml`, `SettingsView.qml`, `LoggerListModel.{h,cpp}`):**
  - `SettingsView.qml`: thêm `TabBar` (`General | Loggers` dùng `ThemedTabButton`), tách cài đặt toàn cục sang `SettingsGeneralPane.qml`.
  - `LoggersSettingsPane.qml`: bố cục 3 cột — (1) bảng danh sách logger (`Name · Station · Host · Modbus / API · Enabled · Status`) có ô tìm kiếm, nút `Add`/`Delete` (kèm `AlertDialog` xác nhận) và switch `Enabled` bật/tắt trực tiếp; (2) form cấu hình inline tách từ `LoggerFormDialog` bổ sung switch `Enabled` + trường `Note`, nút `Connect & Load Config` và `Save`; (3) `ThresholdEditor.qml`.
  - `ThresholdEditor.qml`: hiển thị revision thiết bị, nút nạp lại config, banner lý do chỉ đọc / lỗi / tiến trình đẩy, khóa cả `TextField` Min/Max và nút `Save` theo `ThresholdController.canEdit` khi offline/disabled/read-only.
  - Xóa `LoggerFormDialog.qml`.

## Phase 6 — Dọn dẹp & tài liệu

**2026-11-18 — Hoàn tất Phase 6 (chưa commit theo yêu cầu).**

- **Rà soát dead code & `qmllint`:**
  - `rg` toàn bộ `src/` và `tests/`: không còn tham chiếu mã nguồn tới `LoggerDetailViewModel`, `SensorMonitoringTableModel`, `LoggerDetailView`, `ThresholdsView`, `LoggerFormDialog`, không có bản sao `IoColors`/`ThemedTabButton`/`SensorValueCard` ngoài kit. `SensorStatusColumn` được tái sử dụng trong `LiveSensorListTable.qml`; `ThresholdTableModel` phục vụ `ThresholdController` + `ThresholdEditor.qml`.
  - Đổi tên role QML của `EnabledRole` thành `loggerEnabled` (`kRoleLoggerEnabled`) trên `LoggerListModel` và `LoggerLiveCardsModel` để tránh che khuất (`property-override`) thuộc tính `QQuickItem.enabled` trên delegate QML.
  - Tất cả file QML mới/viết lại đạt **0 warning, 0 info** khi chạy `cmake --build build/loggers-phase0 --target all_qmllint`.
- **Tài liệu:** cập nhật `AGENTS.md`, `HANDOFF.md`, `docs/README.md`, `docs/thiet_ke_db.md`, `docs/ui/material3-component-guidelines.md`, `src/network/README.md`, `docs/plan/loggers-data-graph-todo.md`.
- **Kiểm thử tổng thể (offscreen, không chạm thiết bị thật):**
  - Bổ sung `tests/core/test_loggers_refactor_qml.cpp` kiểm thử khói Qt Quick offscreen theo test matrix Plan §6 (0 logger empty state; 3 logger gồm 1 offline mờ + `—`; logger 8 analog chia lưới 4×2 và giữ nguyên `contentY` khi poll; `Settings > Loggers` mở khóa `ThresholdEditor` khi online + khóa tức thì khi rớt mạng; chuyển `Cards`/`List`; đổi theme `light`/`dark`).
  - Central (`build/loggers-phase0`): build + `all_qmllint` + `ctest` **24/24** pass.
  - Data Logger (`../data-logger/build/loggers-phase0`): build + `qmllint` + `ctest` **30/30** pass (`integration_modbus_test` tự skip do thiếu simulator RTU).

---

## Ghi chú thiết kế phát sinh

1. **`RestApiService::stop()` giải phóng kép trên `data-logger`:** `QHttpServer::bind(m_tcpServer)` chuyển quyền sở hữu `m_tcpServer` sang `m_server`, nên xóa cả hai gây SIGSEGV khi đóng server trong test. Đã sửa bằng `QPointer<QTcpServer>` và đóng socket trước khi hủy `m_server`.
2. **Chuẩn hóa chuỗi rỗng khi upsert catalog (`SensorCatalogRepository::upsert`):** cảm biến DI/DO trả về từ `GET /api/v1/config` không có trường `"unit"`, tạo `QString` null khiến driver `QSQLITE` bind thành SQL `NULL` và vi phạm `NOT NULL` trên `logger_sensor.unit`. Đã chuẩn hóa null `QString` về `""` cho `name` và `unit`.
3. **Gộp (`coalesce`) `RestConfigService::fetchConfig(loggerId)`:** khi mở một logger trong `Settings > Loggers`, cả `LoggerFormController` và `ThresholdController` đều cần snapshot `GET /config`; gộp các yêu cầu đồng thời trên cùng `loggerId` giúp dùng chung một lượt gọi mạng thay vì phát lỗi giả.
4. **Giữ vị trí cuộn trên `Loggers > Data` (Cards) và tránh che khuất `QQuickItem.enabled`:** bổ sung `LoggerLiveCardsModel` phát `dataChanged` tại chỗ kết hợp `Repeater` gắn số lượng cảm biến trong `LoggerLiveSection.qml`, đồng thời đặt tên role QML là `loggerEnabled` để logger bị tắt (`enabled = false`) không vô hiệu hóa thao tác chuột trên hàng bảng hoặc nút mở biểu đồ.

---

## Hậu kiểm song song UI · Logic · UX

**2026-11-18 — Sửa các lỗi phát hiện qua rà soát chéo UI / Logic / UX (chưa commit theo yêu cầu).**

- **UI / Layout & Theme:**
  - `LoggerLiveSection.qml`, `SettingsGeneralPane.qml`: bổ sung `implicitHeight: implicitContentHeight + topPadding + bottomPadding` trên `ElevatedPane` (khắc phục xẹp chiều cao `0px` khi làm delegate của `ListView` hoặc bọc trong `ColumnLayout`) và bọc `ScrollView` + sửa `Layout.preferredHeight: 1` cho divider; bổ sung `ScrollView` chế độ 1 cột (`width < 1120`) ở `LoggersSettingsPane.qml` để không tràn ở `1024×768`; sửa tỉ lệ `Layout.preferredWidth` + `Layout.horizontalStretchFactor` (`DashboardView.qml`, `LoggersSettingsPane.qml`) và `colMinimums` / `Switch` `Binding` trong bảng logger (`1280×800`).
  - `InlineBanner.qml`: bổ sung style riêng cho `semantic: "info"`; `ThresholdEditor.qml`: bỏ `placeholderText: "—"` trên ô Outlined, thêm margin dọc cell, dọn filter khi chuyển logger/đóng view và không hiện toast trùng khi đã có banner lỗi; `LiveSensorListTable.qml`: thêm `clip: true` cột Status; `LoggersView.qml`, `SettingsView.qml`: tắt gạch chân Material mặc định dưới `TabBar`; `OperationalStatus.qml`, `AttachDiType.qml`: dùng `AppColors.errorContainerFg` cho chữ chip `02` để đạt tương phản sáng/tối.
  - `../data-logger/src/app/qml/views/MonitorView.qml`: ẩn `EmptyStatePlaceholder` trùng ở chế độ List và chỉnh padding/cỡ nút `Calibrate` cho khung `SensorValueCard` kiosk `1024×600`.
- **Logic / Concurrency / Contract:**
  - `LoggerFormController.{h,cpp}` & `RestConfigService.{h,cpp}`: khóa `m_pendingApply` và đọc `revToPost` **trước** `emit formSaveFinished(...)` (tránh slot QML đồng bộ xóa `m_probedRevision` thành `-1` trước khi POST), thêm `applyInProgress` / `configApplySucceeded`, thêm `cancelProbe()` / `cancelConfigFetch()` khi đổi logger hoặc đổi host/port/token, đồng bộ `m_probedRevision`/`m_probedSensors` khi `ThresholdController` nạp/xác nhận `GET /config`, và xóa `primaryLoggerId` nếu trạm chính bị xóa.
  - `ThresholdController.{h,cpp}`: tách `m_saveLoggerId` khỏi `m_loadLoggerId` để đổi logger giữa chừng không bỏ dở mirror SQLite; chặn bão request 2s/lần bằng `m_autoLoadAttempted` và bảo vệ `409` / dữ liệu đang sửa; giữ optimistic lock khi có coalesced `GET` lúc bảng đang dirty; gửi patch partial theo R2; đồng bộ `onFormConfigApplied` giữa cột 2 ↔ cột 3; thống nhất `mirrorParsedCatalog` khi vắng khóa ngưỡng.
  - `PollHistoryStore.{h,cpp}`, `SensorSnapshotCache.{h,cpp}`, `DashboardController.cpp`, `LoggerRepository.cpp`, `ModbusBridge.cpp`, `SettingsController.{h,cpp}`: hỗ trợ Modbus Map v1 (`UNKNOWN`) trong `PollHistoryStore`, loại bỏ tính vượt ngưỡng cục bộ ở `refreshGraphChart` (chỉ theo bit alarm của edge), rebuild `SensorSnapshotCache` ngay sau `refreshLoggerCatalog`, lọc `enabled != 0` trong `countOnline()` + `ModbusBridge`, xóa trạng thái live/graph khi disable logger, tôn trọng `systemTimezone`, và tách `persistSettings(false)` cho `saveTheme`/`saveLoggersViewMode` (không trigger `SettingsController::saved()` / purge).
  - `adapters/common/RestV1Server.cpp`, `RestConfigParser.h`, `../data-logger/src/network/rest/{RestConfigUpdater.cpp, RestApiService.{h,cpp}}`: lọc `std::isfinite` ngưỡng, từ chối body rỗng / không có khóa hợp lệ ở đường POST phẳng cũ, kiểm tra `catalogAvailable` + số nguyên không âm ở adapter, và trả `detail` + băm SHA-256 so khớp token trên `RestApiService`.
- **Kiểm thử hồi quy bổ sung & xác nhận lại:** mở rộng `test_dashboard_controller`, `test_logger_form_save`, `test_threshold_controller`, `test_loggers_refactor_qml` và `rest_config_test`.
