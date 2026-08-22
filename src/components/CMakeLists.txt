# Reusable QML components — module URI CentralLogger.Components
# App-specific components only. Generic shell/layout/table/chart/status chrome
# now lives in the shared kit (LoggerKit.Components).
#
# Subfolders (filesystem only; types stay on CentralLogger.Components):
#   shell/   — app chrome (nav rail, top bar, theme toggle, window controls)
#   layout/  — page layout (stat cards, banners, section headers, dialogs)
#   status/  — SensorStatusChip (domain wrapper), SensorStatusColumn
#   chart/   — Qt Graphs panels bound to C++ chart presentation
#   logger/  — logger-specific delegates and forms

qt_add_qml_module(central_logger_components
    URI CentralLogger.Components
    VERSION ${QML_MODULE_VERSION}
    STATIC
    QML_FILES
        # shell
        shell/NavItem.qml
        shell/AppNavigationRail.qml
        shell/AppTopBar.qml
        shell/RailCircleButton.qml
        shell/ThemeToggle.qml
        shell/WindowControls.qml
        # layout
        layout/StatCard.qml
        layout/SectionHeader.qml
        layout/InlineBanner.qml
        layout/AlertDialog.qml
        layout/FormNotice.qml
        # status
        status/SensorStatusChip.qml
        status/SensorStatusColumn.qml
        # chart
        chart/ChartTimeSeriesPanel.qml
        chart/ChartHoverTooltip.qml
        # logger
        logger/LoggerFormDialog.qml
        logger/RecentEventListItem.qml
    DEPENDENCIES
        LoggerKit.Theme
        LoggerKit.Components
        central_logger_theme
        central_logger_core
)

add_library(central_logger::components ALIAS central_logger_components)
