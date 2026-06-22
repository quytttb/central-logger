pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

import CentralLogger.Components
import CentralLogger.Theme

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 768
    visible: true
    visibility: Window.Maximized
    title: qsTr("Central Logger")
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint

    Material.theme:   AppTheme.materialTheme
    Material.accent:  AppTheme.accent
    Material.primary: AppTheme.primary
    color: AppColors.surface

    property string currentView:      "dashboard"
    property int    selectedLoggerId: -1
    property Component activeTopBarToolbar: null

    readonly property int navigationRailWidth: AppTheme.railWidth

    function navigate(view) {
        if (view === "logger-detail") {
            return;
        }
        if (view === "loggers" && currentView === "logger-detail") {
            selectedLoggerId = -1;
        }
        currentView = view;
    }

    function selectLogger(id) {
        selectedLoggerId = id;
        currentView = "logger-detail";
    }

    // --- Notification overlay -----------------------------------------

    MessageDetailDialog {
        id: msgDetailDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        onNavigateToLogger: loggerId => root.selectLogger(loggerId)
    }

    AppToastHost {
        id: appToastHost
        parent: root.contentItem
        // Bottom-center of the content area, above the navigation rail.
        x: root.navigationRailWidth + (root.contentItem.width - root.navigationRailWidth - width) / 2
        y: root.contentItem.height - height - 24
        z: 999
    }

    Connections {
        target: AppNotifier
        function onDetailRequested(title, body, loggerId) {
            const alreadyOnThisLogger = root.currentView === "logger-detail"
                                        && loggerId >= 0
                                        && loggerId === root.selectedLoggerId
            msgDetailDialog.showMessage(title, body, loggerId, alreadyOnThisLogger)
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
                        case "dashboard":     return dashboardComp
                        case "loggers":       return loggersComp
                        case "logger-detail": return loggerDetailComp
                        case "history":       return historyComp
                        case "settings":      return settingsComp
                        default:              return dashboardComp
                        }
                    }
                }

                Component {
                    id: dashboardComp
                    DashboardView {
                        anchors.fill: parent
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                        onSelectLogger: loggerId => root.selectLogger(loggerId)
                    }
                }

                Component {
                    id: loggersComp
                    LoggersView {
                        anchors.fill: parent
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                        onSelectLogger: loggerId => root.selectLogger(loggerId)
                    }
                }

                Component {
                    id: loggerDetailComp
                    LoggerDetailView {
                        anchors.fill: parent
                        loggerId: root.selectedLoggerId
                        Component.onCompleted: root.activeTopBarToolbar = topBarToolbar
                        onGoBack: root.navigate("loggers")
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
