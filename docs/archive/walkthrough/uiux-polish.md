# Walkthrough — UI/UX polish + fix vận hành

> Trạng thái: **đã triển khai và kiểm thử tự động; chờ nghiệm thu trên thiết bị** (2026-11-20).
> Plan: [`../plan/uiux-polish.md`](../plan/uiux-polish.md) ·
> TODO: [`../plan/uiux-polish-todo.md`](../plan/uiux-polish-todo.md)

Mỗi phase ghi: ngày, commit, file đổi, lệnh build/test + kết quả, sai lệch so
với plan và lý do.

---

## Phase 0 — Mở màn

**2026-11-20 — Chốt phạm vi 5 mục với user.**

- U1: bug reconnect, **không** thêm setting `auto_reconnect`.
- U1b: nút "Reload config" chỉ trong **Settings > Loggers**.
- U2: card chỉ cần **filled** + `[0 - 15]` + bỏ "No limit"; không đổi bố cục.
- U3: zoom graph dữ liệu **RAM + query SQLite** cho cửa sổ dài.
- U4: bỏ hẳn shift/ca; tối ưu scroll History.
- U5: UI **English hoàn toàn** (header CSV tiếng Việt giữ nguyên).

Tài liệu của refactor trước đã archive:
`docs/archive/plan/loggers-data-graph-*.md`,
`docs/archive/walkthrough/loggers-data-graph.md`.

## Phase A — Kit `SensorValueCard`

**2026-11-20 — Xong.** Kit commit `37dfa1d` và `1cc2f59`
(`central-uiux-filled-cards`), checkout trong cả hai app; gitlink ở các
repo cha chưa stage/commit. Commit kit chưa push.
`filled` mặc định tắt; test kiểm tra giữ màu/viền mặc định của Data Logger
và bỏ viền khi bật ở Central. Test badge alarm xác nhận dùng màu chữ tương
phản dành cho nền error. Không sửa mã nguồn Data Logger.

- Qt 6.11.1: build và `qmllint` standalone kit tại
  `/tmp/opencode/central-uiux-kit` đạt; `ctest` offscreen 1/1.

## Phase B — Modbus reconnect + Reload config

**2026-11-20 — Đã làm và kiểm thử tự động.**

- `ModbusService`: lỗi poll bỏ client cũ; tick sau tạo TCP mới. Watchdog
  riêng cho kết nối và toàn chuỗi PDU đọc (theo `timeout_s`); epoch của
  client ngăn tín hiệu cũ kết thúc nhầm chu kỳ. Logger bị tắt không poll.
- **Settings > Loggers > Reload config** dùng endpoint đã lưu, chỉ GET.
  Revision và catalog cập nhật trong một transaction; giữ ngưỡng nếu key
  vắng, xóa nếu edge trả `null`. Báo lỗi bằng toast và banner, chỉ ép poll
  mới sau khi mirror thành công. Dùng worker qua Dashboard, không inject trùng.
- Qt 6.11.1: build `test_modbus_service_reconnect`, `test_logger_form_save`;
  `ctest` 2/2. Đã kiểm thử restart server Modbus thật trên loopback và TCP
  mở nhưng không trả lời. Chưa rút/cắm cáp trên logger vật lý.

## Phase C — Card cảm biến

**2026-11-20 — Xong, smoke test QML đạt.** Giữ bố cục hiện
tại. Card analog hợp lệ có ngưỡng dùng nền success, alarm do edge báo
dùng nền error, card offline/không có ngưỡng dùng nền trung tính. Card
hiện `[min - max]` (một phía khi chỉ có một ngưỡng), trống khi không có;
bảng List giữ format cũ. Smoke test xác nhận màu và nhãn cho bốn trạng thái.

## Phase D — Graph

**2026-11-20 — Xong, test backend và QML đạt.**

- Zoom đổi miền thời gian thật (1 phút đến 7 ngày; reset 5 phút). Buffer
  giới hạn 6 giờ và 4.000 mẫu/sensor. Nếu RAM chưa phủ đủ (ví dụ vừa mở app)
  hoặc zoom dài, đọc SQLite nền và ghép với RAM; một query chạy tại một
  thời điểm, hủy query cũ khi đổi logger/cửa sổ. Không quét lại SQLite mỗi
  poll; chỉ tải lại khi cần bù dữ liệu đã rời RAM, có lỗi hoặc bấm Refresh.
  Schema v8 không lưu alarm: downsampler giữ điểm alarm ở RAM, không suy
  diễn ở SQLite.
- Grid chuyển sang model cập nhật `dataChanged` theo sensor; không dựng lại
  Canvas mỗi lần poll. Canvas gộp repaint, vẽ max ngoài vùng plot; Dashboard
  cập nhật riêng series và axis. Không query/vẽ Graph khi tab đóng.
- Qt 6.11.1: `test_downsampler`, `test_graph_history`, `test_poll_history_store`,
  `test_dashboard_controller` đạt. `test_loggers_refactor_qml` xác nhận nút
  zoom đổi cửa sổ, poll giữ nguyên instance Canvas, chart Dashboard giữ
  series sau khi đổi trục và khi thêm dữ liệu.

## Phase E — History

**2026-11-20 — Xong.** Xoá export theo ca ở UI,
ViewModel, SQL/CSV service và test cũ. Giữ export bảng và hourly + 24h, giữ
nhãn CSV tiếng Việt. TableView tái sử dụng cell, không cập nhật hover toàn
bảng; refresh không reset các hàng cũ, dùng `(recorded_at, id)` để phân biệt
các mẫu cùng lúc. Timer đọc lại bộ lọc đã Search, không áp dụng bộ lọc nháp.

- `test_average_export_service` và `test_history_table_model` đạt. Smoke test
  5.000 hàng giữ đúng reading ID và offset dọc sau refresh, kể cả khi thêm
  hàng mới và loại hàng cuối do giới hạn.
- Benchmark QML offscreen (`historyScrollBenchmarkAndRefreshAnchor -iterations 1 -median 3`):
  cùng dataset 5.000 hàng, 125 bước cuộn (4 hàng/bước). Layout trước/sau
  **1.148 → 880 ms**, giảm khoảng **23%**. Đây là thời gian chạy chuỗi thao
  tác layout, không phải FPS trên máy thật.

## Phase F — UI / responsive / text

**2026-11-20 — Xong, smoke test responsive đạt.** History có hai hàng filter/action,
gộp Refresh/Export vào Actions khi hẹp. Dashboard xếp chart trên events khi
thiếu ngang. Loggers thu gọn Cards/List và phần đầu section; tooltip giữ
thông tin bị ẩn. Chuỗi giao diện do Central sinh dùng English; thời điểm
card dạng `Updated dd-MM-yyyy HH:mm` theo timezone cấu hình.

## Phase G — Verify & tài liệu

**2026-11-20 — Build và kiểm thử tự động đạt (Qt 6.11.1).**

- Central: `cmake --build /tmp/opencode/central-uiux --target all qmllint --parallel 5`
  xong; `QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/opencode/central-uiux --output-on-failure`
  đạt **28/28**. Lint còn cảnh báo cũ trong `Main.qml` về `Screen` và enum
  resize; không đổi window chrome trong đợt này.
- Kit standalone: build + `qmllint` xong, CTest **1/1**. Cả hai app trỏ
  checkout kit `1cc2f59` local; chưa push kit, chưa commit repo cha.
- Đã cập nhật `HANDOFF.md`, `AGENTS.md`, index docs và mô tả luồng
  Graph/History. Không đổi schema DB hay contract edge trong đợt này.
- **Còn nghiệm thu:** rút/cắm cáp trên logger vật lý; đối chiếu flicker,
  scroll và bố cục trên màn hình triển khai. Hiện đã có test restart TCP
  loopback và smoke test offscreen, không thay thế nghiệm thu phần cứng.
