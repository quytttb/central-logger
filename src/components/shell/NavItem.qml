import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

// Navigation rail destination — icon above label (M3 collapsed rail).
ItemDelegate {
    id: root

    property string viewName: ""
    property string label:    ""
    property string iconName: ""
    property bool   active:   false

    readonly property color activeContainerColor: AppColors.accentContainer
    readonly property color activeForegroundColor: AppColors.accentContainerFg

    signal navigate(string view)

    Layout.fillWidth: true
    Layout.preferredHeight: AppTheme.navItemHeight
    Layout.leftMargin: 4
    Layout.rightMargin: 4

    padding: 4
    hoverEnabled: true
    onClicked: root.navigate(root.viewName)

    background: Item {}

    contentItem: ColumnLayout {
        spacing: 4

        Item {
            Layout.preferredWidth: AppTheme.navPillWidth
            Layout.preferredHeight: AppTheme.navPillHeight
            Layout.alignment: Qt.AlignHCenter

            Rectangle {
                anchors.centerIn: parent
                width: AppTheme.navPillWidth
                height: AppTheme.navPillHeight
                radius: AppTheme.navPillRadius
                visible: root.active
                color: root.activeContainerColor
            }

            Rectangle {
                anchors.centerIn: parent
                width: AppTheme.navPillWidth
                height: AppTheme.navPillHeight
                radius: AppTheme.navPillRadius
                visible: root.hovered && !root.active
                color: AppColors.hoverFill
            }

            UiIcon {
                anchors.centerIn: parent
                name: root.iconName
                size: AppTheme.iconSizeLg
                iconColor: root.active
                         ? root.activeForegroundColor
                         : AppColors.onSurfaceVariant
            }
        }

        Label {
            text: root.label
            color: root.active ? root.activeForegroundColor : AppColors.onSurfaceVariant
            font: AppTypography.labelMedium
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
