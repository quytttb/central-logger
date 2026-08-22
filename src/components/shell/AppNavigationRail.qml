pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import CentralLogger.Theme
import LoggerKit.Theme

// Fixed 80px Material-style navigation rail (icon above label per destination).
Item {
    id: rail

    property string currentView: "dashboard"

    signal navigate(string view)

    implicitWidth: AppTheme.railWidth

    Rectangle {
        anchors.fill: parent
        color: AppColors.navRail

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: AppColors.dividerLine
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 76

            Image {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/LoggerKit/Components/resources/icons/brand_4m_technologies_blue.svg"
                sourceSize: Qt.size(60, 60)
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                anchors.fill: parent
                onPressed: mouse => {
                    if (mouse.button === Qt.LeftButton && Window.window)
                        Window.window.startSystemMove()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppTheme.navItemSpacing

            NavItem {
                viewName: "dashboard"
                label: qsTr("Dashboard")
                iconName: "viewDashboard"
                active: rail.currentView === "dashboard"
                onNavigate: view => rail.navigate(view)
            }
            NavItem {
                viewName: "loggers"
                label: qsTr("Loggers")
                iconName: "server"
                active: rail.currentView === "loggers"
                     || rail.currentView === "logger-detail"
                onNavigate: view => rail.navigate(view)
            }
            NavItem {
                viewName: "history"
                label: qsTr("History")
                iconName: "history"
                active: rail.currentView === "history"
                onNavigate: view => rail.navigate(view)
            }
            NavItem {
                viewName: "settings"
                label: qsTr("Settings")
                iconName: "cog"
                active: rail.currentView === "settings"
                onNavigate: view => rail.navigate(view)
            }
        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            spacing: AppTheme.sectionSpacing

            ThemeToggle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: AppTheme.railWidth
                Layout.preferredHeight: 52
            }

            WindowControls {
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
