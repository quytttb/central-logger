import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Components
import CentralLogger.Theme

// Reusable confirmation / alert dialog (Material + AppButton).
Dialog {
    id: root

    property string message: ""
    property string detailText: ""
    property string confirmText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    property bool showCancel: false
    property string semantic: "error" // "error" | "warning" | "info"

    signal confirmed()
    signal cancelled()

    modal: true
    closePolicy: showCancel
                 ? (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
                 : Popup.CloseOnEscape
    standardButtons: Dialog.NoButton

    width: parent
        ? Math.min(420, parent.width - 48)
        : 420
    anchors.centerIn: parent

    Material.roundedScale: Material.ExtraLargeScale

    contentItem: ColumnLayout {
        spacing: AppTheme.formRowSpacing

        Label {
            Layout.fillWidth: true
            text: root.message
            wrapMode: Text.WordWrap
            font: AppTypography.bodyMedium
            color: AppColors.primaryText
        }

        InlineBanner {
            Layout.fillWidth: true
            visible: root.detailText.length > 0
            semantic: root.semantic === "warning" ? "warning" : "error"
            message: root.detailText
        }
    }

    footer: DialogButtonBox {
        spacing: 8

        AppButton {
            visible: root.showCancel
            kind: AppButton.Text
            text: root.cancelText
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: {
                root.cancelled()
                root.close()
            }
        }

        AppButton {
            kind: root.semantic === "error" ? AppButton.Primary : AppButton.Tonal
            text: root.confirmText
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                root.confirmed()
                root.close()
            }
        }
    }
}
