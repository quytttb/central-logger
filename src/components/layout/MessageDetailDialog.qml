import QtQuick
import QtQuick.Controls.Material

import CentralLogger.Components
import CentralLogger.Theme

// Generic modal dialog for full error / info messages.
// Opened via AppNotifier.openDetail() → Main.qml → msgDetailDialog.showMessage().
//
// Footer:
//   "Close" — always present; "Open logger" when loggerId >= 0 and not hidden
Dialog {
    id: root

    property string detailTitle:   ""
    property string detailBody:    ""
    property int    detailLoggerId: -1
    /// When true, footer hides "Open logger" (e.g. user is already on that logger's Detail view).
    property bool   hideOpenLoggerButton: false

    signal navigateToLogger(int loggerId)

    function showMessage(title, body, loggerId, hideOpenLogger) {
        root.detailTitle    = title  || ""
        root.detailBody     = body   || ""
        root.detailLoggerId = (loggerId !== undefined && loggerId >= 0) ? loggerId : -1
        root.hideOpenLoggerButton = hideOpenLogger === true
        root.open()
    }

    title: root.detailTitle
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.NoButton

    width: parent
        ? Math.min(parent.width - 48, AppTheme.dialogMaxWidth)
        : AppTheme.dialogMaxWidth
    anchors.centerIn: parent

    Material.roundedScale: Material.ExtraLargeScale

    contentItem: ScrollView {
        id: scrollView
        clip: true
        implicitHeight: Math.min(bodyText.implicitHeight + 8, 320)

        TextArea {
            id: bodyText
            text: root.detailBody
            readOnly: true
            selectByMouse: true
            wrapMode: Text.WordWrap
            font: AppTypography.bodyMedium
            color: AppColors.primaryText
            background: null
            padding: 0
            topPadding: 0
            bottomPadding: 0
        }
    }

    footer: DialogButtonBox {
        spacing: 8

        AppButton {
            kind: AppButton.Tonal
            text: qsTr("Open logger")
            visible: root.detailLoggerId >= 0 && !root.hideOpenLoggerButton
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: {
                root.close()
                root.navigateToLogger(root.detailLoggerId)
            }
        }

        AppButton {
            kind: AppButton.Text
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
    }
}
