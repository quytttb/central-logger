# Agent instructions — Central Logger C++

## Context

Greenfield **Qt 6 + C++ + QML** desktop app. Behavior tham khảo app PySide6 cũ — **không** copy source hay tests Python.

## Read first (bắt buộc)

1. [`HANDOFF.md`](HANDOFF.md)
2. [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md) — schema DB + RAM (spec chính thức)
3. [`docs/README.md`](docs/README.md)
4. [`docs/contracts/`](docs/contracts/) — REST, Modbus (FC01/02/03), QR **frozen v1**

## Repo greenfield

- **Không** có `docs/phase2/` hay quy trình phase của repo PySide6 cũ.
- Implement theo `docs/contracts/` + `docs/thiet_ke_db.md` từ đầu.

## Tham khảo repo cũ (tuỳ chọn — không phải SoT)

[`docs/THAM_KHAO_REPO_CU/phase1/`](docs/THAM_KHAO_REPO_CU/phase1/) — khảo sát app `central-logger-app` (chỉ markdown, **không** port code):

| File | Mục đích |
|------|----------|
| `01-feature-matrix.md` | Danh sách FE (khảo sát cũ); **MVP repo này = FE-001…FE-016** — xem mục MVP scope bên dưới |
| `03-scope-document.md` | Users, in/out scope |
| `05-python-cpp-migration-map.md` | Gợi ý `src/` layout — **điều chỉnh theo contracts hiện tại** |
| `06-qml-logic-inventory.md` | ViewModel methods (thay `.js` QML cũ) |
| `02-pain-points.md`, `04-non-functional-requirements.md` | Bài học + baseline NFR |
| `07-manual-test-with-hardware.md` | Test hardware |

**Khi mâu thuẫn:** `docs/contracts/` + `docs/thiet_ke_db.md` **thắng** (vd. DI/DO qua **FC02/FC01**, không float holding; không poll `/readings` live).

**Cấm:** port Python, pytest cũ, hoặc coi Phase 1 contracts links trong `phase1/README.md` là đường dẫn chuẩn (contracts nằm ở `docs/contracts/`).

## Frozen contracts

- [`docs/contracts/rest-config-contract-v1.md`](docs/contracts/rest-config-contract-v1.md)
- [`docs/contracts/provision-qr-v1.md`](docs/contracts/provision-qr-v1.md)
- [`docs/contracts/modbus-map-v1.md`](docs/contracts/modbus-map-v1.md) — FC03 analog + FC02 DI + FC01 DO

Do not change edge API without explicit user approval.

## Architecture rules

- MVVM: business logic in C++ ViewModels/Services, not in QML
- No `.pragma library` `.js` for business logic (see `06-qml-logic-inventory.md` trong THAM_KHAO)
- Schema DB: **only** [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md)
- Persistence: **SQLite** via **`QSqlDatabase` / `QSQLITE`** (`Qt6::Sql`) — [`docs/adr/0001-db.md`](docs/adr/0001-db.md); repositories dùng `QSqlQuery`; không `libsqlite3` trực tiếp
- Modbus: `ModbusService` poll FC03 → FC02 → FC01 per [`modbus-map-v1.md`](docs/contracts/modbus-map-v1.md)
- REST: config + reports; `GET /readings` **debug only** (one-shot per button, raw JSON — never merge into live table)
- QML module via `qt_add_qml_module`; register types with `QML_ELEMENT` / `QML_SINGLETON`
- Prefer manual composition root (`ApplicationContext`) + interface boundaries for tests — no DI framework

## MVP scope

**MVP Core (repo này — đã implement):** FE-001 … FE-016, **trừ FE-014** (không merge `/readings` live) và **trừ FE-017** (xem dưới). Mapping task: Tasks **1–17** + **19** trong [`docs/plan/tasks-6-24-agent-prompts.md`](docs/plan/tasks-6-24-agent-prompts.md); **Task 18** không làm.

| FE | Trạng thái |
|----|------------|
| FE-001 … FE-013, FE-015, FE-016 | Done (Tasks 1–16, 19) |
| FE-004 (search) | Done (Task 17 — `LoggerSearchProxyModel`) |
| FE-014 | **Out of scope** — `/readings` chỉ debug |
| FE-017 (tray + frameless) | **Deferred / dropped** — giữ `ApplicationWindow` chuẩn OS (đóng = thoát app) |

**Không implement FE-017** trừ khi user yêu cầu lại: không `QSystemTrayIcon`, không `Qt.FramelessWindowHint`, không ẩn cửa sổ xuống tray khi Close.

**Nice-to-have (Tasks 20–24):** Done trừ các task dropped. **Dropped:** Task **18** (FE-017), Task **21** (FE-020 QR). Còn lại chủ yếu manual hardware test (Task 4). Khảo sát cũ ghi FE-001…FE-017 — **khi mâu thuẫn, theo `HANDOFF.md`.**

## Testing

Write **new** Qt Test / CTest in this repo — do not reference legacy pytest.

## CMake

- Qt 6.11 (`qt_standard_project_setup(REQUIRES 6.11)`)
- `find_package` Qt6 components: Quick, Qml, Sql, Network, **SerialBus** (Modbus FC01/02/03), **Graphs** (thay Qt Charts — deprecated 6.11)
- Enable `CMAKE_AUTOMOC`, `CMAKE_AUTORCC` when adding C++ types
- CMake target `app` in `src/app` (binary `central_logger`); static lib `data` in `src/data` (alias `central_logger::data`)
