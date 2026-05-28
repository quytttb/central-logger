import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

Pane {
    id: root

    property string semantic: "error"
    property string message: ""
    property bool dismissible: false

    signal dismissed()

    Material.elevation: 0
    padding: dismissible ? 8 : 12

    background: Rectangle {
        radius: AppTheme.cardRadius
        color: semantic === "warning"
               ? AppColors.warningContainer
               : AppColors.errorContainer
        border.width: 1
        border.color: semantic === "warning"
                        ? AppColors.warning
                        : AppColors.outlineVariant
    }

    contentItem: RowLayout {
        spacing: AppTheme.toolbarGap

        Label {
            Layout.fillWidth: true
            text: root.message
            wrapMode: Text.WordWrap
            color: semantic === "warning"
                   ? AppColors.primaryText
                   : AppColors.errorContainerFg
            font: semantic === "warning" ? AppTypography.bodyMedium : AppTypography.labelMedium
        }

        IconToolButton {
            visible: root.dismissible
            iconName: "close"
            iconColor: semantic === "warning" ? AppColors.primaryText : AppColors.error
            tooltipText: qsTr("Dismiss")
            onClicked: {
                root.dismissed()
                root.visible = false
            }
        }
    }
}
