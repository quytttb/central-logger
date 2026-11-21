pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Theme
import LoggerKit.Theme
import LoggerKit.Components

// Single logger block in Loggers > Data (Cards mode):
// Header (status dot + name/station + sensor count + timestamp + alarm chip + graph shortcut)
// + responsive multi-column grid of shared SensorValueCard tiles.
ElevatedPane {
    id: root

    property int    loggerId: 0
    property string loggerName: ""
    property string stationCode: ""
    property string host: ""
    property bool   loggerEnabled: true
    property bool   online: false
    property bool   polling: false
    property bool   anyAlarm: false
    property int    sensorCount: 0
    property string lastUpdate: ""
    property var    sensors: []

    signal openGraphRequested(int loggerId)

    readonly property int itemCount: root.sensors ? root.sensors.length : 0
    readonly property bool compactHeader: width < AppDefaults.wideLiveHeaderWidth
    readonly property int gridColumns: {
        const count = Math.max(1, root.itemCount)
        const avail = Math.max(220, gridHost.width)
        const fit = Math.max(1, Math.floor((avail + AppTheme.spacingSM) / (216 + AppTheme.spacingSM)))
        if (count <= 4)
            return Math.min(count, fit)
        if (count <= 6)
            return Math.min(3, fit)
        return Math.min(4, fit)
    }

    contentSpacing: AppTheme.spacingSM
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    // ── Header ────────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: AppTheme.spacingS

        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            Layout.alignment: Qt.AlignVCenter
            radius: 5
            color: !root.loggerEnabled ? AppColors.outline
                 : root.online ? AppColors.success : AppColors.error
        }

        Label {
            text: root.loggerName
            font: AppTypography.titleMedium
            color: AppColors.primaryText
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: 320
            HoverHandler { id: nameHover }
            ToolTip.visible: nameHover.hovered && (root.compactHeader || truncated)
            ToolTip.text: root.stationCode.length > 0
                ? qsTr("%1 · %2").arg(root.loggerName).arg(root.stationCode) : root.loggerName
        }

        Label {
            visible: !root.compactHeader && root.stationCode.length > 0
            text: root.stationCode
            font: AppTypography.labelSmall
            color: AppColors.textMuted
            elide: Text.ElideRight
            Layout.maximumWidth: 160
            Layout.alignment: Qt.AlignVCenter
        }

        StatusChip {
            visible: !root.loggerEnabled
            label: qsTr("Disabled")
            indicatorActive: false
            Layout.alignment: Qt.AlignVCenter
        }

        StatusChip {
            visible: root.online && root.anyAlarm
            label: qsTr("Alarm")
            indicatorActive: true
            indicatorActiveColor: AppColors.error
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        Label {
            visible: !root.compactHeader
            text: root.sensorCount === 1 ? qsTr("1 sensor") : qsTr("%1 sensors").arg(root.sensorCount)
            font: AppTypography.bodySmall
            color: AppColors.textMuted
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            visible: root.loggerEnabled
            text: root.online && root.lastUpdate.length > 0
                  ? qsTr("Updated %1").arg(root.lastUpdate)
                  : (root.online ? qsTr("Online") : qsTr("Offline"))
            font: AppTypography.bodySmall
            color: root.online ? AppColors.textMuted : AppColors.onSurfaceVariant
            Layout.alignment: Qt.AlignVCenter
        }

        AppButton {
            kind: AppButton.Neutral
            forceDarkText: false
            iconOnly: true
            controlSize: 32
            iconSide: 18
            iconName: "showChart"
            tooltipText: qsTr("Show graph")
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.openGraphRequested(root.loggerId)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        implicitHeight: 1
        color: AppColors.dividerLine
    }

    // ── Sensor Cards Body ─────────────────────────────────────────────
    Item {
        id: gridHost
        Layout.fillWidth: true
        implicitHeight: root.itemCount > 0 ? cardsGrid.implicitHeight : emptyLabel.implicitHeight + AppTheme.spacingS
        opacity: root.online ? 1.0 : 0.62

        Label {
            id: emptyLabel
            visible: root.itemCount === 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("No active sensors in catalog.")
            font: AppTypography.bodyMedium
            color: AppColors.textMuted
            elide: Text.ElideRight
        }

        GridLayout {
            id: cardsGrid
            visible: root.itemCount > 0
            width: parent.width
            columns: root.gridColumns
            columnSpacing: AppTheme.spacingSM
            rowSpacing: AppTheme.spacingSM

            // Bind Repeater to integer count so live polls update card properties
            // in place without destroying or recreating delegates.
            Repeater {
                model: root.itemCount

                delegate: SensorValueCard {
                    id: cardDelegate
                    required property int index

                    readonly property var s: (root.sensors && index < root.sensors.length)
                                             ? root.sensors[index] : ({})
                    readonly property string sType: String(s.sensorType ?? "")
                    readonly property bool   isAnalog: sType === "ANALOG"
                    readonly property string sStatus: String(s.displayStatus ?? "")
                    readonly property bool   sAuto03: Boolean(s.status03Auto ?? false)
                    readonly property string sAlarm: String(s.alarmType ?? "")
                    readonly property bool   sHasLimit: Boolean(s.hasLimit ?? false)
                    readonly property bool   alarmActive: root.online && isAnalog && sAlarm.length > 0
                    readonly property string colorCode: (sStatus === "00" && sAlarm.length > 0)
                                                        ? "02" : sStatus

                    Layout.fillWidth: true
                    Layout.preferredWidth: 216
                    compact: true
                    filled: true
                    fillColor: alarmActive ? AppColors.errorContainer
                              : root.online && isAnalog && valid && sHasLimit && sStatus !== "02"
                                ? AppColors.successContainer : AppColors.surfaceContainer

                    name: String(s.name ?? "")
                    unit: String(s.unit ?? "")
                    value: String(s.value ?? "—")
                    sensorType: sType
                    valid: Boolean(s.valid ?? false)
                    digitalOn: Boolean(s.digitalOn ?? false)

                    limitText: isAnalog && sHasLimit ? String(s.limitText ?? "") : ""

                    attachDiLabels: s.attachDiTypeLabels ?? []

                    statusText: (root.online && isAnalog && sStatus.length > 0)
                                ? OperationalStatus.statusText(sStatus, sAlarm, sAuto03)
                                : ""
                    statusColor: (root.online && isAnalog && sStatus.length > 0)
                                 ? (colorCode === "02" ? AppColors.errorContainerFg : AppColors.primaryText)
                                 : AppColors.outlineVariant
                    statusFill: (root.online && isAnalog && sStatus.length > 0)
                                ? OperationalStatus.chipFill(colorCode)
                                : AppColors.withAlpha(AppColors.outlineVariant, 0.12)

                    alarmText: !alarmActive ? ""
                             : sAlarm === "min"     ? qsTr("▼ MIN")
                             : sAlarm === "max"     ? qsTr("▲ MAX")
                             : sAlarm === "min+max" ? qsTr("▼▲ MIN/MAX")
                             : qsTr("ALARM")

                    valueColor: alarmActive ? AppColors.errorContainerFg
                                : (root.online && sStatus === "02") ? AppColors.error
                                : AppColors.primaryText
                }
            }
        }
    }
}
