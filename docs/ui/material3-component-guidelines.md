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
| `shell/` | `AppButton`, `IconToolButton`, `NavItem`, `AppNavigationRail`, `AppTopBar`, `UiIcon`, `MaterialIcons`, `ThemeToggle` |
| `layout/` | `ElevatedPane`, `SectionHeader`, `StatCard`, `InlineBanner`, `EmptyStatePlaceholder`, `TableContentStack` |
| `table/` | `TableHeaderCell`, `TableCellBackground` |
| `chart/` | `ChartGraphsView`, `ChartGraphsTheme`, `ChartTimeSeriesPanel`, `ChartHoverTooltip`, … |
| `logger/` | `LoggerFormDialog`, `StatusBadge`, `SensorStatusChip`, `RecentEventListItem` |

| Component | Notes |
|-----------|--------|
| `ElevatedPane` | M3 outlined surface — `surfaceContainerLow`, `cardRadius`, `elevatedBorder`; default padding `AppTheme.sectionSpacing` |
| `SectionHeader` | `titleMedium` + optional trailing actions (refresh icons) |
| `IconToolButton` | Flat 36×36 icon button with tooltip |
| `NavItem` | Rail pill 56×32; `accentContainer` / `onAccentContainer` when active |
| `StatCard` | KPI card; label uses `AppTypography.overline` |
| `TableHeaderCell`, `TableCellBackground` | Table chrome |
| `TableContentStack` | `hasData` → children; else `EmptyStatePlaceholder` (tables, charts, lists) |
| `ChartGraphsTheme` | Plot + view background `surfaceContainerLow`; series from `AppColors.graphSeriesColors` |
| `ChartHoverTooltip` | Snap-to-point hover (`snapAt`); tooltip follows cursor in plot |
| `ChartLinePointMarker` | Circular markers on `LineSeries` (`pointDelegate`) |
| `EmptyStatePlaceholder` | Icon above, message below, centered |
| `LoggerFormDialog` | `ExtraLargeScale`; inputs `Outlined` |

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
