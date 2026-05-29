# Reusable QML components — module URI CentralLogger.Components
# Pure QML, Material controls. No C++ types here.
#
# Subfolders (filesystem only; types stay on CentralLogger.Components):
#   shell/   — app chrome (nav, buttons, icons)
#   layout/  — page layout (stat cards, banners, empty states)
#   table/   — data table chrome
#   chart/   — Qt Graphs wrappers
#   status/  — StatusChip, SensorStatusColumn
#   logger/  — logger-specific delegates and forms

set_source_files_properties(shell/MaterialIcons.qml        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(layout/AppNotifier.qml         PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(central_logger_components
    URI CentralLogger.Components
    VERSION 1.0
    STATIC
    QML_FILES
        # shell
        shell/AppButton.qml
        shell/NavItem.qml
        shell/AppNavigationRail.qml
        shell/AppTopBar.qml
        shell/MaterialIcons.qml
        shell/UiIcon.qml
        shell/IconToolButton.qml
        shell/ThemeToggle.qml
        # layout
        layout/StatCard.qml
        layout/ElevatedPane.qml
        layout/SectionHeader.qml
        layout/InlineBanner.qml
        layout/EmptyStatePlaceholder.qml
        layout/TableContentStack.qml
        layout/AppNotifier.qml
        layout/AppToastHost.qml
        layout/MessageDetailDialog.qml
        layout/FormNotice.qml
        # table
        table/TableHeaderCell.qml
        table/TableCellBackground.qml
        # chart
        chart/ChartGraphsTheme.qml
        chart/ChartGraphsView.qml
        chart/ChartTimeSeriesPanel.qml
        chart/ChartHoverTooltip.qml
        chart/ChartLinePointMarker.qml
        # status
        status/StatusChip.qml
        status/SensorStatusColumn.qml
        # logger
        logger/LoggerFormDialog.qml
        logger/RecentEventListItem.qml
    RESOURCES
        ../../resources/fonts/MaterialSymbols/MaterialSymbolsOutlined.ttf
        ../../resources/icons/brand_4m_technologies_blue.svg
    DEPENDENCIES
        central_logger_theme
        central_logger_core
)

add_library(central_logger::components ALIAS central_logger_components)
