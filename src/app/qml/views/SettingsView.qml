pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

// Settings screen with 2 tabs:
//   - General: app_settings preferences + Diagnostics
//   - Loggers: 3-column logger CRUD + device sensor thresholds
Item {
    id: root

    property int tabIndex: 0
    property Component topBarToolbar: SettingsTopBar {}

    readonly property bool isGeneralTab: root.tabIndex === 0

    StackLayout {
        anchors.fill: parent
        currentIndex: root.tabIndex

        SettingsGeneralPane {
            id: generalPane
        }

        LoggersSettingsPane {
            id: loggersPane
        }
    }

    component SettingsTopBar: RowLayout {
        spacing: AppTheme.toolbarGap

        TabBar {
            id: settingsTabs
            Layout.preferredWidth: 236
            Layout.alignment: Qt.AlignVCenter
            currentIndex: root.tabIndex
            onCurrentIndexChanged: root.tabIndex = currentIndex
            spacing: AppTheme.spacingXS
            padding: 3

            background: Rectangle {
                color: AppColors.surfaceContainer
                radius: AppTheme.listItemRadius
                border.color: AppColors.elevatedBorder
                border.width: 1
            }

            contentItem: ListView {
                model: settingsTabs.contentModel
                currentIndex: settingsTabs.currentIndex
                spacing: settingsTabs.spacing
                orientation: ListView.Horizontal
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.AutoFlickIfNeeded
                snapMode: ListView.SnapToItem
            }

            ThemedTabButton {
                text: qsTr("General")
                iconName: "cog"
            }

            ThemedTabButton {
                text: qsTr("Loggers")
                iconName: "server"
            }
        }

        Item { Layout.fillWidth: true }

        AppButton {
            visible: root.isGeneralTab
            kind: AppButton.Secondary
            text: qsTr("Reset")
            enabled: generalPane.dirty
            onClicked: generalPane.syncFromController()
        }

        AppButton {
            visible: root.isGeneralTab
            kind: AppButton.Primary
            text: qsTr("Save")
            enabled: generalPane.dirty
            onClicked: generalPane.applyAndSave()
        }
    }
}
