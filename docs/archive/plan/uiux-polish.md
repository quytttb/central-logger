# Plan — UI/UX polish + fix vận hành (2026-11-20)

> Trạng thái: **đã triển khai và kiểm thử tự động; chờ nghiệm thu trên thiết bị**. Nguồn yêu cầu: user
> 2026-11-20 (5 mục). Tiến độ theo dõi tại
> [`uiux-polish-todo.md`](uiux-polish-todo.md); nhật ký thực thi tại
> [`../walkthrough/uiux-polish.md`](../walkthrough/uiux-polish.md).
>
> Refactor trước (Loggers `Data | Graph`, Settings > Loggers, ngưỡng từ thiết
> bị) đã xong và archive: [`loggers-data-graph-refactor.md`](loggers-data-graph-refactor.md).

## 1. Yêu cầu (5 mục)

| # | Yêu cầu | Ghi chú từ user |
|---|---------|-----------------|
| U1 | Logger mất kết nối rồi có kết nối lại nhưng Central **không tự reconnect** | "nó đang gặp lỗi không reconnect logger sau khi logger bị mất kết nối → có kết nối" |
| U1b | Thêm nút **Reload config** | Chỉ đặt trong **Settings > Loggers** |
| U2 | Card cảm biến khó nhìn: nền đang outline → chuyển **filled**; limit `0 - 15` → `[0 - 15]`; bỏ chữ **"No limit"** (để trống) | Không đổi bố cục theo screenshot, chỉ 3 điểm này |
| U3 | Graph: hết **flicker**, nhãn **max** không bị line đè, thêm **zoom in/out/reset** đổi độ dài cửa sổ thời gian (tham chiếu `data-logger`) | Dữ liệu: **RAM + query SQLite** cho cửa sổ dài |
| U4 | Bỏ **shift/ca** khỏi History; tối ưu **scroll** bảng History | Xoá cả code liên quan, không chỉ ẩn UI |
| U5 | Tối ưu **UI/layout/text/responsive** | Ngôn ngữ: **English hoàn toàn** |

## 2. Quyết định đã chốt

| # | Quyết định |
|---|------------|
| D1 | Reconnect là **bug fix**, không thêm setting `auto_reconnect`. Polling interval + `logger_info.enabled` giữ nguyên. |
| D2 | Nút **Reload config chỉ ở Settings > Loggers** (cột 2, cạnh Connect). Không thêm ở Loggers/Cards. |
| D3 | Card giữ nguyên bố cục hiện tại của `SensorValueCard`; chỉ thêm chế độ **filled** + format limit + ẩn "No limit". |
| D4 | Zoom graph: cửa sổ ≤ 6h lấy từ **RAM** (`PollHistoryStore`), dài hơn **query SQLite** nền + downsample. Giữ dữ liệu cũ trên màn hình cho tới khi có kết quả mới (không clear → không nháy). |
| D5 | Bỏ hẳn `AverageExportMode::Shift`, `kShiftLengthHours`, `kAverageLabelShift` và toàn bộ UI liên quan. |
| D6 | UI English hoàn toàn. **Giữ** header CSV tiếng Việt (`Thời gian`, `Giá trị trung bình 24h`) vì là file báo cáo — đổi nếu user yêu cầu sau. |
| D7 | Không đổi contract edge (`rest-config-contract-v1`, `modbus-map-v1/v2`, `provision-qr-v1`). Không thêm cột DB ⇒ **không bump schema**. |

## 3. Nguyên nhân gốc

### U1 — reconnect

`src/network/modbus/ModbusService.cpp`:

- `startPollCycle()` chỉ `connectDevice()` khi client ở `UnconnectedState`.
  Khi TCP chết mà `QModbusTcpClient` vẫn báo `ConnectedState` (half-open /
  peer đóng im lặng), mọi poll đi thẳng vào `readHeader()` trên socket chết.
- `finishCycle(success=false)` chỉ reset `pollInFlight` — **không teardown
  client**, nên không có đường hồi phục. Teardown hiện chỉ nằm trong
  `registerLogger()` khi host/port/unit/timeout đổi.
- Chỉ có watchdog cho **pha connect**, không có watchdog cho **cả chu kỳ đọc**.

### U2 — card khó nhìn

`SensorValueCard.qml` dùng `color: AppColors.monitorCardBg` + `border.color`
theo status ⇒ viền/outline mảnh trên nền tối. `LoggerLiveSection.qml` truyền
`qsTr("No limit")` khi không có ngưỡng, và `formatCompactLimit()` trả
`0 – 15` (thiếu ngoặc vuông).

### U3 — flicker / nhãn max / zoom

- `SensorMiniChart.qml`: 10 `Connections` gọi `spark.requestPaint()` riêng lẻ
  (nhiều lần/frame); gutter trái phụ thuộc text tick hiện tại nên lưới nhảy.
- `ChartTimeSeriesPanel.qml`: `onAxisChanged: rebuild()` → `clear()` +
  `append()` toàn bộ mỗi lần axis đổi ⇒ nháy.
- Nhãn max vẽ **trong** `ctx.clip()` nên line/area vẽ sau đè lên nhãn.
- `PollHistoryStore` chỉ giữ **20 điểm** (`kChartDisplayPointCount`), không có
  khái niệm cửa sổ thời gian ⇒ không thể zoom.

### U4 — History

- `reuseItems: false` + `root.hoveredRow` dùng chung: mỗi lần hover ghi
  property của root ⇒ toàn bộ delegate rebind.
- `HistoryTableModel::setRows()` gọi `beginResetModel()` mỗi lần auto-refresh
  (5 s) ⇒ mất vị trí scroll và giật.
- Shift: rải ở QML (`HistoryView`), ViewModel (`exportAverageCsv`) và service
  (`AverageExportMode`).

### U5 — UI

Toolbar History dồn ~9 control/1 hàng ở `minimumWidth: 1024`; Dashboard dùng
`RowLayout` cứng; còn sót chuỗi tiếng Việt trong `LoggerFormController` và
nhãn `"%1 record(s)"`.

## 4. Thay đổi theo file

### U1 — Modbus reconnect

| File | Thay đổi |
|------|----------|
| `src/network/modbus/ModbusService.{h,cpp}` | Tách `forceReconnect(LoggerState&)` từ khối teardown trong `registerLogger()`; gọi trong `finishCycle(success=false)`; watchdog toàn chu kỳ (`Defaults::kPollCycleTimeoutMs`); nối `errorOccurred` để fail nhanh; đếm `consecutiveFailures` + log chẩn đoán |
| `src/utils/AppConstants.h` | Thêm `kPollCycleTimeoutMs` |
| `tests/network/test_modbus_service_reconnect.cpp` (mới) | `QModbusTcpServer` thật: poll OK → đóng server → 1 chu kỳ fail → mở lại → `QTRY_VERIFY` poll OK trở lại |
| `tests/network/CMakeLists.txt` | Đăng ký test mới |

### U1b — Reload config (Settings)

| File | Thay đổi |
|------|----------|
| `src/core/LoggerFormController.{h,cpp}` | `Q_INVOKABLE void reloadLoggerConfig(qint64)`; `setModbusService()`; signal `configReloadFinished(bool, QString)`; GET /config → `upsertBatchPreservingThresholds` + `logger_info.last_revision` (1 transaction) → `DashboardController::refreshLoggerCatalog()` + `logEvent` + ép 1 chu kỳ Modbus |
| `src/core/DashboardController.{h,cpp}` | `setModbusService()` đã có; thêm `Q_INVOKABLE void reconnectLogger(qint64)` gọi `forceReconnect` qua queued call |
| `src/components/settings/LoggersSettingsPane.qml` | Nút "Reload config" (icon `sync`) cạnh Connect; disabled khi `mode === "add"` hoặc host không hợp lệ; toast + `InlineBanner` khi lỗi |
| `src/app/main.cpp` | Nối `loggerForm.setModbusService(&modbusService)` |
| `tests/core/test_logger_form_save.cpp` | Case reload: giả lập `configFetched` → catalog + `lastRevision` cập nhật |

### U2 — Card

| File | Thay đổi |
|------|----------|
| `shared/logger-ui-kit/src/components/status/SensorValueCard.qml` | `property bool filled`, `property color fillColor`; khi `filled` → nền `fillColor`, không viền outline |
| `shared/logger-ui-kit/tests/qml/tst_SensorValueCard.qml` | Case `filled: true` |
| `src/components/logger/LoggerLiveSection.qml` | `filled: true` + `fillColor` theo `surfaceContainerLow` / `successContainer` / `errorContainer`; `limitText` rỗng khi không có ngưỡng (bỏ "No limit") |
| `src/utils/SensorConstants.h` | `formatBracketLimit(min, max)` → `[0 - 15]`, `[≥ 0]`, `[≤ 15]` |
| `src/core/DashboardController.cpp` (`refreshLiveModels`) | `limitText` dùng `formatBracketLimit`; list giữ `formatCompactLimit` |
| Submodule | Commit kit trước → bump `shared/logger-ui-kit` |

### U3 — Graph

| File | Thay đổi |
|------|----------|
| `src/core/charts/PollHistoryStore.{h,cpp}` | Ring buffer theo **thời gian** (`kGraphRamMaxPoints`, `kGraphRamMaxMs`); `seriesForLogger(loggerId, fromMs)` |
| `src/core/charts/Downsampler.{h,cpp}` (mới) | Port min–max bucket + giữ điểm alarm từ `data-logger/src/utils/chart/Downsampler.h` |
| `src/core/charts/GraphHistoryQueryService.{h,cpp}` (mới) | Query `sensor_reading` theo logger + window trên thread pool (`Database::withScopedConnection`), trả series đã downsample + generation |
| `src/core/DashboardController.{h,cpp}` | `graphWindowMinutes` + `zoomGraphIn/Out/resetGraphZoom` + `graphWindowLabel`; `refreshGraphChart()` chọn RAM/SQLite, giữ dữ liệu cũ khi query chạy |
| `src/components/chart/SensorMiniChart.qml` | `scheduleRepaint()` coalesce; `Canvas.Immediate`; gutter ổn định; **rightGutter** vẽ nhãn max ngoài `clip` |
| `src/components/chart/ChartTimeSeriesPanel.qml` | `updateAxis()` thay `rebuild()` khi chỉ axis đổi; append incremental; `deadSeries` destroy trễ |
| `src/app/qml/views/LoggersView.qml` | Nút `−` / `+` / reset + label "Last 5 min" ở tab Graph |
| `tests/core/test_downsampler.cpp`, `tests/core/test_poll_history_store.cpp` | Test downsample + window lọc |

### U4 — History

| File | Thay đổi |
|------|----------|
| `src/app/qml/views/HistoryView.qml` | Xoá shift UI (label, SpinBox, menu item, title dialog, 3 property); `reuseItems: true`; bỏ `hoveredRow`/`HoverHandler`; dùng `SensorStatusChip` |
| `src/core/history/HistoryViewModel.{h,cpp}` | `exportAverageCsv(fileUrl, fromDate, toDate)`; `refresh()` dùng merge thay reset |
| `src/core/history/AverageExportService.{h,cpp}` | Xoá `AverageExportMode`, `shiftStartHour`, nhánh 8h |
| `src/core/history/HistoryTableModel.{h,cpp}` | `mergeLatest(rows)` — `beginInsertRows` cho dòng mới ở đầu |
| `src/utils/AppConstants.h`, `src/utils/FormatConstants.h` | Xoá `kShiftLengthHours`, `kAverageLabelShift` |
| `tests/core/test_average_export_service.cpp` | Xoá 2 case shift; cập nhật `hourlyParams()` |

### U5 — UI/layout/text/responsive

| File | Thay đổi |
|------|----------|
| `src/app/qml/views/HistoryView.qml` | Toolbar 2 hàng; `<1200` ẩn record count, gom Refresh/Export vào overflow menu |
| `src/app/qml/views/LoggersView.qml` | `<1100` ẩn "%1 logger(s)"; Cards/List thành segmented icon-only |
| `src/app/qml/views/DashboardView.qml` | `<1200` chuyển sang `ColumnLayout` |
| `src/components/logger/LoggerLiveSection.qml` | `<900` ẩn sensor count/subtitle; timestamp "Updated dd-MM-yyyy HH:mm" |
| `src/utils/FormatConstants.h` | `kDateDdMmYyyyHm = "dd-MM-yyyy HH:mm"` |
| `src/core/LoggerFormController.cpp` | Dịch chuỗi tiếng Việt còn sót sang English |
| `src/core/history/HistoryTableModel.cpp` | `"%1 records"` |

## 5. Phases & AC

### Phase A — Kit (`SensorValueCard` filled)

- Sửa kit + test; `cmake --build` kit, `ctest`, `qmllint`.
- Commit kit → bump submodule trong Central (và `data-logger` nếu cần).
- **AC:** kit test xanh; `filled: false` không đổi pixel so với trước.

### Phase B — Modbus reconnect (U1) + Reload config (U1b)

- `forceReconnect` + watchdog + `errorOccurred`; test server thật.
- `reloadLoggerConfig` + nút Settings.
- **AC:** rút mạng logger → 1 chu kỳ fail → cắm lại → poll tự xanh trở lại
  (không cần bấm gì, không restart app). Nút Reload config cập nhật catalog +
  `lastRevision` và ép một chu kỳ mới.

### Phase C — Card (U2)

- `filled`, `formatBracketLimit`, bỏ "No limit".
- **AC:** card có nền fill theo ngưỡng/alarm; limit `[0 - 15]`; không còn chuỗi
  "No limit"; không còn viền outline mảnh.

### Phase D — Graph (U3)

- RAM time-buffer, downsample, query service, zoom UI, chống flicker, nhãn max.
- **AC:** zoom 1m→7d không nháy; ≥ 6h vẫn có dữ liệu sau khi mở app lâu;
  đường line không đè nhãn max; CPU không tăng khi để yên tab Graph.

### Phase E — History (U4)

- Bỏ shift; scroll mượt (reuseItems + merge + chip).
- **AC:** không còn chuỗi/knob shift; scroll 5000 dòng không giật; auto-refresh
  5 s không nhảy vị trí scroll.

### Phase F — UI/responsive/text (U5)

- Toolbar 2 hàng, breakpoint 3 view, English hoá.
- **AC:** ở 1024×768 không control nào bị cắt; resize 1024↔1920 không vỡ
  layout; không còn chuỗi tiếng Việt trong UI (trừ header CSV).

### Phase G — Verify & tài liệu

- `cmake --build build --parallel`, `ctest --test-dir build --output-on-failure`,
  `qmllint` các QML đã sửa, test kit standalone.
- Cập nhật `HANDOFF.md`, `docs/README.md`, `AGENTS.md` (mục đang triển khai),
  điền `docs/walkthrough/uiux-polish.md`.

## 6. Ngoài phạm vi

- Không đổi contract edge, không bump schema DB.
- Không port code Python cũ.
- Không thêm system tray / thay đổi frameless window.
- Không đổi header CSV tiếng Việt (chờ user quyết).
- Các thay đổi chưa commit về OSG/packaging/docs đang có trong working tree
  **không** gộp vào đợt này.
