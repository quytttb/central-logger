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

## Cấu trúc Loggers & Settings (refactor 2026-11-18 — đã xong)

Plan của refactor đã archive:
[`docs/archive/plan/loggers-data-graph-refactor.md`](docs/archive/plan/loggers-data-graph-refactor.md).
Kết quả: **Loggers `Data | Graph`**, cấu hình logger trong
**Settings > Loggers** (3 cột: danh sách logger + form inline + `SensorThresholdsPane`).
Từ 2026-11-21, bảng ngưỡng chỉ đọc.

- Plan: [`docs/archive/plan/loggers-data-graph-refactor.md`](docs/archive/plan/loggers-data-graph-refactor.md)
- Checklist: [`docs/archive/plan/loggers-data-graph-todo.md`](docs/archive/plan/loggers-data-graph-todo.md)
- Nhật ký: [`docs/archive/walkthrough/loggers-data-graph.md`](docs/archive/walkthrough/loggers-data-graph.md)
- Rail gồm 4 đích (`Dashboard · Loggers · History · Settings`); không còn `LoggerDetailView`, `ThresholdsView`, `LoggerFormDialog` hay Dashboard `Summary`.
- Giữ nhãn “Show graph”, lưu chế độ `Cards`/`List` trong SQLite schema v8 (`app_settings.loggers_view_mode`). Ngoài phạm vi đã duyệt → hỏi user.
- Giữ tách biệt các thay đổi quản lý tiến trình OSG / đóng gói Windows chưa commit khỏi refactor.

## UI/UX polish + fix vận hành (2026-11-20)

Đã triển khai reconnect Modbus, Reload config tại Settings > Loggers, card
filled, Graph zoom RAM/SQLite, History không còn export theo ca và UI tiếng
Anh. Kiểm thử tự động đã đạt; còn nghiệm thu với logger vật lý.

- Plan: [`docs/archive/plan/uiux-polish.md`](docs/archive/plan/uiux-polish.md)
- Tiến độ: [`docs/archive/plan/uiux-polish-todo.md`](docs/archive/plan/uiux-polish-todo.md)
- Nhật ký: [`docs/archive/walkthrough/uiux-polish.md`](docs/archive/walkthrough/uiux-polish.md)
- Không đổi contract edge, không bump schema DB, không thêm setting `auto_reconnect`.
- Chuỗi tiếng Việt trong CSV export (`Thời gian`, `Giá trị trung bình 24h`) giữ nguyên; UI dùng English.

## Central chỉ đọc (2026-11-21)

- **Cấm** ghi cấu hình hoặc điều khiển logger từ Central (REST, Modbus),
  kể cả Data Logger gốc có `thresholds_writable: true`. Adapter OSG cũng
  là nguồn chỉ đọc đối với Central. Không đổi wire contract hay schema DB.
- **Add** đọc `GET /config` trước khi lưu cục bộ. **Edit/Save** chỉ thay đổi
  cài đặt Central (tên hiển thị, kết nối, chu kỳ đọc, bật/tắt giám sát, ghi chú).
  **Reload config** trong Settings đọc và làm mới catalog/revision, không ghi đè form.
- Ngưỡng chỉ xem ở Central. Edge điều khiển relay DO và báo cáo TT10.
  Central dùng alarm bit từ Modbus làm tín hiệu cảnh báo, độc lập với status TT10.
- **Cấm** thêm cột/nguồn ngưỡng mới trong DB Central mà không sửa contract.
- [Plan](docs/plan/central-read-only.md) · [TODO](docs/plan/central-read-only-todo.md) · [Walkthrough](docs/walkthrough/central-read-only.md).

## Shared UI kit (`shared/logger-ui-kit` — submodule)

- Token M3 + component generic nằm trong **`LoggerKit.Theme` / `LoggerKit.Components`**
  (git submodule, static QML modules). **Cấm** copy/duplicate component về local.
- App chỉ giữ component đặc thù trong `CentralLogger.Components` (rail/topbar,
  StatCard, SensorStatusChip wrapper, LoggerLiveSection…) + singleton domain
  `OperationalStatus`/`AttachDiType` trong `CentralLogger.Theme`.
- Card giá trị cảm biến (`SensorValueCard`), `ThemedTabButton`, `IoColors` **phải**
  nằm trong kit — dùng chung với `data-logger`, không fork bản local.
- Theme mode bind 1 lần trong `Main.qml`: `ThemeMode.mode = Qt.binding(() => SettingsController.theme)`.
- Đổi API của kit ⇒ update cả central_logger và data-logger (commit kit trước, bump submodule pointer sau).
- UI guideline: [`docs/ui/material3-component-guidelines.md`](docs/ui/material3-component-guidelines.md).

## Architecture rules

- MVVM: business logic in C++ ViewModels/Services, not in QML
- No `.pragma library` `.js` for business logic (see `06-qml-logic-inventory.md` trong THAM_KHAO)
- Schema DB: **only** [`docs/thiet_ke_db.md`](docs/thiet_ke_db.md)
- Persistence: **SQLite** via **`QSqlDatabase` / `QSQLITE`** (`Qt6::Sql`) — [`docs/adr/0001-db.md`](docs/adr/0001-db.md); repositories dùng `QSqlQuery`; không `libsqlite3` trực tiếp
- Modbus: `ModbusService` poll FC03 → FC02 → FC01 per [`modbus-map-v1.md`](docs/contracts/modbus-map-v1.md)
- REST: `GET /config`; `GET /readings` **debug only** (one-shot, raw JSON — never merge into live table)
- QML module via `qt_add_qml_module`; register types with `QML_ELEMENT` / `QML_SINGLETON`
- Prefer manual composition root (`ApplicationContext`) + interface boundaries for tests — no DI framework

## MVP scope

**MVP Core (repo này — đã implement):** FE-001 … FE-016, **trừ FE-013/014** (không ghi cấu hình, không merge `/readings` live). Prompt lịch sử: [`docs/archive/plan/tasks-6-24-agent-prompts.md`](docs/archive/plan/tasks-6-24-agent-prompts.md); **Task 18** đã làm lại một nửa (xem FE-017 bên dưới).

| FE | Trạng thái |
|----|------------|
| FE-001 … FE-012, FE-015, FE-016 | Done (Tasks 1–16, 19) |
| FE-004 (search) | Done (Task 17 — `LoggerSearchProxyModel`) |
| FE-013 | **Removed** — Central không apply config |
| FE-014 | **Out of scope** — `/readings` chỉ debug |
| FE-017 (tray + frameless) | **Frameless: đã implement** (chốt 2026-08 theo audit H-D/M-9); **system tray: không làm** — Close thoát app |

**FE-017 đã chốt lại (2026-08-17):** cửa sổ **frameless** là chuẩn hiện tại — `Main.qml` đặt `Qt.FramelessWindowHint` (non-Windows), Windows dùng `utils/os/WindowsFramelessHelper` (native event filter: `WM_NCCALCSIZE` bỏ title bar, `WM_NCHITTEST` giữ viền resize) + mở maximized mặc định; **không dùng** `QSystemTrayIcon`, Close vẫn thoát app (không minimize-to-tray).

**Nice-to-have (Tasks 20–24):** Done trừ các task dropped. **Dropped:** Task **21** (FE-020 QR). Task **18** (FE-017): tray vẫn không làm, frameless đã implement — xem hàng FE-017 ở trên. Khảo sát cũ ghi FE-001…FE-017 — **khi mâu thuẫn, theo `HANDOFF.md`.**

## Testing

Write **new** Qt Test / CTest in this repo — do not reference legacy pytest.

## CMake

- Qt 6.11 (`qt_standard_project_setup(REQUIRES 6.11)`)
- `find_package` Qt6 components: Quick, Qml, Sql, Network, **SerialBus** (Modbus FC01/02/03), **Graphs** (thay Qt Charts — deprecated 6.11)
- Enable `CMAKE_AUTOMOC`, `CMAKE_AUTORCC` when adding C++ types
- CMake target `app` in `src/app` (binary `central_logger`); static lib `data` in `src/data` (alias `central_logger::data`)
- Bump `QT_VERSION_REQUIRED` / `QML_MODULE_VERSION` in root `CMakeLists.txt` — they propagate to `find_package`, `qt_standard_project_setup`, and every `qt_add_qml_module(VERSION ...)`. Patch versions in packaging scripts must match.

## Constants — single source of truth

All magic numbers and duplicated string literals live under `src/utils/`. When
adding a new constant, put it in the right header and include it. Don't
hardcode in source.

| Header | What |
|--------|------|
| `utils/AppConstants.h`     | Operational defaults — `CentralLogger::Defaults::*` (ports, intervals, batch sizes, decimals, log rotation, retention) |
| `utils/Version.h`          | Protocol / schema versions — `CentralLogger::Version::*` (DB schema, REST API, Modbus map) |
| `utils/DbConstants.h`      | SQL table + bind-param names — `CentralLogger::Data::Db::*` |
| `utils/UiConstants.h`      | QML role names + chart payload keys — `CentralLogger::Ui::*` |
| `utils/FormatConstants.h`  | Date/time formats + reusable error messages — `CentralLogger::Format::*` |
| `utils/SensorConstants.h`  | Sensor / status / alarm type / event level strings — `CentralLogger::Sensor::*` |

QML-side defaults come from the `AppDefaults` QML singleton in
`src/core/AppDefaults.{h,cpp}` (auto-generated, registered as
`CentralLogger.Core.AppDefaults`); QML code should reference
`AppDefaults.modbusPort`, `AppDefaults.chartDisplayPointCount`, etc.
instead of hardcoded literals.
