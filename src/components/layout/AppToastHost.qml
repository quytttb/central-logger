import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Theme

Popup {
    id: root

    padding: 0
    implicitWidth: Math.min(480, parent ? parent.width - 48 : 480)
    implicitHeight: toastBody.implicitHeight + 16

    visible: AppNotifier.toastVisible && !AppNotifier.suppressed
    closePolicy: Popup.NoAutoClose

    Material.elevation: 3

    function handleToastActivated() {
        if (AppNotifier.pendingCopyPath.length > 0) {
            if (DesktopService.copyToClipboard(AppNotifier.pendingCopyPath)) {
                AppNotifier.show(qsTr("Path copied to clipboard"), "success", { durationMs: 2500 })
            }
        } else if (AppNotifier.pendingDetailText.length > 0) {
            AppNotifier.openDetail(
                AppNotifier.pendingDetailTitle,
                AppNotifier.pendingDetailText,
                AppNotifier.pendingDetailLoggerId
            )
        }
        AppNotifier.dismiss()
    }

    Timer {
        running: root.visible
        interval: AppNotifier.toastDurationMs
        onTriggered: AppNotifier.dismiss()
    }

    background: Rectangle {
        color: AppColors.surfaceContainerHigh
        radius: AppTheme.cardRadius
        border.width: 1
        border.color: AppColors.elevatedBorder
    }

    contentItem: Item {
        id: toastBody
        implicitWidth:  contentRow.implicitWidth + 32
        implicitHeight: contentRow.implicitHeight + 16

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.handleToastActivated()
        }

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            width: parent.width - 32
            spacing: 10

            UiIcon {
                name: {
                    switch (AppNotifier.toastSemantic) {
                    case "success": return "checkCircle"
                    case "warning": return "warning"
                    case "error":   return "error"
                    default:        return "info"
                    }
                }
                size: 20
                iconColor: {
                    switch (AppNotifier.toastSemantic) {
                    case "success": return AppColors.success
                    case "warning": return AppColors.warning
                    case "error":   return AppColors.error
                    default:        return AppColors.info
                    }
                }
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                text: AppNotifier.toastSummary
                color: AppColors.primaryText
                font: AppTypography.bodyMedium
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }

            Text {
                visible: AppNotifier.pendingCopyPath.length > 0
                         || AppNotifier.pendingDetailText.length > 0
                text: AppNotifier.pendingCopyPath.length > 0
                      ? qsTr("Copy path")
                      : qsTr("Details")
                color: AppColors.primaryColor
                font: AppTypography.labelLarge
            }

            Item {
                implicitWidth:  26
                implicitHeight: 26
                Layout.alignment: Qt.AlignVCenter

                UiIcon {
                    anchors.centerIn: parent
                    name: "close"
                    size: 18
                    iconColor: AppColors.textMuted
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function(mouse) {
                        mouse.accepted = true
                        AppNotifier.dismiss()
                    }
                }
            }
        }
    }
}
