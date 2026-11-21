pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import CentralLogger.Core
import LoggerKit.Theme
import LoggerKit.Components

// Rail-bottom outlined circular theme toggle (light_mode / dark_mode, no label).
Item {
    id: root

    readonly property bool isDark: SettingsController.theme === AppDefaults.themeDark
    readonly property string tooltipText: isDark ? qsTr("Light mode") : qsTr("Dark mode")
    property bool hovered: false

    implicitWidth: AppTheme.railWidth
    implicitHeight: AppTheme.buttonHeight

    Rectangle {
        id: circle
        anchors.centerIn: parent
        width: 40
        height: 40
        radius: width / 2
        color: root.hovered ? AppColors.hoverFill : "transparent"
        border.width: 1
        border.color: AppColors.outline

        UiIcon {
            anchors.centerIn: parent
            name: root.isDark ? "whiteBalanceSunny" : "weatherNight"
            size: AppTheme.iconSizeMd
            iconColor: AppColors.primaryText
        }
    }

    MouseArea {
        anchors.fill: circle
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: {
            const newTheme = root.isDark ? AppDefaults.themeLight : AppDefaults.themeDark
            if (SettingsController.theme !== newTheme)
                SettingsController.saveTheme(newTheme)
        }
    }

    ToolTip {
        visible: root.hovered && root.tooltipText.length > 0
        text: root.tooltipText
        delay: 500
    }
}
