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

Chung từ kit (`import LoggerKit.Components`): `AppButton` (kind enum), `UiIcon`,
`MaterialIcons`, `ElevatedPane`, `EmptyStatePlaceholder`, `TableContentStack`,
`AppScrollBar`, `AppTableView`, `TableHeaderCell`, `TableCellBackground`,
`AppNotifier`, `AppToastHost`, `MessageDetailDialog`, `DateField`, `DatePickerPopup`,
`ChartGraphsTheme`, `ChartGraphsView`, `ChartLinePointMarker`, `StatusChip` (generic),
`ClipboardService`.

App-specific (`import CentralLogger.Components`):

| Folder | Contents |
|--------|----------|
| `shell/` | `NavItem`, `AppNavigationRail`, `AppTopBar`, `RailCircleButton`, `ThemeToggle`, `WindowControls` |
| `layout/` | `StatCard`, `SectionHeader`, `InlineBanner`, `AlertDialog`, `FormNotice` |
| `status/` | `SensorStatusChip` (domain wrapper trên kit `StatusChip`: OperationalStatus + AttachDiType), `SensorStatusColumn` |
| `chart/` | `ChartTimeSeriesPanel` (bind `timezoneId` từ `SettingsController.systemTimezone`), `ChartHoverTooltip` |
| `logger/` | `LoggerFormDialog`, `RecentEventListItem` |

Domain singletons (`CentralLogger.Theme`): `OperationalStatus`, `AttachDiType`.

## Notification policy

| Context | Toast | Form inline | Recent events |
|---------|-------|-------------|---------------|
| Report download **OK** | Yes — `"success"`, tap **Copy path** (`ClipboardService`) | — | Info row |
| Report download **fail** | Yes — `"error"`, tap → detail dialog (`contextId` = loggerId) | — | Warning row |
| Config push **fail** | Yes — `"error"` + `contextId` | — | Warning row |
| Settings save OK/fail | Toast success/error | — | — |
| Form Save fail / Connect fail/OK | **No** | `FormNotice` | — |

- `AppNotifier.show(summary, semantic, options)` — `options: { detailText, detailTitle,
  contextId, copyPath, durationMs }`. `contextId` thay cho `loggerId` cũ (generic).
- `MessageDetailDialog` (kit): đặt 1 lần trong `Main.qml`, set
  `contextActionText: qsTr("Open logger")` + `onContextActionRequested: id => selectLogger(id)`.
- Không hiện toast khi `AppNotifier.suppressed` (modal dialogs tự set).

## Recent events click policy

| `eventType` | Action |
|-------------|--------|
| `Online`, `Offline`, `Info` (audit) | `loggerId > 0` → Logger Detail |
| Message bắt đầu `Report saved:` | **Copy path** vào clipboard (không navigate) |
| `Warning`, `Error`, `Alarm` | `MessageDetailDialog` (+ "Open logger" nếu `loggerId > 0`) |

## Charts (`QtGraphs`)

`ChartGraphsView` (kit) + `timezoneId: SettingsController.systemTimezone`;
hover qua `ChartHoverTooltip` + `ChartLinePointMarker` (`pointDelegate`).
Series colors từ `AppColors.graphSeriesColors` (light/dark reactive).

## Manual QA (light + dark)

Toggle theme từ rail `ThemeToggle`:

- [ ] **Dashboard** — StatCards, panes, chart plot background khớp pane
- [ ] **Loggers** — table trong elevated card, Outlined search, Add Logger primary CTA
- [ ] **Logger Detail** — panes, SensorStatusChip (operational + attach-DI), wide/narrow 950px
- [ ] **Settings** — elevated form pane, Outlined inputs
- [ ] **LoggerFormDialog** — rounded dialog, Outlined fields
- [ ] **Rail** — NavItem pill active dùng accent container tokens
