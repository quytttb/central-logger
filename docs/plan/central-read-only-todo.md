# TODO — Central chỉ đọc từ logger

> Plan: [central-read-only.md](central-read-only.md) · [Walkthrough](../walkthrough/central-read-only.md)

## Phase A — Phạm vi và tài liệu

- [x] Thay plan hiện hành; giữ bộ tài liệu UI/UX cũ trong archive, gồm các mục nghiệm thu chưa thực hiện.

## Phase B — C++

- [x] Add lấy snapshot bằng GET; Save Add/Edit chỉ ghi SQLite.
- [x] Reload chỉ cập nhật catalog/revision; giữ cài đặt Central và yêu cầu poll lại.
- [x] Xóa REST POST và các phương thức sửa/lưu ngưỡng; giữ hiển thị ngưỡng và Modbus đọc.

## Phase C — QML và hướng dẫn

- [x] Tách rõ cài đặt Central và ngưỡng thiết bị chỉ xem; giữ UI tiếng Anh và Reload trong Settings.
- [x] Cập nhật hướng dẫn cho Data Logger/OSG, không đổi contract wire hay schema.

## Phase D — Nghiệm thu

- [x] Kiểm thử Add/Edit/Reload với fixture native và OSG, ghi lại HTTP method và Modbus function code.
- [x] Kiểm thử Edit offline, ngưỡng vắng/`null`, lỗi Reload, và UI chỉ đọc.
- [x] Build Qt 6.11.1, CTest offscreen 28/28, kiểm tra QML và cập nhật walkthrough.
- [ ] Nghiệm thu trực quan và thu lưu lượng với Data Logger và OSG thật khi có thiết bị/quyền chạy.
