# Tài liệu — Central Logger

## Spec chính thức (đọc trước)

| Tài liệu | Nội dung |
|----------|----------|
| [`thiet_ke_db.md`](thiet_ke_db.md) | **Thiết kế database + RAM** (SQLite, 5 bảng) |
| [`adr/0001-db.md`](adr/0001-db.md) | **ADR:** `QSqlDatabase` / `QSQLITE` (`Qt6::Sql`) |
| [`../HANDOFF.md`](../HANDOFF.md) | Tổng quan dự án + thứ tự đọc |
| [`../AGENTS.md`](../AGENTS.md) | Quy tắc cho Cursor Agent |
| [`plan/tasks-6-24-agent-prompts.md`](plan/tasks-6-24-agent-prompts.md) | **19 task** sau nền móng (6–24): prompt copy-paste cho agent |
| [`plan/ui-modernization-agent-prompts.md`](plan/ui-modernization-agent-prompts.md) | **UI-M1…M3b** (done) — `Loader`, `TableView`, `LoggerListModel` table + `tr()` headers |
| [`contracts/`](contracts/) | Hợp đồng edge frozen v1: REST, Modbus (FC01/02/03), QR |

## Tham khảo app cũ (tuỳ chọn)

| Path | Nội dung |
|------|----------|
| [`THAM_KHAO_REPO_CU/phase1/`](THAM_KHAO_REPO_CU/phase1/) | Khảo sát Phase 1 (feature matrix FE-001…, pain points, QML logic inventory) |

**Không** port Python/tests. Mâu thuẫn với contracts → **theo `contracts/` + `thiet_ke_db.md`**.
