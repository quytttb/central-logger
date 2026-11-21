pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

// Flat multi-logger sensor table for Loggers > Data (List mode).
// Columns: Logger · Sensor · Value · Unit · Status · Limit.
ElevatedPane {
    id: root

    property string filterText: ""
    property int    hoveredRow: -1

    signal openGraphRequested(int loggerId)

    padding: 0

    AppTableView {
        id: liveTable
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: DashboardController.liveList
        hasData: DashboardController.liveList.count > 0
        colWeights:  [0.20, 0.24, 0.14, 0.08, 0.20, 0.14]
        colMinimums: [140,  150,  96,   64,   140,  112]
        headerAlignRight: function(col) { return col === 2 || col === 5 }
        emptyIconName: root.filterText.length > 0 ? "magnify" : "sensors"
        emptyMessage: root.filterText.length > 0
                      ? qsTr("No sensors match \"%1\".").arg(root.filterText)
                      : qsTr("No active sensors yet. Configure loggers in Settings > Loggers.")

        HoverHandler {
            onHoveredChanged: if (!hovered) root.hoveredRow = -1
        }

        delegate: ItemDelegate {
            id: cell

            required property int    row
            required property int    column
            required property int    loggerId
            required property string loggerName
            required property string stationCode
            required property bool   online
            required property string name
            required property string sensorType
            required property string value
            required property string unit
            required property string displayStatus
            required property bool   status03Auto
            required property string alarmType
            required property var    attachDiTypeCodes
            required property var    attachDiTypeLabels
            required property bool   hasLimit
            required property string limitText
            required property bool   valid

            readonly property bool isDigital: sensorType === "DI" || sensorType === "DO"
            readonly property bool isInput:   sensorType === "DI"
            readonly property bool inAlarm:   online && (alarmType.length > 0 || displayStatus === "02")

            implicitHeight: 46
            padding: 0
            hoverEnabled: true
            onHoveredChanged: if (hovered) root.hoveredRow = row
            onDoubleClicked: root.openGraphRequested(cell.loggerId)

            background: TableCellBackground {
                cellHovered: root.hoveredRow === cell.row
            }

            contentItem: Item {
                opacity: cell.online ? 1.0 : 0.62

                // Col 0: Logger (status dot + logger name)
                RowLayout {
                    visible: cell.column === 0
                    anchors {
                        fill: parent
                        leftMargin: 12
                        rightMargin: 8
                    }
                    spacing: AppTheme.spacingS

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: cell.online ? AppColors.success : AppColors.error
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Label {
                        Layout.fillWidth: true
                        text: cell.loggerName
                        font.weight: Font.DemiBold
                        color: AppColors.primaryText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Col 1: Sensor (DI/DO pill + sensor name)
                RowLayout {
                    visible: cell.column === 1
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 8
                    }
                    spacing: AppTheme.spacingS

                    Rectangle {
                        visible: cell.isDigital
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 18
                        radius: AppTheme.radiusTiny
                        color: cell.isInput ? IoColors.diStrong : IoColors.doStrong
                        Layout.alignment: Qt.AlignVCenter

                        Label {
                            anchors.centerIn: parent
                            text: cell.isInput ? qsTr("DI") : qsTr("DO")
                            color: AppColors.onPrimary
                            font: AppTypography.labelSmall
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: cell.name
                        color: AppColors.primaryText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Col 2: Value
                Label {
                    visible: cell.column === 2
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 8
                    }
                    text: cell.value
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: AppTypography.monoFamily
                    font.weight: Font.DemiBold
                    color: cell.inAlarm ? AppColors.error
                         : (cell.valid ? AppColors.primaryText : AppColors.tableCellMuted)
                    elide: Text.ElideRight
                }

                // Col 3: Unit
                Label {
                    visible: cell.column === 3
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 8
                    }
                    text: cell.unit
                    verticalAlignment: Text.AlignVCenter
                    color: AppColors.tableHeaderText
                    elide: Text.ElideRight
                }

                // Col 4: Status
                Item {
                    visible: cell.column === 4
                    clip: true
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 8
                    }

                    SensorStatusColumn {
                        visible: cell.online && cell.displayStatus.length > 0
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        displayStatus: cell.displayStatus
                        status03Auto: cell.status03Auto
                        alarmType: cell.alarmType
                        attachDiTypeCodes: cell.attachDiTypeCodes
                        attachDiTypeLabels: cell.attachDiTypeLabels
                    }

                    Label {
                        visible: !cell.online || cell.displayStatus.length === 0
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: cell.online ? "—" : qsTr("Offline")
                        color: AppColors.tableCellMuted
                        font: AppTypography.bodySmall
                    }
                }

                // Col 5: Limit
                Label {
                    visible: cell.column === 5
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 12
                    }
                    text: cell.hasLimit ? cell.limitText : "—"
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: AppTypography.monoFamily
                    color: cell.hasLimit ? AppColors.tableCellMuted : AppColors.textFaint
                    elide: Text.ElideRight
                }
            }
        }
    }
}
