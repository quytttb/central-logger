import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Theme

// M3 buttons: Primary, Secondary, Tonal, Neutral (surface filled), Error (soft red filled), Text.
Button {
    id: root

    enum Kind { Primary, Secondary, Tonal, Neutral, Error, Text }

    property int    kind: AppButton.Primary
    property string iconName: ""
    property bool   iconOnly: false
    property string tooltipText: ""

    // Square footprint for icon-only buttons; also drives default implicitHeight.
    property int controlSize: AppTheme.buttonHeight
    property int iconSide: 18
    readonly property int squarePad: Math.max(0, (controlSize - iconSide) / 2)

    property bool forceDarkText: true

    // Only Primary fills with a light brand tone in dark mode, so only it needs
    // forced dark text. Other kinds (Secondary/Tonal/Neutral/Error/Text) already
    // carry correct-contrast foregrounds via fgColor in both themes.
    readonly property color effectiveFgColor: {
        if (AppColors.isDark && forceDarkText && kind === AppButton.Primary)
            return root.enabled ? "#000000" : AppColors.withAlpha("#000000", 0.38)
        return root.fgColor
    }

    readonly property color fgColor: {
        if (!root.enabled) {
            if (kind === AppButton.Tonal)
                return AppColors.withAlpha(AppColors.accentContainerFg, 0.38)
            if (kind === AppButton.Neutral)
                return AppColors.withAlpha(AppColors.onSurfaceVariant, 0.38)
            if (kind === AppButton.Error)
                return AppColors.withAlpha(AppColors.error, 0.38)
            return AppColors.disabledContent
        }
        if (kind === AppButton.Primary)
            return AppColors.onPrimary
        if (kind === AppButton.Tonal)
            return AppColors.accentContainerFg
        if (kind === AppButton.Neutral)
            return AppColors.onSurfaceVariant
        if (kind === AppButton.Error)
            return AppColors.error
        if (kind === AppButton.Secondary)
            return AppColors.primaryColor
        return AppColors.primaryText
    }

    flat: true
    implicitHeight: root.controlSize
    implicitWidth: root.iconOnly
                   ? root.controlSize
                   : (implicitContentWidth + leftPadding + rightPadding)
    leftPadding: root.iconOnly ? root.squarePad
                                 : (iconName.length > 0 ? 16 : AppTheme.buttonPaddingH)
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0

    ToolTip.text: root.tooltipText
    ToolTip.visible: hovered && root.tooltipText.length > 0
    ToolTip.delay: 500

    Material.foreground: root.fgColor
    Material.background: "transparent"

    background: Rectangle {
        anchors.fill: parent
        radius: root.iconOnly ? AppTheme.chipRadius : AppTheme.buttonRadius
        border.width: root.kind === AppButton.Secondary ? 1 : 0
        border.color: root.enabled ? AppColors.outline : AppColors.outlineVariant
        color: {
            if (root.kind === AppButton.Tonal) {
                const base = AppColors.accentContainer
                return root.enabled ? base : AppColors.withAlpha(base, 0.35)
            }
            if (root.kind === AppButton.Neutral) {
                const base = AppColors.surfaceContainerHigh
                if (!root.enabled)
                    return AppColors.withAlpha(base, 0.35)
                if (root.pressed || root.down)
                    return AppColors.surfaceContainer
                if (root.hovered)
                    return AppColors.surfaceContainer
                return base
            }
            if (root.kind === AppButton.Error) {
                const base = AppColors.withAlpha(AppColors.error, AppColors.isDark ? 0.22 : 0.14)
                if (!root.enabled)
                    return AppColors.withAlpha(base, 0.35)
                if (root.pressed || root.down)
                    return AppColors.withAlpha(AppColors.error, AppColors.isDark ? 0.32 : 0.22)
                if (root.hovered)
                    return AppColors.withAlpha(AppColors.error, AppColors.isDark ? 0.28 : 0.18)
                return base
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
            size: root.iconSide
            iconColor: root.effectiveFgColor
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: root.iconSide
            Layout.preferredHeight: root.iconSide
            Layout.alignment: Qt.AlignVCenter
        }

        // Text (not Label) — Material Label ignores color and uses theme foreground (black in light).
        Text {
            visible: !root.iconOnly
            text: root.text
            font: root.font
            color: root.effectiveFgColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
