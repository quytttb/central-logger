import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

// M3 buttons: Primary (teal filled), Secondary (outlined), Tonal (indigo container), Text (flat).
Button {
    id: root

    enum Kind { Primary, Secondary, Tonal, Text }

    property int    kind: AppButton.Primary
    property string iconName: ""

    readonly property color fgColor: {
        if (!root.enabled) {
            if (kind === AppButton.Tonal)
                return AppColors.withAlpha(AppColors.accentContainerFg, 0.38)
            return AppColors.disabledContent
        }
        if (kind === AppButton.Primary)
            return AppColors.onPrimary
        if (kind === AppButton.Tonal)
            return AppColors.accentContainerFg
        if (kind === AppButton.Secondary)
            return AppColors.primaryColor
        return AppColors.primaryText
    }

    flat: true
    implicitHeight: AppTheme.buttonHeight
    leftPadding: iconName.length > 0 ? 16 : AppTheme.buttonPaddingH
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0

    Material.foreground: root.fgColor
    Material.background: "transparent"

    background: Rectangle {
        anchors.fill: parent
        radius: AppTheme.buttonRadius
        border.width: root.kind === AppButton.Secondary ? 1 : 0
        border.color: root.enabled ? AppColors.outline : AppColors.outlineVariant
        color: {
            if (root.kind === AppButton.Tonal) {
                const base = AppColors.accentContainer
                return root.enabled ? base : AppColors.withAlpha(base, 0.35)
            }
            if (!root.enabled)
                return "transparent"
            if (root.kind === AppButton.Primary)
                return AppColors.primaryColor
            if (root.kind === AppButton.Secondary)
                return root.pressed || root.down || root.hovered
                       ? AppColors.hoverFill
                       : "transparent"
            return root.hovered ? AppColors.hoverFill : "transparent"
        }
    }

    contentItem: RowLayout {
        spacing: root.iconName.length > 0 ? 8 : 0
        Layout.alignment: Qt.AlignHCenter

        UiIcon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 18
            iconColor: root.fgColor
            Layout.alignment: Qt.AlignVCenter
        }

        // Text (not Label) — Material Label ignores color and uses theme foreground (black in light).
        Text {
            text: root.text
            font: root.font
            color: root.fgColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
