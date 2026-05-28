# Scope Document — Central Logger App Rewrite

**Version:** 1.0 | **Date:** 2026-05-23 | **Status:** Draft (awaiting Phase 1 sign-off)

---

## 1. Problem Statement

Central Logger quản lý tập trung nhiều **Modbus TCP Data Logger** công nghiệp trên LAN nhà máy. App hiện tại (PySide6 + Python) hoạt động nhưng có giới hạn về hiệu năng (GIL, asyncio complexity), khó maintain (logic nghiệp vụ trong QML `.js`), và deploy phức tạp (Nuitka + ZBar DLL Windows).

Mục tiêu rewrite: Dùng **Qt 6 C++/QML** để đạt native performance, MVVM sạch (logic trong C++ ViewModel), và deploy đơn giản hơn qua Qt Installer Framework.

---

## 2. Users

| Actor | Vai trò |
|-------|---------|
| **Operator** | Xem dashboard, theo dõi sensor, xem events — không thay đổi cấu hình |
| **Admin** | CRUD loggers, chỉnh sửa cấu hình thiết bị, thay đổi settings |

Hiện tại: single-user desktop app (multi-user là Future scope).

---

## 3. MVP Scope — Phase 4 Rewrite (Core features)

Tất cả FE-001 → FE-017 từ [01-feature-matrix.md](01-feature-matrix.md):

| ID | Feature | Ghi chú |
|----|---------|---------|
| FE-001 | Fleet statistics (total/online/alarms) | AppState singleton |
| FE-002 | Dashboard ingestion chart 24h | SQL bucket aggregation |
| FE-003 | Recent events list + drill-down | Last 20 system events |
| FE-004 | Logger list + search | `QSortFilterProxyModel` |
| FE-005 | Add logger | Form + optional QR |
| FE-006 | Edit logger | Connection + cloud + device config |
| FE-007 | Delete logger | Confirm dialog + cascade |
| FE-008 | Modbus TCP polling | Per-logger QThread worker |
| FE-009 | Sensor monitoring table | `QAbstractTableModel` |
| FE-010 | Sensor trending chart | In-memory poll history |
| FE-011 | Logger overview badges | poll/RTU/alarm/online |
| FE-012 | REST fetch config | `GET /api/v1/config` |
| FE-013 | REST apply config | `POST /api/v1/config` + revision |
| FE-014 | REST readings merge | `GET /api/v1/readings` + catalog |
| FE-015 | App settings | theme/timezone/retention/maintenance |
| FE-016 | Data retention purge | Auto + on-demand |
| FE-017 | System tray + frameless window | `QSystemTrayIcon` |

---

## 4. Nice-to-have (target Phase 4 if time allows)

| ID | Feature |
|----|---------|
| FE-020 | QR provision scan (C++ ZBar/ZXing) |
| FE-021 | Download latest report |
| FE-022 | Connect & Load Config probe |
| FE-023 | Collapsible sidebar + responsive layout |

---

## 5. Explicitly Out of Scope — v1

| Item | Lý do |
|------|-------|
| Multi-user / roles (FE-030) | Cần auth layer; single-user đủ cho use case hiện tại |
| Export Excel/PDF (FE-031) | Chưa có yêu cầu format cụ thể |
| PostgreSQL (FE-032) | SQLite đủ cho single-machine |
| macOS (FE-033) | Linux + Windows đủ; macOS nếu cần sau |
| Cloud sync / multi-site | Out of architecture scope v1 |

---

## 6. Dependencies & Contracts (frozen at v1)

Repo mới **phụ thuộc** vào các hợp đồng sau — **không thay đổi** trong Phase 4 rewrite:

| Hợp đồng | File tham chiếu | Status |
|----------|-----------------|--------|
| Modbus TCP Map v1 | `README.md` + `services/modbus_map.py` | **Frozen** |
| REST Config v1 (`GET/POST /config`, `/readings`, `/reports/latest`) | `docs/rest-config-contract-v1.md` | **Frozen** |
| QR Provision schema v1 | `docs/provision-qr-v1.md` | **Frozen** |
| SQLite schema (4 tables) | `docs/phase1/05-python-cpp-migration-map.md` (DB section) | Migrate as-is |

Nếu edge firmware thay đổi contract, cần changelog + negotiation trước khi merge vào repo mới.

---

## 7. Risks

| Rủi ro | Xác suất | Impact | Giảm thiểu |
|--------|----------|--------|------------|
| Edge firmware thay đổi REST/Modbus contract | Thấp | Cao | Freeze v1; changelog protocol |
| **Modbus FC3 vượt 125 registers** (`N ≥ 15` nếu một PDU gộp header + toàn bộ sensors) | Trung | Cao | Repo C++: [`docs/contracts/modbus-map-v1.md`](../../contracts/modbus-map-v1.md) — multi-PDU (pha header + chunk sensor); xem [`thiet_ke_db.md`](../../thiet_ke_db.md) luồng Modbus → `sensor_reading` |
| **Firmware chưa expose FC02 (DI) / FC01 (DO)** | Trung | Cao | [`docs/contracts/modbus-map-v1.md`](../../contracts/modbus-map-v1.md) §3–5; live UI [`thiet_ke_db.md`](../../thiet_ke_db.md) §3.3 — **không** poll `/readings` |
| Qt 6 Modbus TCP module không đủ feature | Thấp | Trung | Fallback `QTcpSocket` raw framing |
| ZBar/ZXing Qt port phức tạp | Trung | Thấp | QR là Nice-to-have; skip nếu chậm |
| Windows Qt deploy phức tạp | Trung | Trung | `windeployqt` + Qt IFW; test CI sớm |
| Canvas chart vs QtCharts decision delay | Trung | Trung | ADR quyết định trong Phase 2 trước khi code |

---

## 8. Success Criteria for Phase 1 (Definition of Done)

- [x] App PySide6 hiện tại chạy được trên máy dev; pytest xanh (104 passed)
- [x] `01-feature-matrix.md` ≥ 28 dòng feature, mỗi Core có flow + table + backend entry
- [x] `02-pain-points.md` ≥ 8 mục với evidence cụ thể (code path)
- [x] `03-scope-document.md` (file này) — draft hoàn chỉnh
- [x] `04-non-functional-requirements.md` có số đo baseline
- [x] `05-python-cpp-migration-map.md` map Python module → C++ layer
- [x] `06-qml-logic-inventory.md` liệt kê mọi function trong 2 file `.js`
- [ ] `07-manual-test-with-hardware.md` — hoàn thành khi test với thiết bị thật (≥8/10 pass)
- [ ] Scope Document reviewed (self hoặc stakeholder sign-off)

---

## 9. Handoff to Phase 2

Khi Phase 1 Done:

1. **Copy** `docs/phase1/` sang repo C++ mới.
2. **Phase 2 input:** `01-feature-matrix.md` làm Use Case seeds; `05-migration-map.md` làm Component Diagram seeds; `06-qml-logic-inventory.md` làm ViewModel method list.
3. **Open questions** (cần trả lời trong Phase 2 ADR):
   - QtCharts `QLineSeries` vs Canvas custom paint cho trending chart?
   - `QModbusTcpClient` (Qt Modbus module) vs `QTcpSocket` raw framing?
   - Single `DashboardController` C++ vs tách `LoggerDetailController`?
   - ~~`QSqlDatabase` (QSQLITE) vs `libsqlite3` directly?~~ → **Chốt:** [`docs/adr/0001-db.md`](../../adr/0001-db.md)
4. **Pain points** từ `07-manual-test-with-hardware.md` failures → điền vào Phase 2 ADR risks.
