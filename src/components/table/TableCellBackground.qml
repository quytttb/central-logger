import QtQuick
import QtQuick.Controls.Material

import CentralLogger.Theme

// M3 table row/cell background for TableView ItemDelegate (multi-column).
Rectangle {
    required property bool cellHovered
    required property int  rowIndex

    anchors.fill: parent

    color: cellHovered
           ? AppColors.hoverFill
           : (rowIndex % 2 === 1 ? AppColors.surfaceContainer : "transparent")

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: AppColors.outlineVariant
    }
}
