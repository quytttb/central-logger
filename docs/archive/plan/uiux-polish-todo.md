# TODO — UI/UX polish + fix vận hành

> Nguồn: [`plan/uiux-polish.md`](uiux-polish.md). Cập nhật tiến độ tại đây;
> không sửa plan để đổi phạm vi.

Ký hiệu: `[ ]` chưa làm · `[~]` đang làm · `[x]` xong · `[-]` bỏ

## Phase A — Kit `SensorValueCard` (U2)

- [x] Thêm `filled` + `fillColor` vào `shared/logger-ui-kit/.../SensorValueCard.qml`
- [x] Test kit `tst_SensorValueCard.qml`: case `filled: true`
- [x] Build + `ctest` + `qmllint` kit standalone
- [x] Commit kit `37dfa1d` + `1cc2f59` → cập nhật checkout submodule ở Central và Data Logger

## Phase B — Modbus reconnect (U1) + Reload config (U1b)

- [x] `ModbusService::forceReconnect()` dùng chung cho đổi endpoint, reconnect thủ công và lỗi poll
- [x] Khi poll lỗi: dừng chu kỳ, bỏ client và dựng lại ở lần poll kế tiếp; bỏ qua tín hiệu từ client cũ
- [x] Watchdog theo logger: giới hạn connect riêng, toàn bộ pha đọc dùng `timeout_s` theo contract
- [x] Nối `QModbusDevice::errorOccurred` để fail nhanh
- [x] Log chẩn đoán: lỗi đầu tiên (state/error/epoch) và số lần lỗi khi phục hồi
- [x] `tests/network/test_modbus_service_reconnect.cpp` + CMake, gồm server restart và TCP không phản hồi
- [x] `DashboardController::reconnectLogger(qint64)` (queued → `forceReconnect`)
- [x] `LoggerFormController::reloadLoggerConfig(qint64)` + signal `configReloadFinished`
- [x] Tận dụng wiring Modbus sẵn có qua `DashboardController`; không inject worker lần hai
- [x] `LoggersSettingsPane.qml`: nút "Reload config" chỉ khi edit, dùng endpoint đã lưu
- [x] Toast + `InlineBanner` khi reload lỗi
- [x] Test `test_logger_form_save.cpp`: cập nhật catalog, revision và giữ/xóa ngưỡng theo trường vắng/`null`; không POST

## Phase C — Card cảm biến (U2)

- [x] `SensorConstants::formatBracketLimit()` → `[0 - 15]`, `[≥ 0]`, `[≤ 15]`
- [x] `DashboardController::refreshLiveModels`: `limitText` dùng bracket (list giữ compact)
- [x] `LoggerLiveSection.qml`: `filled: true`, `fillColor` theo ngưỡng/alarm, bỏ "No limit"
- [x] Kiểm tra 4 trạng thái: có ngưỡng, alarm, không ngưỡng, offline

## Phase D — Graph (U3)

- [x] `AppConstants.h`: `kGraphRamMaxPoints`, `kGraphRamMaxMs`, preset zoom, `kGraphDownsamplePoints`
- [x] `PollHistoryStore`: ring buffer theo thời gian + `seriesForLogger(id, fromMs)`
- [x] `src/core/charts/Downsampler.{h,cpp}`: stream min–max + giữ điểm alarm có trong RAM
- [x] `tests/core/test_downsampler.cpp`
- [x] `src/core/charts/GraphHistoryQueryService.{h,cpp}`: query nền + downsample + hủy request bị thay thế
- [x] `DashboardController`: `graphWindowMinutes`, `zoomGraphIn/Out/resetGraphZoom`, `graphWindowLabel`
- [x] Dùng RAM khi phủ đủ khoảng; bù từ SQLite khi thiếu hoặc cửa sổ >6h; giữ chart cũ cùng logger khi chờ
- [x] `SensorMiniChart.qml`: `scheduleRepaint()` coalesce + `Canvas.Immediate` + gutter ổn định
- [x] `SensorMiniChart.qml`: `rightGutter` vẽ nhãn max ngoài `ctx.clip()`
- [x] `ChartTimeSeriesPanel.qml`: đổi axis riêng, append/trim hoặc replace điểm theo lô, hủy series trễ
- [x] `LoggersView.qml`: nút − / + / reset, nhãn cửa sổ và model giữ delegate khi poll
- [x] `test_poll_history_store.cpp` và `test_graph_history.cpp`: giới hạn RAM, zoom tới 7 ngày, ghép RAM/SQLite
- [x] Smoke test giao diện Graph: nút zoom và giữ cùng delegate qua poll

## Phase E — History (U4)

- [x] `AverageExportService`: xoá `AverageExportMode`, `shiftStartHour`, nhánh 8h
- [x] `HistoryViewModel::exportAverageCsv(fileUrl, from, to)` (bỏ 2 tham số)
- [x] `HistoryView.qml`: xoá control, menu và dialog của export theo ca
- [x] `AppConstants.h`: xoá `kShiftLengthHours`; `FormatConstants.h`: xoá `kAverageLabelShift`
- [x] `test_average_export_service.cpp`: xoá 2 case shift, cập nhật `hourlyParams()`
- [x] `HistoryView.qml`: `reuseItems: true`, bỏ hover theo hàng; delegate chỉ bind `display` và `statusCode`
- [x] `HistoryView.qml`: dùng `SensorStatusChip` qua `Loader` ở cột status
- [x] `HistoryTableModel::mergeLatest()`: cập nhật theo thứ tự `(recorded_at, id)`, giữ vị trí đọc khi refresh
- [x] Đo scroll 5000 dòng: 1.148 → 880 ms (median 3 lượt offscreen); test giữ hàng và offset qua refresh

## Phase F — UI / responsive / text (U5)

- [x] `HistoryView.qml`: toolbar 2 hàng; `<1200` ẩn record count + menu Actions
- [x] `LoggersView.qml`: `<1100` ẩn số logger; Cards/List icon-only khi hẹp
- [x] `DashboardView.qml`: grid một cột khi `<1200`, hai cột khi đủ chỗ
- [x] `LoggerLiveSection.qml`: `<900` ẩn sensor count/station code; giữ tooltip
- [x] `FormatConstants.h`: `kDateDdMmYyyyHm`; timestamp "Updated 20-11-2026 16:36"
- [x] `HistoryView.qml`: nhãn `1 record` / `%1 records`
- [x] `LoggerFormController.cpp` và `SqlHelper.h`: dịch thông báo sang English
- [x] Rà chuỗi tiếng Việt trong C++/QML: chỉ còn comment và nhãn CSV chủ ý giữ
- [x] Smoke test resize 1024×768 → 1920×1080: Dashboard, History toolbar; Settings cuộn ở chiều rộng hẹp

## Phase G — Verify & tài liệu

- [x] Build Central với Qt 6.11.1 tại `/tmp/opencode/central-uiux`
- [x] CTest Central offscreen: 28/28
- [x] `qmllint` aggregate: chạy xong; cảnh báo có sẵn ở `Main.qml`, không có warning mới trong QML đã sửa
- [x] Kit standalone: build + CTest 1/1 + `qmllint`
- [x] Smoke test offscreen: zoom 1m↔7d, giữ tile và scroll khi cập nhật, bố cục 1024×768↔1920×1080
- [ ] Nghiệm thu vật lý: rút/cắm mạng logger; xem Graph/History khi có dữ liệu thực; kiểm tra layout trên máy triển khai
- [x] Cập nhật `HANDOFF.md`, `docs/README.md`, `AGENTS.md` và mục luồng Graph/History trong `docs/thiet_ke_db.md`
- [x] Điền `docs/walkthrough/uiux-polish.md`
