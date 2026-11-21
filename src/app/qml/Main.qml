pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

import CentralLogger.Components
import CentralLogger.Core
import LoggerKit.Theme
import LoggerKit.Components

ApplicationWindow {
    id: root

    readonly property Screen targetScreen: root.screen || Screen
    readonly property real windowScreenFraction: 0.8

    width: targetScreen
           ? Math.round(targetScreen.availableWidth * windowScreenFraction)
           : Math.round(Screen.desktopAvailableWidth * windowScreenFraction)
    height: targetScreen
            ? Math.round(targetScreen.availableHeight * windowScreenFraction)
            : Math.round(Screen.desktopAvailableHeight * windowScreenFraction)
    minimumWidth: 1024
    minimumHeight: 768
    visible: true
    visibility: Window.Maximized

    function centerOnTargetScreen() {
        const scr = root.screen || Screen
        if (!scr)
            return
        x = scr.virtualX + Math.round((scr.availableWidth - width) / 2)
        y = scr.virtualY + Math.round((scr.availableHeight - height) / 2)
    }

    Component.onCompleted: {
        ThemeMode.mode = Qt.binding(() => SettingsController.theme)
        centerOnTargetScreen()
    }

    title: qsTr("Central Logger")
    flags: Qt.Window | (Qt.platform.os === "windows" ? 0 : Qt.FramelessWindowHint) | Qt.WindowSystemMenuHint

    Material.theme:   AppTheme.materialTheme
    Material.accent:  AppTheme.accent
    Material.primary: AppTheme.primary
    color: AppColors.surface

    property string currentView:        "dashboard"
    property string pendingLoggersTab:  "data"
    property Component activeTopBarToolbar: null

    readonly property int navigationRailWidth: AppTheme.railWidth

    function navigate(view) {
        if (view === "loggers" && currentView !== "loggers") {
            pendingLoggersTab = "data"
        }
        currentView = view
    }

    function openLoggerGraph(loggerId) {
        if (loggerId > 0)
            DashboardController.graphLoggerId = loggerId
        pendingLoggersTab = "graph"
        currentView = "loggers"
        const loaded = viewLoader.item as LoggersView
        if (loaded)
            loaded.openGraphForLogger(loggerId)
    }

    // --- Notification overlay -----------------------------------------

    MessageDetailDialog {
        id: msgDetailDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        contextActionText: qsTr("Open logger")
        onContextActionRequested: loggerId => root.openLoggerGraph(loggerId)
    }

    AppToastHost {
        id: appToastHost
        parent: root.contentItem
        x: root.navigationRailWidth + (root.contentItem.width - root.navigationRailWidth - width) / 2
        y: root.contentItem.height - height - 24
        z: 999
    }

    Connections {
        target: AppNotifier
        function onDetailRequested(title, body, loggerId) {
            const loadedLoggers = viewLoader.item as LoggersView
            const alreadyOnThisGraph = root.currentView === "loggers"
                                       && loadedLoggers !== null
                                       && loadedLoggers.isGraphTab
                                       && loggerId > 0
                                       && loggerId === DashboardController.graphLoggerId
            msgDetailDialog.showMessage(title, body, loggerId, alreadyOnThisGraph)
        }
    }

    // ------------------------------------------------------------------

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        clip: false

        handle: Item { implicitWidth: 0 }

        AppNavigationRail {
            SplitView.preferredWidth: root.navigationRailWidth
            SplitView.minimumWidth:   root.navigationRailWidth
            SplitView.maximumWidth:   root.navigationRailWidth
            SplitView.fillHeight:     true
            currentView: root.currentView
            onNavigate:  view => root.navigate(view)
        }

        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            spacing: 0

            AppTopBar {
                Layout.fillWidth: true
                toolbarSource: root.activeTopBarToolbar
            }

            Item {
                id: contentHost
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: AppColors.surface
                    z: -1
                }

                component ResizeHandle: MouseArea {
                    property int edge: Qt.RightEdge
                    hoverEnabled: true
                    cursorShape: {
                        if (edge === Qt.TopEdge || edge === Qt.BottomEdge)
                            return Qt.SizeVerCursor
                        if (edge === Qt.LeftEdge || edge === Qt.RightEdge)
                            return Qt.SizeHorCursor
                        if (edge === Qt.TopLeftEdge || edge === Qt.BottomRightEdge)
                            return Qt.SizeFDiagCursor
                        if (edge === Qt.TopRightEdge || edge === Qt.BottomLeftEdge)
                            return Qt.SizeBDiagCursor
                        return Qt.ArrowCursor
                    }
                    enabled: root.visibility !== Window.Maximized
                    visible: enabled
                    width: 6
                    height: 6
                    propagateComposedEvents: true
                    onPressed: mouse => {
                        if (mouse.button === Qt.LeftButton && Window.window)
                            Window.window.startSystemResize(edge)
                    }
                }

                ResizeHandle { anchors.right: parent.right;  anchors.top: parent.top;        edge: Qt.TopRightEdge }
                ResizeHandle { anchors.right: parent.right;  anchors.bottom: parent.bottom;    edge: Qt.BottomRightEdge }
                ResizeHandle { anchors.bottom: parent.bottom; anchors.right: parent.right;     edge: Qt.BottomRightEdge }
                ResizeHandle { anchors.right: parent.right;  anchors.verticalCenter: parent.verticalCenter; edge: Qt.RightEdge }
                ResizeHandle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; edge: Qt.BottomEdge }

                Loader {
                    id: viewLoader
                    anchors.fill: parent
                    asynchronous: false
                    onItemChanged: {
                        if (!item)
                            root.activeTopBarToolbar = null
                    }
                    sourceComponent: {
                        switch (root.currentView) {
                        case "dashboard": return dashboardComp
                        case "loggers":   return loggersComp
                        case "history":   return historyComp
                        case "settings":  return settingsComp
                        default:          return dashboardComp
                        }
                    }
                }

                Component {
                    id: dashboardComp
                    DashboardView {
                        anchors.fill: parent
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                        onOpenLoggerGraph: loggerId => root.openLoggerGraph(loggerId)
                    }
                }

                Component {
                    id: loggersComp
                    LoggersView {
                        anchors.fill: parent
                        initialTab: root.pendingLoggersTab
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                    }
                }

                Component {
                    id: historyComp
                    HistoryView {
                        anchors.fill: parent
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                    }
                }

                Component {
                    id: settingsComp
                    SettingsView {
                        anchors.fill: parent
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                    }
                }
            }
        }
    }
}
