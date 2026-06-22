import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

// M3 stat card — elevation 0, surfaceContainerLow + elevatedBorder.
Pane {
    id: root

    property string label: ""
    property int    value: 0
    property bool   semanticNumberColor: false
    property color  numberColor

    Material.theme: AppTheme.materialTheme
    Material.elevation: 0
    padding: AppTheme.cardPadding

    background: Rectangle {
        color: AppColors.surfaceContainerLow
        radius: AppTheme.cardRadius
        border.width: 1
        border.color: AppColors.elevatedBorder
    }

    contentItem: ColumnLayout {
        spacing: 6

        Label {
            text: root.label
            font: AppTypography.overline
            color: AppColors.textMuted
        }

        Text {
            Layout.fillWidth: true
            text: root.value
            font: AppTypography.displaySmall
            color: root.semanticNumberColor ? root.numberColor : AppColors.primaryText
        }
    }
}
