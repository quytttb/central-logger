import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

// Compact inline notice for form dialogs (Connect fail, Save fail, probe status).
// Unlike a toast, FormNotice stays visible inside the form until cleared.
// A "Xem chi tiết" link is shown when detailText is non-empty.
//
// Usage:
//   FormNotice {
//       visible:    root.probeMessage.length > 0
//       semantic:   root.probeStatus     // "success" | "error" | "info" | "warning"
//       summary:    root.probeMessage
//       detailText: root.configLoadError
//       onDetailRequested: (title, body) => AppNotifier.openDetail(title, body, -1)
//   }
Item {
    id: root

    property string semantic:   "error"   // "success" | "error" | "info" | "warning"
    property string summary:    ""
    property string detailText: ""

    signal detailRequested(string title, string body)

    implicitWidth:  contentRow.implicitWidth
    implicitHeight: contentRow.implicitHeight

    readonly property color _fgColor: {
        switch (semantic) {
        case "success": return AppColors.success
        case "warning": return AppColors.warning
        case "info":    return AppColors.info
        default:        return AppColors.error
        }
    }

    RowLayout {
        id: contentRow
        anchors.left:  parent.left
        anchors.right: parent.right
        spacing: 4

        UiIcon {
            name: {
                switch (root.semantic) {
                case "success": return "checkCircle"
                case "warning": return "warning"
                case "info":    return "info"
                default:        return "error"
                }
            }
            size: 16
            iconColor: root._fgColor
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
        }

        Text {
            Layout.fillWidth: true
            text: root.summary
            color: root._fgColor
            font: AppTypography.labelMedium
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.detailText.length > 0
            text: qsTr("View details")
            color: AppColors.primaryColor
            font: AppTypography.labelMedium
            Layout.alignment: Qt.AlignTop

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.detailRequested(root.summary, root.detailText)
            }
        }
    }
}
