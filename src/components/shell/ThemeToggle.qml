import QtQuick

import CentralLogger.Core

// Rail-bottom outlined circular theme toggle (light_mode / dark_mode, no label).
Item {
    id: root

    readonly property bool isDark: SettingsController.theme === "dark"

    implicitWidth: 80
    implicitHeight: 52

    RailCircleButton {
        anchors.fill: parent
        iconName: root.isDark ? "whiteBalanceSunny" : "weatherNight"
        tooltipText: root.isDark ? qsTr("Light mode") : qsTr("Dark mode")
        onClicked: {
            const newTheme = root.isDark ? "light" : "dark"
            if (SettingsController.theme !== newTheme)
                SettingsController.saveTheme(newTheme)
        }
    }
}
