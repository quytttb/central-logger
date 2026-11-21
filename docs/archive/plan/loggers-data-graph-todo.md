# TODO — Loggers Data | Graph refactor

> Trạng thái: **hoàn tất, đã archive** (2026-11-20). Checklist của refactor 2026-11-18.
> Nguồn: [`plan/loggers-data-graph-refactor.md`](loggers-data-graph-refactor.md).
> Không sửa plan để đổi phạm vi; mọi việc mới theo
> [`uiux-polish-todo.md`](uiux-polish-todo.md).

Ký hiệu: `[ ]` chưa làm · `[~]` đang làm · `[x]` xong · `[-]` bỏ

## Phase 0 — Chuẩn bị

- [x] Chốt bốn quyết định còn mở (plan §2–3)
- [x] Commit riêng TT10 / Modbus map v2: Central `f10e2d0`, Data Logger `5d91023` (local, theo yêu cầu)
- [x] Build + `ctest` hiện trạng Central (25/25)
- [x] Build + `ctest` hiện trạng Data Logger (30/30 theo CTest; `integration_modbus_test` tự skip do thiếu simulator)
- [x] Checkout kit Central qua `24ef015` sau khi tách TT10

## Phase 1 — Kit dùng chung

- [x] `LoggerKit.Theme`: thêm `IoColors` trong worktree kit riêng
- [x] `LoggerKit.Components`: thêm `ThemedTabButton` và `SensorValueCard` trong worktree kit riêng
- [x] Verify standalone kit: build, `qmllint`, Qt Quick Test
- [x] Refactor `data-logger/MonitorView.qml` dùng kit component
- [x] Xoá `data-logger/src/theme/IoColors.qml`, `ThemedTabButton.qml` local
- [x] Commit kit `2080c2b` và checkout trong submodule hai app; gitlink app chưa commit
- [x] Verify tích hợp Central: build + 25/25 CTest với kit `2080c2b`
- [x] Verify tích hợp Data Logger: build và `qmllint` đạt; CTest 30/30, trong đó bài RTU tự skip do thiếu simulator
- [ ] So giao diện kiosk 1024×600 trước/sau; chưa thử phần cứng

## Phase 2 — Contract ngưỡng

- [x] `data-logger`: parse envelope v1, optimistic locking và patch `config.sensors[]` (R1–R6); giữ POST app-config dạng phẳng
- [x] `data-logger`: `GET /config` trả `thresholds_writable`, `min_threshold`/`max_threshold` (R7)
- [x] `data-logger`: reload threshold cho worker sau `configApplied`
- [x] `data-logger`: test `rest_config_test` (hợp lệ / sensor lạ / min>=max / key lạ / null)
- [x] `central adapters`: `RestV1Server` trả 422 cho `sensors[]`
- [x] `central adapters`: `AnalogPoint` + `configResponse()` xuất limit
- [x] `central`: `RestConfigService::applySensorThresholds`
- [x] Docs: `rest-config-contract-v1.md`, `adapters/osg/README.md`, data-logger README/AGENTS

## Phase 3 — Backend Central

- [x] `SensorSnapshotCache` lưu `measuredAt`
- [x] `DashboardController::liveSections` (QVariantList)
- [x] `LoggerLiveListModel` (cột Logger/Sensor/Value/Unit/Status/Limit)
- [x] `ThresholdController`: online + capability + revision → POST → GET xác nhận → mirror DB; không lưu/xếp hàng khi offline
- [x] `SensorMerger`: dùng alarm bit của edge, không thay thế TT10 status
- [x] `LoggerFormController`: bỏ hardcode `enabled = true`, thêm `setLoggerEnabled`
- [x] Persist `app_settings.loggers_view_mode` (`cards`/`list`), bump schema v8 và đồng bộ `thiet_ke_db.md`
- [x] Test: dashboard/threshold/merger
- [x] Test schema v8 backup DB cũ và Cards/List lưu được qua reload

## Phase 4 — LoggersView Data | Graph

- [x] `LoggersView.qml`: TabBar Data|Graph + Cards/List + search
- [x] `LoggerLiveSection.qml` (header + grid)
- [x] `LiveSensorListTable.qml` (chế độ List)
- [x] Tab Graph: chuyển Summary từ Dashboard, giữ nhãn “Show graph”
- [x] `DashboardView.qml`: bỏ `summaryMode` + helper summary*
- [x] `Main.qml`: bỏ route `logger-detail`/`thresholds`
- [x] Xoá `LoggerDetailView`, `ThresholdsView`, `LoggerDetailViewModel`, `SensorMonitoringTableModel` + test
- [x] Bỏ UI report và `fetchReadingsDebug` (chỉ giữ chẩn đoán phía dev)

## Phase 5 — Settings > Loggers

- [x] `SettingsView`: TabBar General|Loggers, tách `SettingsGeneralPane`
- [x] `LoggersSettingsPane.qml` (list + form inline + sensor/ngưỡng)
- [x] `ThresholdEditor.qml` (tách từ ThresholdsView)
- [x] Form: thêm switch `Enabled` + `Note`
- [x] Xoá `LoggerFormDialog.qml`

## Phase 6 — Dọn dẹp & tài liệu

- [x] Grep dead code (`SensorStatusColumn`, `ThresholdTableModel`, `IoColors` bản sao)
- [x] Cập nhật `AGENTS.md`, `docs/README.md`, `material3-component-guidelines.md`
- [x] Điền `docs/walkthrough/loggers-data-graph.md`
- [x] Build + ctest + qmllint toàn bộ
- [x] Smoke test offscreen theo bảng test matrix (`test_loggers_refactor_qml` + `test_osg_adapter`; chưa chạm phần cứng thật)
