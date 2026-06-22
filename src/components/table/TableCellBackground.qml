import QtQuick

import CentralLogger.Theme

// M3 table cell background: hover highlight + row divider (no zebra — avoids corner bleed on rounded panes).
Rectangle {
    required property bool cellHovered

    anchors.fill: parent
    color: cellHovered ? AppColors.hoverFill : "transparent"

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: AppColors.outlineVariant
    }
}
