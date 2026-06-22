# Material 3 — Component guidelines (Central Logger)

SoT strategy: [`docs/plan/chien_luoc_material3_cho_qt_desktop.md`](../plan/chien_luoc_material3_cho_qt_desktop.md)

Palette: **Teal** primary, **Indigo** accent; light/dark via rail `ThemeToggle` only. No dynamic color.

## Shell layout

```
ApplicationWindow
└── SplitView (horizontal)
    ├── AppNavigationRail (AppTheme.railWidth = 80)
    └── ColumnLayout
        ├── AppTopBar (AppTheme.topBarHeight = 80)
        └── Loader (views)
```

## Tokens (`CentralLogger.Theme`)

| Token | QML | Typical use |
|-------|-----|-------------|
| App canvas | `AppColors.surface` | Window, rail, top bar |
| Card / pane | `AppColors.surfaceContainerLow` + `elevatedBorder` + `AppTheme.cardRadius` | `ElevatedPane`, `StatCard` |
| Table header | `AppColors.surfaceContainerHigh` | `TableHeaderCell` |
| Table zebra | `AppColors.surfaceContainer` | `TableCellBackground` |
| Body text | `AppColors.primaryText` | Default stat value, icons (theme-reactive) |
| Muted text | `AppColors.onSurfaceVariant` | Labels, secondary |
| Semantic | `AppColors.success` / `error` / `warning` | Status, alarms |
| Shape | `AppTheme.cardRadius` (12), `chipRadius` (12), `listItemRadius` (8) | Cards, chips, list rows |

**Rule:** Data tables and chart blocks sit inside `ElevatedPane` on all views. Shell (rail, top bar) stays flat on `AppColors.surface`.

## Components (`CentralLogger.Components`)

Source layout under `src/components/` (import unchanged: `import CentralLogger.Components`):

| Folder | Contents |
|--------|----------|
| `shell/` | `AppButton`, `NavItem`, `AppNavigationRail`, `AppTopBar`, `UiIcon`, `MaterialIcons`, `ThemeToggle` |
| `layout/` | `ElevatedPane`, `SectionHeader`, `StatCard`, `InlineBanner`, `EmptyStatePlaceholder`, `TableContentStack`, `AppNotifier` (singleton), `AppToastHost`, `MessageDetailDialog`, `FormNotice` |
| `table/` | `TableHeaderCell`, `TableCellBackground` |
| `chart/` | `ChartGraphsView`, `ChartGraphsTheme`, `ChartTimeSeriesPanel`, `ChartHoverTooltip`, … |
| `status/` | `StatusChip`, `SensorStatusColumn` |
| `logger/` | `LoggerFormDialog`, `RecentEventListItem` |

| Component | Notes |
|-----------|--------|
| `ElevatedPane` | M3 outlined surface — `surfaceContainerLow`, `cardRadius`, `elevatedBorder`; default padding `AppTheme.sectionSpacing` |
| `SectionHeader` | `titleMedium` + optional trailing actions (refresh icons) |
| `AppButton` | Single M3 button for all actions: `kind` (Primary/Secondary/Tonal/Neutral/Error/Text), `iconName` + `text` (either or both), `iconOnly`, `controlSize`, `iconSide`, `tooltipText`, `enabled`. Icon-only buttons use `iconOnly: true` + a filled `kind` (e.g. `Neutral`) so every button keeps a background. Excludes rail buttons (`RailCircleButton`/`NavItem`). |
| `NavItem` | Rail pill 56×32; `accentContainer` / `onAccentContainer` when active |
| `StatCard` | KPI card; label uses `AppTypography.overline` |
| `TableHeaderCell`, `TableCellBackground` | Table chrome |
| `TableContentStack` | `hasData` → children; else `EmptyStatePlaceholder` (tables, charts, lists) |
| `ChartGraphsTheme` | Plot + view background `surfaceContainerLow`; series from `AppColors.graphSeriesColors` |
| `ChartHoverTooltip` | Snap-to-point hover (`snapAt`); tooltip follows cursor in plot |
| `ChartLinePointMarker` | Circular markers on `LineSeries` (`pointDelegate`) |
| `EmptyStatePlaceholder` | Icon above, message below, centered |
| `LoggerFormDialog` | `ExtraLargeScale`; inputs `Outlined`; `FormNotice` inside for connect/save errors |
| `AppNotifier` | **Singleton** (`pragma Singleton`). Call `AppNotifier.show(summary, semantic, options)`. `options`: `{ copyPath, detailText, detailTitle, loggerId, durationMs }`. `copyPath` → toast tap copies path to clipboard. |
| `AppToastHost` | Custom `Popup`-based toast. Tap **Copy path** when `copyPath` set, or **Details** → `MessageDetailDialog`. Place once in `Main.qml`. |
| `DesktopService` | `copyToClipboard(text)`, `reportSavedMessagePrefix()` for Recent events rows. |
| `MessageDetailDialog` | Modal `Dialog` for full error/info text (selectable). Call `showMessage(title, body, loggerId)`. Shows "Mở logger" button when `loggerId >= 0`. Emits `navigateToLogger(id)`. |
| `FormNotice` | Compact inline notice strip for form dialogs. `semantic`: `"success"/"error"/"info"/"warning"`. When `detailText` is set, a "Xem chi tiết" link calls `AppNotifier.openDetail`. |

## Notification policy

| Context | Toast | Form inline | Recent events |
|---------|-------|-------------|---------------|
| Report download **OK** | Yes — `"success"`, tap **Copy path** | — | Info row (list shows `Report saved: <basename>`; DB stores full path; tap copies path) |
| Report download **fail** | Yes — `"error"`, tap → detail dialog | — | Warning row (auto on `logEvent`) |
| Config push **fail** | Yes — `"error"`, tap → detail dialog | — | Already logged as Warning |
| Settings save **OK** | Yes — `"success"` | — | — |
| Settings save **fail** | Yes — `"error"`, tap → detail dialog | — | — |
| Form Save **OK** | Yes — `"success"` (after `close()`) | — | Info row |
| Form Save **fail** | **No** | `FormNotice` + detail link | — |
| Form Connect **fail** | **No** | `FormNotice` | — |
| Form Connect **OK** | **No** | Short `FormNotice "success"` line | — |

**Rule:** Never show a toast while `AppNotifier.suppressed` is true (set via `onVisibleChanged: AppNotifier.suppressed = visible` on modal dialogs).

## Recent events click policy

| `eventType` | Action |
|-------------|--------|
| `Online`, `Offline`, `Info` (audit, not report-saved) | `loggerId > 0` → Logger Detail |
| Message starts with `Report saved:` | **Copy path** to clipboard (not navigate) |
| `Warning`, `Error`, `Alarm` | `MessageDetailDialog` (+ "Mở logger" if `loggerId > 0`) |

## Charts (`QtGraphs`)

Default Qt Graphs grid/axes via `GraphsTheme.colorScheme` (Light/Dark). App sets `backgroundColor` + `plotAreaBackgroundColor` + `seriesColors` in `ChartGraphsTheme`. Hover: `ChartHoverTooltip` + `ChartLinePointMarker` (`pointDelegate`).

## Manual QA (light + dark)

Toggle theme from rail `ThemeToggle` and verify:

- [ ] **Dashboard** — StatCards, Traffic Readings + Recent events panes, chart plot background matches pane
- [ ] **Loggers** — table inside elevated card, Outlined search field, Add Logger primary CTA
- [ ] **Logger Detail** — sensor + trending panes with card chrome, section titles, wide/narrow at 950px
- [ ] **Settings** — elevated form pane, Outlined inputs
- [ ] **LoggerFormDialog** — rounded dialog, Outlined fields, semantic validation labels
- [ ] **Rail** — active NavItem pill uses accent container tokens; Tonal buttons match
