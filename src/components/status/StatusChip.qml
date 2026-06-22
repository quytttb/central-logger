import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import CentralLogger.Components
import CentralLogger.Theme

/// Unified chip: operational status, attach-DI type, or connection indicator.
Item {
    id: root

    property string label: ""
    property bool indicatorActive: false
    property color indicatorActiveColor: AppColors.success
    property color indicatorInactiveColor: AppColors.outline

    property string displayStatus: ""
    property string alarmType: ""

    /// Attach-DI `di_type` code; empty = operational / indicator only.
    property string attachDiTypeCode: ""
    /// Optional label from catalog (custom `di_type`); falls back to standard table.
    property string attachDiTypeLabel: ""

    readonly property bool indicatorMode: label.length > 0
    readonly property bool attachDiTypeMode: !indicatorMode
        && String(attachDiTypeCode || "").trim().length > 0
    readonly property bool operationalMode: !indicatorMode && !attachDiTypeMode

    readonly property string normalizedStatus: String(displayStatus || "").toUpperCase()
    readonly property string normalizedAlarmType: String(alarmType || "").toLowerCase()

    readonly property string chipText: {
        if (indicatorMode) {
            return label
        }
        if (attachDiTypeMode) {
            return AttachDiType.typeLabel(attachDiTypeCode, attachDiTypeLabel)
        }
        return OperationalStatus.statusText(normalizedStatus, normalizedAlarmType)
    }

    readonly property string statusIconName: operationalMode
        ? OperationalStatus.statusIconName(normalizedAlarmType) : ""

    readonly property bool hasContent: indicatorMode || attachDiTypeMode
        || normalizedStatus.length > 0

    readonly property int chipHeight: indicatorMode ? 30 : 24
    readonly property color chipFill: {
        if (indicatorMode) {
            return "transparent"
        }
        if (attachDiTypeMode) {
            return AttachDiType.chipFill(attachDiTypeCode)
        }
        return OperationalStatus.chipFill(normalizedStatus)
    }
    readonly property color chipBorder: {
        if (indicatorMode) {
            return indicatorActive ? indicatorActiveColor : indicatorInactiveColor
        }
        if (attachDiTypeMode) {
            return AttachDiType.chipBorder(attachDiTypeCode)
        }
        return OperationalStatus.chipBorder(normalizedStatus)
    }
    readonly property color chipTextColor: {
        if (indicatorMode) {
            return indicatorActive ? indicatorActiveColor : indicatorInactiveColor
        }
        if (attachDiTypeMode) {
            return AttachDiType.chipTextColor(attachDiTypeCode)
        }
        return OperationalStatus.chipTextColor(normalizedStatus)
    }

    implicitHeight: hasContent ? chipHeight : 0
    implicitWidth: hasContent ? chipBox.width : 0
    visible: hasContent

    Rectangle {
        id: chipBox
        anchors.verticalCenter: parent.verticalCenter
        height: root.chipHeight
        width: chipContent.implicitWidth + (root.indicatorMode ? 28 : 24)
        radius: root.indicatorMode ? height / 2 : AppTheme.chipRadius
        color: root.chipFill
        border.width: 1
        border.color: root.chipBorder

        RowLayout {
            id: chipContent
            anchors.centerIn: parent
            spacing: root.indicatorMode ? 6 : 2

            Rectangle {
                visible: root.indicatorMode
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 10
                Layout.preferredHeight: 10
                radius: width / 2
                color: root.indicatorActive ? root.indicatorActiveColor
                                            : root.indicatorInactiveColor
            }

            UiIcon {
                visible: !root.indicatorMode && root.statusIconName.length > 0
                Layout.alignment: Qt.AlignVCenter
                name: root.statusIconName
                size: AppTheme.iconSizeSm
                iconColor: root.chipTextColor
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: root.chipText
                font: AppTypography.labelSmall
                color: root.chipTextColor
                elide: Text.ElideNone
                maximumLineCount: 1
            }
        }
    }
}
