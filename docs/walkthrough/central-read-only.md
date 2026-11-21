# Walkthrough — Central chỉ đọc từ logger

> Plan: [central-read-only.md](../plan/central-read-only.md) · [TODO](../plan/central-read-only-todo.md)

## Phase A — Phạm vi và tài liệu (2026-11-21)

- Quy tắc chỉ đọc áp dụng cho cả Data Logger quảng bá quyền ghi và OSG qua adapter. Cài đặt theo dõi riêng của Central vẫn chỉnh được.
- Bộ [plan](../archive/plan/uiux-polish.md), [TODO](../archive/plan/uiux-polish-todo.md) và [walkthrough](../archive/walkthrough/uiux-polish.md) UI/UX đã chuyển vào archive, không đổi kết quả kiểm thử hay mục nghiệm thu còn mở.
- Kiểm tra ban đầu: form Add/Edit còn ghi `station_name`/`poll_interval`; bảng ngưỡng còn ghi `config.sensors[]`. Reload đã đọc GET nhưng còn dùng chung trạng thái với form.

## Phase B — C++ (2026-11-21)

- `LoggerFormController` chỉ dùng probe cho Add, kiểm tra snapshot khớp host/cổng/token trước khi nhập vào DB. Edit không yêu cầu kết nối và không thay catalog/revision. Reload giữ transaction catalog/revision, tách khỏi form, sau đó yêu cầu worker đọc lại.
- Gỡ xây dựng request POST, hàng đợi apply và kiểm tra quyền ghi khỏi REST client. Bảng ngưỡng chỉ đọc SQLite, không tự tải cấu hình khi chọn logger hoặc khi logger online lại. `ModbusService` không đổi.
- Chưa chạy build ở phase này: còn các tham chiếu QML và test của luồng ghi cũ, được thay trong phase C/D.

## Phase C — QML và hướng dẫn (2026-11-21)

- Form dùng nhãn `Display name`, `Central poll (s)` và `Monitor`; Add gợi ý tên/mã/unit ID nhưng không sao chép chu kỳ đo. Edit đọc từ DB, Reload không chạm các giá trị đang nhập.
- `SensorThresholdsPane` thay chỗ bảng chỉnh sửa, chỉ có ô tìm kiếm và giá trị. `—` nghĩa là không có giá trị trong bản sao, không khẳng định thiết bị OSG chưa cấu hình ngưỡng.
- Cập nhật hướng dẫn vận hành, REST contract (chỉ phần Central sử dụng) và hướng dẫn OSG. `POST /api/auth/login` của adapter vẫn dùng để xác thực, không phải ghi cấu hình.

## Phase D — Nghiệm thu (2026-11-21)

- Fixture loopback khởi chạy trên cổng ngẫu nhiên, không cần thiết bị thật.
  `test_logger_form_save` kiểm tra HTTP method và Modbus function code trong
  luồng Add/Edit/Reload của hai nguồn. `test_loggers_refactor_qml` dùng Qt Quick
  offscreen để thao tác form và xem ngưỡng.
- Cả hai fixture ghi nhận đúng hai `GET /api/v1/config` (Connect và Reload),
  không có REST khi Save/Edit. Mỗi lượt poll đọc `FC03 → FC03 → FC02 → FC01`
  với native và `FC03 → FC03` với OSG. Không có Modbus ghi, kể cả native trả
  `thresholds_writable: true`.
- Đã kiểm tra Edit khi offline; Reload giữ tên/chu kỳ Central kể cả khi có
  thay đổi chưa lưu; lỗi HTTP 503 hoặc thiếu revision không đổi bản sao; khóa
  ngưỡng vắng giữ giá trị cũ, `null` xóa giá trị trong bản sao.
- Qt 6.11.1: `cmake --build /tmp/opencode/central-read-only --parallel 4` đạt;
  `QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/opencode/central-read-only
  --output-on-failure --parallel 4` đạt **28/28**, gồm test adapter OSG.
  Một lượt thử ép `QT_QUICK_BACKEND=software` không đạt assertion vị trí cuộn
  của benchmark History; lượt offscreen chuẩn đạt. Không sửa History.
- Target `qmllint` chạy xong; chỉ còn cảnh báo đã biết ở `Main.qml`.
  `git diff --check` và kiểm tra liên kết trong plan/archive đều đạt.

### Còn trên thiết bị thật

- Khi được phép, thử trên cả Data Logger và OSG. Đặt tên/chu kỳ Central khác
  giá trị thiết bị, sửa khi offline, sau đó Reload khi kết nối lại. Xác nhận
  cài đặt thiết bị giữ nguyên và Central chỉ đọc.
- Nếu thu lưu lượng để nghiệm thu, ghi lại phương thức và function code;
  không lưu/chia sẻ Bearer token hay nội dung xác thực của adapter.
