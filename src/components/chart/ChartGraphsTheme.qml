import QtGraphs

import CentralLogger.Theme

GraphsTheme {
    id: root

    colorScheme: AppTheme.isLightTheme ? GraphsTheme.ColorScheme.Light : GraphsTheme.ColorScheme.Dark
    // Match elevated panes/cards (Dashboard, StatCard) — plot area defaults to a darker scheme color.
    backgroundColor: AppColors.surfaceContainerLow
    plotAreaBackgroundColor: AppColors.surfaceContainerLow
    seriesColors: AppColors.graphSeriesColors
}
