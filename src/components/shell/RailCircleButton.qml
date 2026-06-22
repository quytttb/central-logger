import QtQuick
import QtQuick.Controls

import CentralLogger.Theme

// Outlined circular icon button for the navigation rail (40dp circle, 52dp row).
Item {
    id: root

    property string iconName: ""
    property color iconColor: AppColors.primaryText
    property color borderColor: AppColors.outline
    property string tooltipText: ""

    property bool hovered: false

    signal clicked()

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
        border.color: root.borderColor

        UiIcon {
            anchors.centerIn: parent
            name: root.iconName
            size: AppTheme.iconSizeMd
            iconColor: root.iconColor
        }
    }

    MouseArea {
        anchors.fill: circle
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.clicked()
    }

    ToolTip {
        visible: root.hovered && root.tooltipText.length > 0
        text: root.tooltipText
        delay: 500
    }
}
