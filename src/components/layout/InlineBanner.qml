pragma ComponentBehavior: Bound

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
        color: root.semantic === "warning"
               ? AppColors.warningContainer
               : AppColors.errorContainer
        border.width: 1
        border.color: root.semantic === "warning"
                        ? AppColors.warning
                        : AppColors.outlineVariant
    }

    contentItem: RowLayout {
        spacing: AppTheme.toolbarGap

        Label {
            Layout.fillWidth: true
            text: root.message
            wrapMode: Text.WordWrap
            color: root.semantic === "warning"
                   ? AppColors.primaryText
                   : AppColors.errorContainerFg
            font: root.semantic === "warning" ? AppTypography.bodyMedium : AppTypography.labelMedium
        }

        AppButton {
            visible: root.dismissible
            kind: AppButton.Neutral
            forceDarkText: false
            iconOnly: true
            controlSize: 32
            iconName: "close"
            tooltipText: qsTr("Dismiss")
            onClicked: {
                root.dismissed()
                root.visible = false
            }
        }
    }
}
