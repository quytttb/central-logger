# Phase 1 — Feature Survey & Analysis (Central Logger)

## Mục đích

Bộ tài liệu này là đầu ra của **Giai đoạn 1: Khảo sát & Phân tích Chức năng** trước khi viết lại sang Qt 6 C++/QML.
Mục tiêu: team mới đọc xong không cần đoán hành vi, dữ liệu, hoặc edge case.

**Baseline khảo sát (2026-05-23):**

| Chỉ số | Giá trị |
|--------|---------|
| Python | 3.14.4 |
| PySide6 / Qt | 6.11.1 |
| OS (dev) | Linux 7.0.0-15-generic x86_64 |
| App version | 0.1.11 |
| Test suite | **104 passed** in 1.47 s (headless) |
| Ingestion chart bench | avg **2.3 ms** @ 100k rows (min 1.9, max 3.6) |
| Headless launch | Clean (timeout-killed at 8 s, no traceback) |

---

## Deliverables

| File | Nội dung |
|------|----------|
| [01-feature-matrix.md](01-feature-matrix.md) | Bảng ~30 chức năng (song ngữ): flow, UI, backend, DB, deps, priority, pain point, ghi chú C++ |
| [02-pain-points.md](02-pain-points.md) | Điểm đau kiến trúc và vận hành (≥8 mục, mỗi mục có evidence) |
| [03-scope-document.md](03-scope-document.md) | Tài liệu phạm vi MVP 1-2 trang: users, in-scope, out-of-scope, risks, DoD |
| [04-non-functional-requirements.md](04-non-functional-requirements.md) | NFR có số đo baseline (performance, reliability, platform, security…) |
| [05-python-cpp-migration-map.md](05-python-cpp-migration-map.md) | Map Python module → đề xuất C++ layer (Model/Service/ViewModel) theo payoff |
| [06-qml-logic-inventory.md](06-qml-logic-inventory.md) | Liệt kê đầy đủ mọi function trong 2 file `.js`; ghi chú chuyển sang C++ |
| [07-manual-test-with-hardware.md](07-manual-test-with-hardware.md) | Checklist test với thiết bị thật (10 scenario, fill Actual + Pass khi chạy) |

---

## Contracts (frozen v1 — spec repo C++ mới)

Hợp đồng edge **không** nằm trong `phase1/` — dùng bản đã cập nhật:

| Tài liệu | Path |
|----------|------|
| REST config v1 | [`../../contracts/rest-config-contract-v1.md`](../../contracts/rest-config-contract-v1.md) |
| QR provision v1 | [`../../contracts/provision-qr-v1.md`](../../contracts/provision-qr-v1.md) |
| Modbus map v1 (FC03/02/01) | [`../../contracts/modbus-map-v1.md`](../../contracts/modbus-map-v1.md) |

Phase 1 có thể vẫn nhắc REST `/readings` hoặc holding cho DI/DO — **bỏ qua khi code**; theo contracts ở `docs/contracts/`.

Legacy app còn có `smoke-validation.md` / `perf-baseline.md` trên repo PySide6 — không copy sang đây.

---

## Handoff sang Giai đoạn 2

Khi Phase 1 Done (tất cả DoD trong [03-scope-document.md](03-scope-document.md) ✓):

1. Copy hoặc submodule `docs/phase1/` sang repo C++ mới.
2. Đính kèm `01-feature-matrix.md` + `05-python-cpp-migration-map.md` vào session thiết kế kiến trúc.
3. Dùng `07-manual-test-with-hardware.md` failures để điền vào Phase 2 ADR risks.
4. `06-qml-logic-inventory.md` → danh sách C++ ViewModel methods cần implement.
