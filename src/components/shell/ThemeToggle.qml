import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Theme

// Rail-bottom outlined circular theme toggle (light_mode / dark_mode, no label).
Item {
    id: root

    readonly property bool isDark: SettingsController.theme === "dark"
    readonly property string iconName: isDark ? "whiteBalanceSunny" : "weatherNight"

    property bool hovered: false

    implicitWidth: 80
    implicitHeight: 52

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
            name: root.iconName
            size: 20
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
            const newTheme = root.isDark ? "light" : "dark"
            if (SettingsController.theme !== newTheme)
                SettingsController.saveTheme(newTheme)
        }
    }
}
