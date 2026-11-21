# Material 3 — Component guidelines (Central Logger)

SoT component chung: [`shared/logger-ui-kit/README.md`](../../shared/logger-ui-kit/README.md)
(kit dùng chung với data-logger — không fork, không copy local).
Chiến lược M3: [`docs/plan/chien_luoc_material3_cho_qt_desktop.md`](../plan/chien_luoc_material3_cho_qt_desktop.md)

Palette: **Teal** primary, **Indigo** accent; light/dark qua `ThemeToggle` trên rail
(`SettingsController.theme` → bind `ThemeMode.mode` trong `Main.qml`). No dynamic color.

## Shell layout

```
ApplicationWindow
└── SplitView (horizontal)
    ├── AppNavigationRail (AppTheme.railWidth = 80)
    └── ColumnLayout
        ├── AppTopBar (AppTheme.topBarHeight = 80)
        └── Loader (views)
```

## Tokens (`LoggerKit.Theme`)

| Token | QML | Typical use |
|-------|-----|-------------|
| App canvas | `AppColors.surface` | Window, rail, top bar |
| Card / pane | `AppColors.surfaceContainerLow` + `elevatedBorder` + `AppTheme.cardRadius` | `ElevatedPane`, `StatCard` |
| Table header | `AppColors.surfaceContainerHigh` | `TableHeaderCell` |
| Body text | `AppColors.primaryText` | Default text, icons (theme-reactive) |
| Muted text | `AppColors.onSurfaceVariant` | Labels, secondary |
| Semantic | `AppColors.success` / `error` / `warning` / `info` | Status, alarms |
| Shape | `AppTheme.cardRadius` (12), `chipRadius` (12), `listItemRadius` (8) | Cards, chips, list rows |
| Spacing | `AppTheme.spacingXS…spacingL`, `sectionSpacing`, `cardPadding` | Thay spacing số |

**Rule:** Data tables và chart blocks nằm trong `ElevatedPane`. Shell (rail, top bar)
nằm phẳng trên `AppColors.surface`.

## Components

Chung từ kit (`import LoggerKit.Components` / `import LoggerKit.Theme`): `AppButton` (kind enum), `ThemedTabButton`, `UiIcon`,
`MaterialIcons`, `ElevatedPane`, `EmptyStatePlaceholder`, `TableContentStack`,
`AppScrollBar`, `AppTableView`, `TableHeaderCell`, `TableCellBackground`,
`AppNotifier`, `AppToastHost`, `MessageDetailDialog`, `DateField`, `DatePickerPopup`,
`ChartGraphsTheme`, `ChartGraphsView`, `ChartLinePointMarker`, `StatusChip` (generic),
`SensorValueCard`, `IoColors`, `ClipboardService`.

App-specific (`import CentralLogger.Components`):

| Folder | Contents |
|--------|----------|
| `shell/` | `NavItem`, `AppNavigationRail`, `AppTopBar`, `RailCircleButton`, `ThemeToggle`, `WindowControls` |
| `layout/` | `StatCard`, `SectionHeader`, `InlineBanner`, `AlertDialog`, `FormNotice` |
| `status/` | `SensorStatusChip` (domain wrapper trên kit `StatusChip`: OperationalStatus + AttachDiType), `SensorStatusColumn` |
| `chart/` | `ChartTimeSeriesPanel` (bind `timezoneId` từ `SettingsController.systemTimezone`), `ChartHoverTooltip`, `SensorMiniChart` |
| `logger/` | `LoggerLiveSection`, `LiveSensorListTable`, `RecentEventListItem` |
| `settings/` | `SettingsGeneralPane`, `LoggersSettingsPane`, `SensorThresholdsPane` |

Điều hướng chính trên rail gồm 4 đích: `Dashboard` · `Loggers` (`Data | Graph`, nhớ `Cards`/`List` qua `app_settings.loggers_view_mode`) · `History` · `Settings` (`General | Loggers`).

Domain singletons (`CentralLogger.Theme`): `OperationalStatus`, `AttachDiType`.

## Notification policy

| Context | Toast | Form inline | Recent events |
|---------|-------|-------------|---------------|
| Reload config | Toast success/error | `InlineBanner` khi lỗi | Info row khi OK |
| Settings save OK/fail | Toast success/error | — | — |
| Form Save fail / Connect fail/OK | Toast khi Save OK | `FormNotice` | Info row (khi Save OK) |

- `AppNotifier.show(summary, semantic, options)` — `options: { detailText, detailTitle,
  contextId, copyPath, durationMs }`. `contextId` thay cho `loggerId` cũ (generic).
- `MessageDetailDialog` (kit): đặt 1 lần trong `Main.qml`, set
  `contextActionText: qsTr("Open logger")` + `onContextActionRequested: id => openLoggerGraph(id)`.
- Không hiện toast khi `AppNotifier.suppressed` (modal dialogs tự set).
- Report download & `/readings` debug không có UI người dùng (chỉ giữ hàm C++ chẩn đoán ngầm cho dev).

## Recent events click policy

| `eventType` | Action |
|-------------|--------|
| `Online`, `Offline`, `Info` (audit) | `loggerId > 0` → mở `Loggers > Graph` của logger đó (`openLoggerGraph(loggerId)`) |
| Message bắt đầu `Report saved:` | **Copy path** vào clipboard (không navigate) |
| `Warning`, `Error`, `Alarm` | `MessageDetailDialog` (+ "Open logger" mở `Loggers > Graph` nếu `loggerId > 0`) |

## Charts (`QtGraphs`)

`ChartGraphsView` (kit) + `timezoneId: SettingsController.systemTimezone`;
hover qua `ChartHoverTooltip` + `ChartLinePointMarker` (`pointDelegate`).
Series colors từ `AppColors.graphSeriesColors` (light/dark reactive).

## Manual QA (light + dark)

Toggle theme từ rail `ThemeToggle`:

- [ ] **Dashboard** — 3 `StatCard`, biểu đồ `Traffic Readings`, `Recent events` (click mở `Loggers > Graph`)
- [ ] **Loggers > Data** — toggle `Cards`/`List` (nhớ qua lần mở app), `LoggerLiveSection` (`SensorValueCard` 4 cột × 2 hàng cho 8 analog, mờ + `—` khi offline), `LiveSensorListTable`
- [ ] **Loggers > Graph** — bộ chọn `"Show graph"`, lưới `SensorMiniChart`, bảng `"Other loggers"`
- [ ] **Settings > General** — elevated form pane, Outlined inputs, Diagnostics copy log path
- [ ] **Settings > Loggers** — bố cục 3 cột hoặc xếp dọc khi hẹp, form Central sửa được khi offline, `SensorThresholdsPane` luôn chỉ xem, Reload không ghi đè tên/chu kỳ Central
- [ ] **Rail** — 4 đích, `NavItem` pill active dùng accent container tokens
