import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    visible: true
    visibility: Window.Maximized
    title: qsTr("Central Logger")

    Material.theme:   AppTheme.materialTheme
    Material.accent:  AppTheme.accent
    Material.primary: AppTheme.primary
    color: AppColors.surface

    property string currentView:      "dashboard"
    property int    selectedLoggerId: -1

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
                toolbarSource: viewLoader.item ? viewLoader.item.topBarToolbar : null
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
                    sourceComponent: {
                        switch (root.currentView) {
                        case "dashboard":     return dashboardComp
                        case "loggers":       return loggersComp
                        case "logger-detail": return loggerDetailComp
                        case "settings":      return settingsComp
                        default:              return dashboardComp
                        }
                    }
                }

                Component {
                    id: dashboardComp
                    DashboardView {
                        anchors.fill: parent
                        onSelectLogger: loggerId => root.selectLogger(loggerId)
                    }
                }

                Component {
                    id: loggersComp
                    LoggersView {
                        anchors.fill: parent
                        onSelectLogger: loggerId => root.selectLogger(loggerId)
                    }
                }

                Component {
                    id: loggerDetailComp
                    LoggerDetailView {
                        anchors.fill: parent
                        loggerId: root.selectedLoggerId
                        onGoBack: root.navigate("loggers")
                    }
                }

                Component {
                    id: settingsComp
                    SettingsView {
                        anchors.fill: parent
                    }
                }
            }
        }
    }
}
