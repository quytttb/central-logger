# Plan — Loggers: Data | Graph, Settings > Loggers, ngưỡng từ thiết bị

> Trạng thái: **hoàn tất, đã archive** (2026-11-20). Plan gốc của refactor
> 2026-11-18 (Phase 0–6). Xem [TODO](loggers-data-graph-todo.md) và
> [walkthrough](../walkthrough/loggers-data-graph.md) để tra cứu tiến độ.
> Không đổi contract ngoài phạm vi ghi ở §3.

## 1. Vấn đề hiện tại

| # | Vấn đề | Bằng chứng |
|---|--------|-----------|
| 1 | Muốn xem thông số phải bấm vào từng logger | `LoggersView` chỉ là bảng CRUD; `LoggerDetailView` là nơi duy nhất có số đo |
| 2 | Thông số và biểu đồ nằm hai chỗ khác nhau | Bảng sensor ở `LoggerDetailView`, biểu đồ gộp + mini chart ở `DashboardView` (chế độ Summary) |
| 3 | Dashboard Summary trùng vai trò với Loggers | `DashboardView.qml` có 2 chế độ; nút Detail/Summary ở top bar |
| 4 | Cấu hình logger nằm trong dialog CRUD | `LoggerFormDialog.qml` (524 dòng) mở từ bảng Loggers |
| 5 | Ngưỡng cảnh báo bị chia làm hai nguồn | `logger_sensor.min/max_threshold` (Central, không đẩy sang thiết bị) vs `sensor.min/max_threshold` + `analog_digital_link.trigger_on_max/min` (edge, điều khiển relay DO) |
| 6 | Cảnh báo từ thiết bị bị bỏ qua | `AlarmSample::qualityAlarm` (map v2) được parse nhưng `SensorMerger` không dùng; badge fleet dùng `HR1.bit2` nên có thể lệch với chip từng cảm biến |
| 7 | Nút Download report + Fetch readings debug hiện cho user | `LoggerDetailView` top bar; user vận hành không dùng, không hiểu |
| 8 | `logger_info.enabled` không sửa được | `LoggerFormController` hardcode `enabled = true` khi save |

## 2. Quyết định đã chốt (2026-11-18)

| # | Quyết định |
|---|-----------|
| D1 | **Ngưỡng: thiết bị là chủ (H1).** Edge trả `min_threshold`/`max_threshold` trong `GET /config`; Central **hiển thị** ngưỡng đó. |
| D1b | **Central sửa được ngưỡng và đẩy xuống edge** qua `POST /config`, giống các config khác (không còn ngưỡng "local-only"). |
| D2 | **Bỏ hẳn Dashboard Summary**; các biểu đồ của Summary chuyển sang `Loggers > Graph`. Dashboard giữ biểu đồ lưu lượng readings. |
| D3 | **Dùng lại code**: component dùng chung đưa vào `shared/logger-ui-kit`, không copy sang Central. |
| D4 | **Xoá hẳn route `logger-detail`**. Report/debug là việc của dev: không có UI, chỉ log ngầm (qWarning/log file). |
| D5 | **ThresholdsView gộp vào `Settings > Loggers`** (không thành tab thứ 3 của Loggers). |
| D6 | **Giữ “Show graph”** làm nhãn bộ chọn logger. Tab vẫn có tên `Graph`. |
| D7 | **Nhớ Cards/List qua lần mở app**: thêm `app_settings.loggers_view_mode` (`cards` mặc định, hoặc `list`). Schema v7 → v8; DB cũ được backup `.bak` và tạo mới theo chính sách hiện hành, không tự mang dữ liệu sang. |
| D8 | **Không cho sửa ngưỡng khi offline**: khóa cả editor và Save, kiểm tra lại trong C++. Không lưu ngưỡng cục bộ, không xếp hàng gửi lại. |

Hệ quả điều hướng: rail còn 4 đích — Dashboard · Loggers · History · Settings.

## 3. Thay đổi contract đã duyệt (triển khai ở Phase 2)

`POST /api/v1/config` **đã** có field `config.sensors[]` trong
[`rest-config-contract-v1.md`](../../contracts/rest-config-contract-v1.md) ("replace full"),
nhưng edge hiện **bỏ qua** field này (`RestConfigUpdater` chỉ ghi app-config;
`tests/rest_config_test.cpp` còn assert rằng `sensors` không vào audit).

Chốt lại nghĩa của `sensors[]` — **patch ngưỡng**, không replace toàn bộ catalog.
Dùng `sensor_id` của edge, không dùng ID hàng trong DB Central:

```json
{
  "api_version": 1,
  "request_id": "central-<uuid>",
  "expected_revision": 7,
  "config": {
    "sensors": [
      { "sensor_id": 3, "min_threshold": 0, "max_threshold": 30 },
      { "sensor_id": 4, "min_threshold": null, "max_threshold": 320 }
    ]
  }
}
```

| Rule | Nội dung |
|------|----------|
| R1 | Mỗi entry có `sensor_id` và ít nhất một trong `min_threshold`, `max_threshold`. ID trùng hoặc key khác → **422**. |
| R2 | Chỉ nhận số hữu hạn hoặc `null`. `null` = xoá ngưỡng; vắng key = giữ nguyên. |
| R3 | `sensor_id` phải là sensor ANALOG đang active và tồn tại. ID không hợp lệ → **422**, rollback toàn bộ. Không tạo/xoá sensor hay sửa liên kết DO bằng patch này. |
| R4 | So sánh min/max **sau khi ghép với giá trị hiện tại**; `min >= max` khi cả hai có giá trị → **422**. |
| R5 | So khớp `expected_revision` trong transaction trước khi ghi (lệch → **409**). Ghi ngưỡng, tăng `config_revision` và audit **cùng một transaction**. Audit chỉ ghi `{sensor_id, from, to}` theo whitelist. |
| R6 | Edge reload threshold cho `ModbusWorker` sau commit, không cần restart app; ngưỡng mới áp dụng ở lần đánh giá mẫu hợp lệ tiếp theo. |
| R7 | `GET /config` của firmware hỗ trợ trả `thresholds_writable: true` và `min_threshold`/`max_threshold` cho analog (`null` khi không đặt). Central phân biệt **vắng key** (không biết ngưỡng) với **`null`** (đã xác nhận chưa đặt). Đọc revision và catalog trong một snapshot nhất quán. |

Giữ `api_version: 1`; field đọc là additive. Firmware cũ có thể bỏ qua
`config.sensors[]` mà vẫn trả 2xx. Central chỉ cho sửa khi bản GET mới có
`thresholds_writable: true`, và GET lại sau POST để xác nhận ngưỡng đã thực sự
áp dụng. Adapter không bật cờ này và vẫn trả 422 “Adapter is read-only”,
kể cả khi có ngưỡng để hiển thị.

## 4. Kiến trúc đích

```
LoggerKit.Components (submodule, dùng chung 2 app)
├── ThemedTabButton.qml          ← chuyển từ data-logger
├── SensorValueCard.qml          ← tách từ MonitorView (generic, không domain)
└── (LoggerKit.Theme) IoColors.qml ← chuyển từ data-logger

CentralLogger.Components
├── logger/LoggerLiveSection.qml   ← 1 logger = header + grid card (dùng SensorValueCard)
├── logger/LiveSensorListTable.qml ← chế độ List của tab Data
├── settings/LoggersSettingsPane.qml
└── settings/ThresholdEditor.qml   ← tách từ ThresholdsView, nhúng vào pane

CentralLogger.Core
├── LoggerLiveCardsModel      (mới)  — mọi logger + rows, 1 lần đọc cache
├── LoggerLiveListModel       (mới)  — bản table phẳng cho chế độ List
├── ThresholdController       (sửa)  — thêm pushEdge + revision + mirror DB
├── RestConfigService         (sửa)  — applySensorThresholds()
└── (xoá) LoggerDetailViewModel, SensorMonitoringTableModel, ThresholdsView

CentralLogger.App
├── LoggersView (viết lại)  Data | Graph
├── SettingsView (+ tab Loggers)
├── DashboardView (bỏ summaryMode)
└── Main.qml (bỏ route logger-detail + thresholds)
```

## 5. Phases

### Phase 0 — Chuẩn bị (không đổi hành vi)

- Chốt & commit riêng đợt TT10 / Modbus map v2 ở Central và Data Logger
  trước khi sửa app hoặc bump submodule. Không gom phần quản lý tiến trình
  OSG và dọn tài liệu vào commit TT10.
- `ctest` xanh trên baseline đó.
- Bump submodule `shared/logger-ui-kit` lên commit mới nhất của data-logger
  (`24ef015`: token `monitorCardBg`, sửa `DateField`/`DatePickerPopup`).

**AC:** build + ctest xanh; `git submodule status` khớp HEAD của kit.

Có thể chuẩn bị các component dùng chung trong worktree kit riêng dựa trên
`24ef015`. Việc chuyển Monitor và bump submodule chỉ làm sau Phase 0.

### Phase 1 — Kit dùng chung (D3)

1. `LoggerKit.Theme`: thêm `IoColors` (từ `data-logger/src/theme/IoColors.qml`).
2. `LoggerKit.Components`: thêm `ThemedTabButton` (từ data-logger, bỏ import
   `DataLogger.Components`) và `SensorValueCard` (nhận dữ liệu đã định dạng;
   `valid` tách khỏi `digitalOn`, `statusText`/`statusColor` tách khỏi
   `alarmText`; `footerAction` để app gắn nút hiệu chuẩn). Kit **không** diễn
   giải mã TT10, không tính ngưỡng và không gọi controller của app.
3. Refactor `data-logger/src/app/qml/views/MonitorView.qml` dùng
   `SensorValueCard` + `LoggerKit.Theme.IoColors`; xoá bản local.
4. Xoá `data-logger/src/theme/IoColors.qml` + `ThemedTabButton.qml` local;
   `data-logger` bump submodule.

**AC:** data-logger build + `ctest` xanh; MonitorView grid/list **không đổi
giao diện** (so screenshot 1024×600 trước/sau); Central build xanh với kit mới;
`qmllint` sạch.

**Rủi ro:** kit là submodule 2 repo → phải commit kit trước, bump sau, đúng
thứ tự; data-logger có ràng buộc kiosk (font 28–42px) nên `SensorValueCard`
phải nhận scale qua `compact`/`fontScale` thay vì hardcode.

### Phase 2 — Contract ngưỡng (edge + adapter + Central service)

1. **data-logger**: `RestConfigUpdater::apply` xử lý `config.sensors[]` theo
   §3 R1–R6; `RestApiService` GET theo R7. Hiện Central gửi envelope còn
   updater edge đọc app-config ở root: cần đọc đúng `config` trong envelope
   v1, so revision trong transaction, giữ tương thích với các POST
   app-config dạng phẳng đã hỗ trợ (không cho đường cũ ghi ngưỡng).
2. **data-logger**: `MonitorController`/`ModbusWorker` reload threshold sau
   `configApplied`; ưu tiên đường `refreshSensors` có sẵn. Xác nhận kiosk
   sửa ngưỡng cũng tăng revision qua `SensorListModel::commitSensorWrite`.
3. **central adapters**: `RestV1Server` trả 422 cho `sensors[]`; `EdgeSource`
   thêm `std::optional<double> minThreshold/maxThreshold` trên `AnalogPoint`,
   `configResponse()` xuất ra nếu có (OSG: omit → Central hiện "Chưa có giới hạn").
4. **central**: `RestConfigService::applySensorThresholds(loggerId, revision,
   changes)`; `RestConfigParser` đã parse 2 field này, chỉ cần test.
5. Docs: cập nhật `central/docs/contracts/rest-config-contract-v1.md`,
   `adapters/osg/README.md`, `data-logger/README.md` + `AGENTS.md` (bỏ câu
   "POST /config chỉ ghi app-config").

**AC:** `rest_config_test` (data-logger) có case: patch hợp lệ → DB đổi +
revision+1 + audit whitelist; `sensor_id` lạ → 422 + rollback; `min>=max` →
422; key lạ → 422; `null` xoá. Test thêm v1 envelope với app-config và
sensors trong cùng transaction, POST phẳng cũ, cạnh tranh revision khi thay
đổi từ kiosk. Central: test parser/revision/409 và firmware cũ/adapters
không cho sửa.

### Phase 3 — Backend Central: model live + threshold push

1. `SensorSnapshotCache`: lưu thêm `QDateTime measuredAt` mỗi logger.
2. `LoggerLiveCardsModel` (`QAbstractListModel`? → thực tế: `Q_PROPERTY
   QVariantList liveSections` trên `DashboardController`, cùng pattern với
   `graphSensorCharts`, nhẹ hơn 1 model/logger):
   `[{loggerId, name, stationCode, online, polling, anyAlarm, sensorCount,
   lastUpdate, sensors:[{edgeSensorId, name, unit, value, sensorType,
   displayStatus, status03Auto, alarmType, attachDiTypeLabels,
   minThreshold, maxThreshold, hasLimit, valid}]}]`.
3. `LoggerLiveListModel` (`QAbstractTableModel`): cột Logger · Sensor · Value ·
   Unit · Status · Limit; nguồn = cùng dữ liệu trên.
4. `ThresholdController`: thêm `edgeRevision`, `pushProgress`,
   `save()` → kiểm tra online, capability và revision → validate →
   `RestConfigService::applySensorThresholds` → GET xác nhận → cập nhật
   `logger_sensor` (mirror) và `last_revision`. Khi offline hoặc đã disable:
   chỉ xem, không POST, không ghi DB và không gửi bù khi online lại. Nếu mất
   kết nối giữa chừng, yêu cầu load config lại để xác nhận kết quả; 409 yêu cầu
   tải lại trước khi lưu; 422/401 báo lỗi phù hợp.
5. `SensorMerger`: dùng alarm bit từ thiết bị khi có, tách biệt TT10 status.
   Không dùng ngưỡng tự đặt ở Central để quyết định alarm; ngưỡng mirror chỉ
   phục vụ hiển thị/giải thích cảnh báo. Dọn helper tính ngưỡng không dùng.
6. `logger_info.enabled`: `LoggerFormController` không hardcode `= true`.
7. `SettingsController` + `SettingsRepository` + schema v8: persist Cards/List
   với mặc định `cards`. Verify giá trị còn đúng sau reload và DB v7 được
   backup/tạo mới theo chính sách hiện hành.

**AC:** `test_dashboard_controller` + `test_threshold_controller` +
`test_sensor_merger` mở rộng: 2 logger cùng lúc trả đúng section; ngưỡng
NULL → `hasLimit=false`; offline không gửi POST; push fail không ghi DB;
alarm lấy từ edge và không đổi mã TT10; Cards/List được nhớ qua lần mở app.

### Phase 4 — LoggersView mới (Data | Graph) + xoá Detail/Summary

Tích hợp cùng Phase 5 để không có bản trung gian thiếu chỗ cấu hình logger.

1. `LoggersView.qml` viết lại: top bar = TabBar (`Data` | `Graph`) + search +
   toggle `Cards`/`List` từ `SettingsController` + badge "N logger(s)" (bỏ
   nút Add Logger & Set Threshold — chuyển sang Settings).
2. Tab **Data**: `ScrollView` dọc, mỗi logger = `LoggerLiveSection`
   (header: dot online/offline + tên + `N chỉ số` + `cập nhật HH:mm:ss` + badge
   alarm; body: grid `SensorValueCard` hoặc `LiveSensorListTable`).
   Empty state khi chưa có logger; logger offline → card mờ + `—`.
3. Tab **Graph**: bê nguyên khối Summary của `DashboardView`
   (`graphLoggerOptions`/`graphSensorCharts`/`graphChartAxis`/`overviewTable`)
   sang, giữ tên "Show graph"; chọn logger cục bộ tab này, không đụng
   `SettingsController.primaryLoggerId`.
4. `DashboardView`: xoá `viewMode`, `summaryMode`, `summaryLayout`,
   `dashboardTopBar` Detail/Summary, các helper `summaryGrid*`,
   `summaryColWeights`, `overviewPaneHeight`, `selectGraphLogger`,
   `syncGraphLogger`; giữ 3 StatCard + Traffic readings + Recent events.
   Click event → điều hướng `Loggers > Graph`.
5. `Main.qml`: bỏ `loggerDetailComp` + `thresholdsComp` + 2 case trong
   `sourceComponent`; `navigate()` bỏ nhánh `logger-detail`; `selectLogger()`
   thay bằng `openLoggerGraph(loggerId)`.
6. Xoá `LoggerDetailView.qml`, `ThresholdsView.qml`, `LoggerDetailViewModel.*`,
   `SensorMonitoringTableModel.*` + test tương ứng; xoá
   `RestConfigService::fetchReadingsDebug` khỏi UI (giữ hàm C++ + log ngầm
   `qInfo` khi debug, không có property QML).

**AC:** không còn route `logger-detail`/`thresholds` hoặc class
`LoggerDetailViewModel` trong mã app; build + ctest xanh; 2 tab render đúng
khi 0/1/N logger; Data tab cập nhật theo poll, không reset scroll.

### Phase 5 — Settings > Loggers (D5)

1. `SettingsView.qml`: thêm `TabBar` (General | Loggers), tách khối General
   hiện tại thành component `SettingsGeneralPane` để file không phình.
2. `LoggersSettingsPane.qml`: 3 cột — danh sách logger (bảng: name, station,
   host, modbus/API port, enabled, status), form logger inline (tách từ
   `LoggerFormDialog`, thêm switch `Enabled` + `Note`), pane sensor + ngưỡng
   (`ThresholdEditor` tách từ `ThresholdsView`, có cột Min/Max + nút Save →
   Phase 3 push).
3. `LoggerFormController`: bỏ tham số `note` thừa nếu không dùng; thêm
   `setLoggerEnabled(id, bool)`; Delete chuyển vào pane (giữ confirm dialog).
4. Xoá `LoggerFormDialog.qml`.

**AC:** thêm/sửa/xoá/bật-tắt logger hoàn tất trong Settings; sửa ngưỡng →
Save → POST /config thành công → `GET /config` trả lại đúng giá trị; 409 hiện
"Connect lại rồi lưu"; không còn dialog CRUD ở trang Loggers.

### Phase 6 — Dọn dẹp, test, tài liệu

- Grep dead code: `MonitorTableModel` (data-logger, còn dùng), `SensorStatusColumn`
  (còn dùng?), `IoColors` bản sao, `ThresholdTableModel` (nếu `ThresholdEditor`
  thay bằng model mới thì xoá).
- Cập nhật `AGENTS.md`, `docs/README.md`, `docs/ui/material3-component-guidelines.md`;
  đồng bộ `docs/thiet_ke_db.md` với cột `app_settings` ở Phase 3.
- `docs/walkthrough/loggers-data-graph.md` điền kết quả thực thi.
- Build + `ctest` toàn bộ, `qmllint`, smoke test thủ công với 2 adapter OSG.

## 6. Test matrix (thủ công, sau Phase 6)

| Case | Kỳ vọng |
|------|---------|
| 0 logger | Data/Graph empty state, không crash |
| 1 logger online | Card hiện giá trị, timestamp nhảy mỗi poll |
| 3 logger, 1 offline | Section offline mờ, `—`, không hiện giá trị cũ |
| Logger 8 analog | Grid 4 cột × 2 hàng, không tràn |
| Sửa ngưỡng khi online | Edge nhận, chip đỏ ngay sau poll kế tiếp |
| Logger offline / mất kết nối khi đang sửa | Editor và Save bị khóa; không ghi ngưỡng vào Central hoặc tự gửi khi online lại |
| Revision lệch (sửa từ edge) | 409 → message Connect lại |
| Bật/tắt logger | Poll dừng/chạy, badge cập nhật |
| Chuyển List rồi đóng/mở app | Vẫn mở ở List; DB mới mặc định Cards |
| Theme light/dark | Card, chip, chart đổi màu đúng |
| Kiosk data-logger | MonitorView giống trước refactor |
