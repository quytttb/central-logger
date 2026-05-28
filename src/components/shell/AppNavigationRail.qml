import QtQuick
import QtQuick.Layouts

import CentralLogger.Theme

// Fixed 80px Material-style navigation rail (icon above label per destination).
Item {
    id: rail

    property string currentView: "dashboard"

    signal navigate(string view)

    implicitWidth: AppTheme.railWidth

    Rectangle {
        anchors.fill: parent
        color: AppColors.surface

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: AppColors.dividerLine
        }
    }

    Item {
        id: logoArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 76

        Image {
            anchors.centerIn: parent
            source: "qrc:/qt/qml/CentralLogger/Components/resources/icons/brand_4m_technologies_blue.svg"
            sourceSize: Qt.size(60, 60)
            fillMode: Image.PreserveAspectFit
        }
    }

    ColumnLayout {
        id: navColumn
        anchors.top: logoArea.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: AppTheme.navItemSpacing

        NavItem {
            viewName: "dashboard"
            label: "Dashboard"
            iconName: "viewDashboard"
            active: rail.currentView === "dashboard"
            onNavigate: view => rail.navigate(view)
        }
        NavItem {
            viewName: "loggers"
            label: "Loggers"
            iconName: "server"
            active: rail.currentView === "loggers"
                 || rail.currentView === "logger-detail"
            onNavigate: view => rail.navigate(view)
        }
        NavItem {
            viewName: "settings"
            label: "Settings"
            iconName: "cog"
            active: rail.currentView === "settings"
            onNavigate: view => rail.navigate(view)
        }
    }

    Item {
        id: themeToggleWrap
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 16
        height: 52

        ThemeToggle {
            anchors.fill: parent
        }
    }
}
