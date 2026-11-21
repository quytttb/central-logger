# Tài liệu — Central Logger

## Spec đang hiệu lực (đọc trước)

| Tài liệu | Nội dung |
|----------|----------|
| [`thiet_ke_db.md`](thiet_ke_db.md) | **Schema SQLite + RAM** (5 bảng) — nguồn duy nhất cho DB |
| [`contracts/`](contracts/) | Hợp đồng edge: REST config v1, Modbus map v1/v2, QR |
| [`ui/material3-component-guidelines.md`](ui/material3-component-guidelines.md) | Palette, token, component kit + quy ước UI |
| [`../AGENTS.md`](../AGENTS.md) | Quy tắc bắt buộc khi implement |
| [`../HANDOFF.md`](../HANDOFF.md) | Tổng quan dự án, quyết định đã chốt, trạng thái |

## Plan hiện hành — Central chỉ đọc từ logger (2026-11-21)

- [Plan](plan/central-read-only.md)
- [TODO](plan/central-read-only-todo.md)
- [Walkthrough](walkthrough/central-read-only.md)

## Đã triển khai — UI/UX polish (2026-11-20)

**Đã triển khai và kiểm thử tự động:** reconnect Modbus, Reload config trong
Settings, card cảm biến filled, Graph zoom RAM/SQLite, bỏ export theo ca và
tối ưu cuộn History, UI English/responsive. Còn nghiệm thu trên thiết bị.

| Tài liệu | Nội dung |
|----------|----------|
| [`archive/plan/uiux-polish.md`](archive/plan/uiux-polish.md) | Plan lịch sử (Phase A–G) |
| [`archive/plan/uiux-polish-todo.md`](archive/plan/uiux-polish-todo.md) | Checklist, gồm các mục nghiệm thu còn mở |
| [`archive/walkthrough/uiux-polish.md`](archive/walkthrough/uiux-polish.md) | Nhật ký và kết quả kiểm thử |

**Đã xong:** refactor Loggers `Data | Graph` + Settings > Loggers + ngưỡng từ
thiết bị (2026-11-18) — xem
[`archive/plan/loggers-data-graph-refactor.md`](archive/plan/loggers-data-graph-refactor.md).

## Tham khảo

| Tài liệu | Nội dung |
|----------|----------|
| [`adr/`](adr/) | ADR: SQLite qua QtSql, store-on-change cho `sensor_reading` |
| [`plan/chien_luoc_material3_cho_qt_desktop.md`](plan/chien_luoc_material3_cho_qt_desktop.md) | Chiến lược M3 (palette, layout, shape) |
| [`report/`](report/) | Audit + bug survey (không phải spec) |
| [`archive/`](archive/) | Plan/report/walkthrough đã hoàn thành — chỉ tra cứu lịch sử |
| [`THAM_KHAO_REPO_CU/`](THAM_KHAO_REPO_CU/) | Khảo sát app PySide6 cũ — **không** port code |

**Mâu thuẫn** giữa bất kỳ tài liệu nào và `contracts/` + `thiet_ke_db.md` →
theo **contracts + thiet_ke_db** (hoặc hỏi user nếu contract cần đổi).
