import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import CentralLogger.Theme

Item {
    id: root

    property string displayStatus: ""
    property string diStatusCode: ""
    property string alarmType: ""
    property bool showAlarmBadge: false

    implicitHeight: 24

    readonly property string chipText: AppColors.compactStatusText(
        displayStatus, diStatusCode, alarmType, showAlarmBadge)
    readonly property string toolTipText: AppColors.sensorStatusTooltip(
        displayStatus, diStatusCode, alarmType, showAlarmBadge)

    Rectangle {
        id: chip
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: 24
        radius: AppTheme.chipRadius
        color: AppColors.sensorStatusFill(displayStatus, diStatusCode)
        border.width: 1
        border.color: AppColors.sensorStatusBorder(displayStatus, diStatusCode)

        Label {
            id: chipLabel
            anchors {
                left: parent.left
                right: parent.right
                leftMargin: 6
                rightMargin: 6
                verticalCenter: parent.verticalCenter
            }
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
            text: root.chipText
            font: AppTypography.labelSmall
            color: AppColors.sensorStatusText(displayStatus, diStatusCode)
        }
    }

    HoverHandler { id: chipHover }

    ToolTip {
        visible: chipHover.hovered && root.toolTipText.length > 0
                 && root.toolTipText !== root.chipText
        text: root.toolTipText
        delay: 400
    }
}
