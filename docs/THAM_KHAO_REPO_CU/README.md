# Tham khảo — khảo sát app cũ (PySide6)

Tài liệu trong thư mục này **không** phải source of truth cho repo C++ mới.  
Chỉ dùng để hiểu hành vi app legacy `central-logger-app` và danh sách feature.

Các mục “Handoff to Phase 2”, “Phase 2 ADR” trong `phase1/` là **quy trình dự án cũ** — repo Qt mới **không** có `docs/phase2/` và không bắt buộc làm theo các phase đó.

## Spec chính thức (repo mới)

| Tài liệu | Path |
|----------|------|
| DB + RAM | [`../thiet_ke_db.md`](../thiet_ke_db.md) |
| Handoff | [`../../HANDOFF.md`](../../HANDOFF.md) |
| Contracts v1 | [`../contracts/`](../contracts/) |

## Phase 1 (khảo sát)

[`phase1/README.md`](phase1/README.md) — feature matrix, pain points, NFR, migration map, QML logic inventory.

**Lưu ý:** Một số mô tả trong Phase 1 (REST `/readings` cho DI/DO, holding float cho digital) **đã lỗi thời** so với [`../contracts/modbus-map-v1.md`](../contracts/modbus-map-v1.md) và [`../contracts/rest-config-contract-v1.md`](../contracts/rest-config-contract-v1.md). Khi implement C++ → theo **contracts + thiet_ke_db**.

## Legacy app

**Không** copy source Python hay pytest — chỉ đọc markdown spec.

### Legacy QML reference (UI layout only)

Repo PySide6 cũ ở ngoài workspace, dùng để đối chiếu **layout** sidebar/top bar /
opacity crossfade khi viết QML mới (đã port pattern vào Task 2):

- [`/home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/main.qml`](file:///home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/main.qml)
- [`/home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/components/navigation/`](file:///home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/components/navigation)
- [`/home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/views/`](file:///home/haiquy/Documents/Projects/central-logger-app/src/central_logger/ui/views)

**Không** port: `Colors.qml`, `UiMotion.qml`, `*.js` logic, `LoggerFormDialog`, charts, tray.
Task 2 dùng Material light/dark thẳng từ `QtQuick.Controls.Material` thay cho `Colors`.