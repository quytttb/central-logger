import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

Rectangle {
    id: badge

    property string label: ""
    property bool active: false
    property color activeColor: AppColors.success
    property color inactiveColor: AppColors.outline

    implicitHeight: 30
    implicitWidth: badgeText.implicitWidth + dot.width + 28
    radius: height / 2
    color: "transparent"
    border.width: 1
    border.color: badge.active ? badge.activeColor : badge.inactiveColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 12
        spacing: 6

        Rectangle {
            id: dot
            width: 10
            height: 10
            radius: 5
            color: badge.active ? badge.activeColor : badge.inactiveColor
        }

        Label {
            id: badgeText
            text: badge.label
            font: AppTypography.labelSmall
            color: badge.active ? badge.activeColor : badge.inactiveColor
        }
    }
}
