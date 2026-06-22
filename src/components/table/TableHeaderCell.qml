import QtQuick

import CentralLogger.Theme

// HorizontalHeaderView delegate — Qt 6 injects column + model; must not shadow with defaults.
Rectangle {
    id: cell

    required property int column
    required property var model

    property bool alignRight: false
    property real cornerRadius: 0
    property bool roundTopLeft: false
    property bool roundTopRight: false

    readonly property string headerText: {
        if (model !== null && model !== undefined && model.display !== undefined)
            return String(model.display)
        return ""
    }

    implicitHeight: AppTheme.tableHeaderHeight
    height: implicitHeight

    color: AppColors.surfaceContainerHigh
    topLeftRadius: roundTopLeft ? cornerRadius : 0
    topRightRadius: roundTopRight ? cornerRadius : 0

    Text {
        anchors {
            left: parent.left
            right: parent.right
            leftMargin: 8
            rightMargin: 8
            verticalCenter: parent.verticalCenter
        }
        text: cell.headerText
        font: AppTypography.labelLarge
        color: AppColors.tableHeaderText
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: cell.alignRight ? Text.AlignRight : Text.AlignLeft
        elide: Text.ElideRight
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: AppColors.outline
    }
}
