# Plan — Central chỉ đọc từ logger (2026-11-21)

> Yêu cầu đã duyệt: “Central chỉ đọc từ các Logger, không can thiệp, config ngược lại Logger”.
> Theo dõi [TODO](central-read-only-todo.md) và [walkthrough](../walkthrough/central-read-only.md).

## Phạm vi

- Central chỉ đọc từ mọi logger: Data Logger gốc, OSG qua adapter, và thiết bị tương thích. Không `POST /api/v1/config`, không lệnh Modbus ghi hay điều khiển, kể cả khi edge quảng bá `thresholds_writable: true`. Không giữ tùy chọn bật lại đường ghi.
- Không sửa firmware, mã adapter, giao thức REST/Modbus/QR, token, schema SQLite, UI kit hay tính năng UI/UX đã hoàn thành. Giới hạn quyền của token khi sử dụng ngoài Central là phạm vi khác.

## Hành vi đích — Settings > Loggers

- **Add:** `Connect & Load Config` chỉ gọi GET. Dùng mã trạm, gợi ý tên hiển thị và Modbus unit ID khi phù hợp; không lấy `poll_interval` làm chu kỳ đọc của Central. `Save` ghi bản ghi Central, revision và catalog đã đọc vào SQLite.
- **Edit:** mở từ dữ liệu đã lưu, sửa được khi offline. Tên hiển thị, kết nối, chu kỳ Central đọc, timeout, bật/tắt giám sát và ghi chú là cài đặt Central. `Save` không gọi REST và không đổi mã trạm, ngưỡng hay revision. Lưu thông tin kết nối mới trước khi Reload.
- **Reload config:** chỉ trong Settings, GET theo kết nối đã lưu; cập nhật catalog và revision cùng một transaction rồi yêu cầu poll mới nếu đang giám sát. Không ghi đè form hay cài đặt Central. Lỗi GET không làm mất bản sao đã có.
- **Sensor thresholds:** chỉ xem bản sao từ logger, kể cả khi offline. Không có input, nút Save/Discard hoặc đường ghi C++. Giữ quy tắc đồng bộ: khóa ngưỡng vắng mặt không ghi đè giá trị đã biết; `null` xóa giới hạn trong bản sao.

## Các phase

1. **A — Phạm vi và tài liệu:** đưa plan UI/UX cũ ra khỏi danh sách plan hiện hành; lưu lịch sử và các mục nghiệm thu còn mở.
2. **B — C++:** tách Add/Save local/Reload, gỡ transport ghi REST và logic sửa ngưỡng, giữ Modbus đọc.
3. **C — QML và hướng dẫn:** phân biệt cài đặt Central và dữ liệu thiết bị chỉ đọc; cập nhật hướng dẫn Data Logger/OSG mà không đổi wire contract.
4. **D — Nghiệm thu:** Qt Test với fixture native (có quyền ghi) và OSG (chỉ đọc), kiểm tra phương thức HTTP và function code Modbus thực tế; build, CTest, rà soát QML. Ghi riêng việc cần thiết bị thật.
