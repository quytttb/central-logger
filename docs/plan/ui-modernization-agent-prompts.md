# UI modernization — Agent prompts (thay pattern cũ)

Tài liệu này thay thế các **lựa chọn / pattern cũ** đã được audit (2026-05), không đụng MVP Tasks 6–24. Mỗi task **độc lập**; giao **một task mỗi session**.

## Trạng thái (repo hiện tại)

| Task | Trạng thái |
|------|------------|
| UI-M1 | **Done** — `Main.qml` `Loader` + `Component` per view |
| UI-M2 | **Done** — `LoggerDetailView` `HorizontalHeaderView` + `TableView` / `SensorMonitoringTableModel` |
| UI-M3 | **Done** — `LoggersView` `TableView` (ban đầu Option 1 full-row) |
| UI-M3b | **Done** — `LoggerListModel` → `QAbstractTableModel` (7 cột), header từ `headerData()`; `loggerId` role; proxy `beginFilterChange`/`endFilterChange` (Qt 6.11) |

Header cột bảng: `tr()` trong C++ (`LoggerListModel`, `SensorMonitoringTableModel`). Cột Status loggers: `DisplayRole` = Online/Offline (`tr()`), QML bind `display`.

## Pattern cũ → thay bằng

| Cũ (hiện tại) | Vấn đề | Thay bằng (task) |
|---------------|--------|------------------|
| `Main.qml`: 4 view chồng nhau, `opacity` crossfade, luôn trong cây QML | RAM/CPU; comment “legacy pattern” | **UI-M1** — `Loader` lazy (hoặc `StackView` + transition ngắn) |
| `LoggerDetailView`: `ListView` + header `Pane` thủ công cho sensor | `SensorMonitoringTableModel` đã `QAbstractTableModel` + `headerData` | **UI-M2** — `TableView` + `columnWidthProvider` |
| `LoggersView`: `ListView` + header thủ công | Cột nhiều, search proxy; UX bảng kém | **UI-M3** — `TableView` + proxy (delegate hàng hoặc mở rộng model) |

**Không làm trong các task này:** Qt Charts, tray/frameless (FE-017), port Python, đổi contracts/DB, FE-014.

## Quy ước chung (mọi task)

- **SoT:** [`thiet_ke_db.md`](../thiet_ke_db.md), [`contracts/`](../contracts/), [`AGENTS.md`](../../AGENTS.md), [`HANDOFF.md`](../../HANDOFF.md)
- **MVVM:** logic C++; QML chỉ bind / layout
- **Imports QML:** `import QtQuick`, `import QtQuick.Controls.Material`, `import QtQuick.Layouts` (versionless Qt 6)
- **Charts:** giữ `QtGraphs` / `GraphsView` — không `QtCharts`
- **Không** `.js` / `.pragma library` cho nghiệp vụ
- **Không** commit trừ khi user yêu cầu
- **Gating:** `cmake --build build/Desktop-Debug` (hoặc preset của bạn) + `ctest` **12/12** pass; không regression Modbus/REST

## Bảng tra nhanh

| Task | Tên | File chính | Phụ thuộc |
|------|-----|------------|-----------|
| UI-M1 | Navigation lazy | `src/app/qml/Main.qml` | Task 2 shell |
| UI-M2 | Sensor `TableView` | `src/app/qml/views/LoggerDetailView.qml` | Task 7–9 |
| UI-M3 | Loggers `TableView` | `src/app/qml/views/LoggersView.qml` | Task 17 search |
| UI-M3b | Loggers `QAbstractTableModel` + header sync | `LoggerListModel.*`, `LoggersView.qml` | UI-M3 |

**Thứ tự gợi ý:** UI-M1 → UI-M2 → UI-M3 → UI-M3b (đã xong trong repo).

## Cách giao agent mỗi lần

```
Implement UI-M? as specified in docs/plan/ui-modernization-agent-prompts.md.
Do NOT edit any .plan.md files.
Requirements: docs/thiet_ke_db.md, docs/contracts/, AGENTS.md, HANDOFF.md.
Build and ctest must pass before done.
No FE-014: never merge GET /readings into live UI.
Do NOT reintroduce Qt Charts, QSystemTrayIcon, or frameless window.

[Paste block "Prompt giao agent" của UI-M? bên dưới]
```

---

## UI-M1 — Navigation: bỏ opacity stack, dùng Loader lazy

**Thay thế:** `Main.qml` lines ~68–112 — bốn view (`DashboardView`, `LoggersView`, `LoggerDetailView`, `SettingsView`) cùng `anchors.fill`, chỉ đổi `opacity`/`visible`/`enabled`/`z`.

### Mục tiêu

- Chỉ **một** view content active trong memory tại một thời điểm (hoặc lazy-create lần đầu và giữ instance — chấp nhận nếu document rõ).
- Giữ API navigation hiện có: `currentView`, `navigate()`, `selectLogger()`, sidebar + top bar không đổi hành vi.
- `LoggerDetailView` vẫn nhận `loggerId: root.selectedLoggerId` khi vào `logger-detail`.

### Deliverables

| File | Nội dung |
|------|----------|
| `src/app/qml/Main.qml` | Thay stack opacity bằng một trong hai (ưu tiên **A**): |

**A — `Loader` (ưu tiên):**

```qml
// Ví dụ cấu trúc — agent chỉnh cho khớp repo
Item {
    id: contentHost
    Layout.fillWidth: true
    Layout.fillHeight: true

    Loader {
        id: viewLoader
        anchors.fill: parent
        active: true
        sourceComponent: {
            switch (root.currentView) {
            case "dashboard":     return dashboardComp
            case "loggers":       return loggersComp
            case "logger-detail": return loggerDetailComp
            case "settings":      return settingsComp
            default:               return dashboardComp
            }
        }
    }

    Component {
        id: dashboardComp
        DashboardView {
            anchors.fill: parent
            onSelectLogger: id => root.selectLogger(id)
        }
    }
    // ... tương tự loggersComp, loggerDetailComp (loggerId binding), settingsComp
}
```

- `Loader.active`: true khi `currentView` khớp; có thể `asynchronous: false` (desktop).
- **Không** giữ bốn child `anchors.fill` song song với `opacity: 0`.

**B — `StackView` (chấp nhận nếu giữ transition):**

- `replace()` / `push` theo `currentView`; transition ≤ 250 ms; không giữ depth history phức tạp (tab app, không browser back stack).

### Gating / kiểm tra thủ công

- [ ] Chuyển 4 tab sidebar: mỗi view hiển thị đúng, không ghost click view ẩn
- [ ] Loggers → chọn logger → Detail → Back về Loggers
- [ ] Theme light/dark vẫn áp dụng (`Material.theme` trên `ApplicationWindow`)
- [ ] `ctest` pass

### Out of scope

- Đổi tên route, thêm tab mới, tray/frameless
- Refactor ViewModels

### Prompt giao agent

```
UI-M1: Replace Main.qml opacity crossfade navigation with lazy Loader-based view switching.

Current anti-pattern: four views stacked with opacity/visible/z (see Main.qml ~68-112).
Target: one active Loader sourceComponent per currentView ("dashboard"|"loggers"|"logger-detail"|"settings").
Preserve navigate(), selectLogger(), AppNavigationRail, AppTopBar, Material theme on ApplicationWindow.
LoggerDetailView must still bind loggerId from root.selectedLoggerId.
Optional: StackView with short transition instead of Loader — document choice in commit message.
No business logic in QML JS files. No Qt Charts/tray/frameless.
SoT: AGENTS.md, HANDOFF.md. Build + ctest must pass. No FE-014.
```

---

## UI-M2 — Sensor monitoring: `ListView` → `TableView`

**Thay thế:** `LoggerDetailView.qml` — `ListView` + header `Pane`/`RowLayout` thủ công (~330–450).

**Model sẵn có:** `detailVm.sensorTable` → `SensorMonitoringTableModel` (`QAbstractTableModel`, 5 cột, `headerData()` — `src/core/sensors/SensorMonitoringTableModel.*`).

### Mục tiêu

- Dùng `TableView` (Qt 6, `import QtQuick`) gắn trực tiếp `model: detailVm.sensorTable`.
- Cột: ID, Name, Value, Unit, Status — width gợi ý: `60`, flexible name, `120`, `60`, `120` (điều chỉnh cho `isWide`).
- Giữ badge màu status (logic màu hiện trong delegate — chuyển sang cell Status).
- `clip: true`; chiều cao hàng ~40; Material hover nếu có.

### Gợi ý QML (agent hoàn thiện)

```qml
TableView {
    id: sensorTableView
    model: detailVm.sensorTable
    clip: true
    columnWidthProvider: function(column) {
        switch (column) {
        case 0: return 60
        case 1: return Math.max(120, width - 60 - 120 - 60 - 120 - 32)
        case 2: return 120
        case 3: return 60
        case 4: return 120
        default: return 80
        }
    }
    delegate: Rectangle {
        required property int row
        required property int column
        // Read display via model.* roles OR index — match SensorMonitoringTableModel roles
        implicitHeight: 40
        // ... per-column Label / status badge when column === 4
    }
}
```

- Có thể dùng `headerData` từ model cho nhãn cột (bỏ `Pane` header thủ công) **hoặc** `horizontalHeader` sync — ưu tiên bỏ duplicate header QML.
- Empty state / offline message **giữ** như hiện tại (ngoài `TableView`).

### Tests

- Không bắt buộc test QML mới; regression: `ctest` toàn repo.
- Nếu sửa C++ model: thêm/điều chỉnh `tests/core/test_sensor_monitoring_table_model.cpp` nếu đã có — không bắt buộc đổi API model.

### Out of scope

- Đổi `SensorMerger` / Modbus
- Trending `GraphsView` (giữ nguyên)

### Prompt giao agent

```
UI-M2: LoggerDetailView sensor table — migrate from ListView + manual header Pane to Qt 6 TableView.

Model: detailVm.sensorTable (SensorMonitoringTableModel, QAbstractTableModel, 5 columns, headerData in C++).
File: src/app/qml/views/LoggerDetailView.qml (~sensor table section).
Use columnWidthProvider; preserve status badge colors and monospace value column.
Remove duplicate manual column header row if TableView/headerData covers it.
Keep empty/offline labels outside the table. Do not change merge or Modbus logic.
No Qt Charts. Build + ctest pass. AGENTS.md + contracts unchanged. No FE-014.
```

---

## UI-M3 — Edge loggers list: `ListView` → `TableView`

**Thay thế:** `LoggersView.qml` — `ListView` `model: searchProxy` + header `Pane` (~82–220).

**Model:** `LoggerSearchProxyModel` → `DashboardController.loggers` (`LoggerListModel`, `QAbstractListModel` + roles: `stationCode`, `name`, `host`, …).

### Mục tiêu

- `TableView` với `model: searchProxy` (giữ Task 17 search).
- Click hàng → `selectLogger(model.id)` như `ItemDelegate.onClicked` hiện tại.
- Cột: Station, Name, Host, Modbus, Sensors, Status (+ vùng action Edit/Delete nếu đang có trong delegate).
- Header: dùng hàng header Material đồng bộ với `TableView` (bỏ `Pane` header trùng lặp nếu có thể).

### Chiến lược model (chọn một — ghi trong PR/commit)

| Cách | C++ | QML |
|------|-----|-----|
| **1 (ưu tiên, ít C++)** | Không đổi `LoggerListModel` | `TableView` + **một delegate full-row** `RowLayout` (tương đương ListView nhưng API TableView: selection, keyboard, header) |
| **2 (đầy đủ hơn)** | `LoggerListModel` → `QAbstractTableModel` + `headerData`, hoặc wrapper mỏng | `TableView` multi-cell delegate theo `column` |

Proxy `LoggerSearchProxyModel` phải tiếp tục lọc `stationCode` / `name` / `host`.

### Gating / kiểm tra thủ công

- [ ] Search box vẫn lọc danh sách
- [ ] Click hàng mở Logger Detail
- [ ] Edit / Delete / Add logger không regress
- [ ] `ctest` pass

### Out of scope

- CRUD API đổi contract REST
- QR / tray

### Prompt giao agent

```
UI-M3: LoggersView — migrate logger list from ListView to Qt 6 TableView.

Keep LoggerSearchProxyModel (searchProxy) filtering stationCode, name, host.
Preserve row click → selectLogger(id), Edit/Delete buttons, Add logger + LoggerFormDialog.
Prefer TableView with full-row delegate (no C++ change) unless you add QAbstractTableModel + headerData for true per-column cells — document choice.
Remove duplicate manual header Pane if TableView header covers columns.
File: src/app/qml/views/LoggersView.qml. No REST/Modbus/DB schema changes.
Build + ctest pass. No FE-014. No Qt Charts/tray.
```

---

## Prompt tổng hợp (một session — chỉ khi bạn muốn gộp)

Dùng khi một agent làm cả ba; rủi ro diff lớn — **khuyến nghị tách UI-M1 / M2 / M3**.

```
UI modernization (UI-M1 + UI-M2 + UI-M3): docs/plan/ui-modernization-agent-prompts.md.

1) Main.qml: replace opacity crossfade with Loader (or StackView) — one active view at a time.
2) LoggerDetailView: sensor ListView → TableView on SensorMonitoringTableModel.
3) LoggersView: logger ListView → TableView on searchProxy; keep search + row actions.

Do not edit plan files. AGENTS.md, HANDOFF.md, contracts, thiet_ke_db unchanged.
Build + ctest 12/12. No FE-014, no Qt Charts, no tray/frameless.
Report: files changed, manual nav/table checks, ctest output.
```

---

## Tham chiếu file (sau modernization)

| File | Pattern hiện tại |
|------|------------------|
| [`src/app/qml/Main.qml`](../../src/app/qml/Main.qml) | `Loader` + `Component` per route |
| [`src/app/qml/views/LoggerDetailView.qml`](../../src/app/qml/views/LoggerDetailView.qml) | `HorizontalHeaderView` + `TableView` / `SensorMonitoringTableModel` |
| [`src/app/qml/views/LoggersView.qml`](../../src/app/qml/views/LoggersView.qml) | `HorizontalHeaderView` + `TableView` 7 cột / `LoggerSearchProxyModel` |

**Đã đúng chuẩn — không đổi trong các task trên:** `QtGraphs` / `GraphsView`, `FileDialog` (`QtQuick.Dialogs`), `ApplicationWindow` OS chrome, `QGuiApplication`, `QML_ELEMENT`/`QML_SINGLETON`.

**Build hygiene (tùy chọn, không task riêng):** sau khi gỡ FE-017, `ninja -t clean` hoặc xóa `build/` nếu còn artifact `TrayManager` trong thư mục build cũ.
