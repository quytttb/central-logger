import QtQuick
import QtQuick.Controls.Material

import CentralLogger.Theme

// Flat icon tool button — 36×36 with Material Symbols + tooltip.
ToolButton {
    id: root

    property string iconName: ""
    property int iconSize: 20
    property color iconColor: AppColors.primaryText
    property string tooltipText: ""

    implicitWidth: 36
    implicitHeight: 36
    flat: true

    ToolTip.text: root.tooltipText
    ToolTip.visible: hovered && root.tooltipText.length > 0

    contentItem: UiIcon {
        name: root.iconName
        size: root.iconSize
        iconColor: root.iconColor
        anchors.centerIn: parent
    }
}
