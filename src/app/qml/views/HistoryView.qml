pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

// Global History page — sidebar navigation.
Item {
    id: root

    property Component topBarToolbar: HistoryTopBar {}

    HistoryViewModel {
        id: histVm

        onExportFinished: (ok, message) => {
            if (ok) {
                AppNotifier.show(
                    qsTr("CSV exported: %1").arg(DesktopService.fileBaseName(message)),
                    "success",
                    { copyPath: message }
                )
            } else {
                AppNotifier.show(
                    qsTr("CSV export failed"),
                    "error",
                    { detailText: message, detailTitle: qsTr("Export error") }
                )
            }
        }
    }

    readonly property HistoryViewModel historyVm: histVm

    FileDialog {
        id: csvSaveDialog
        title: qsTr("Export CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV files (*.csv)"), qsTr("All files (*)")]
        defaultSuffix: "csv"
        onAccepted: histVm.exportCsv(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        InlineBanner {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            visible: histVm.lastError.length > 0
            semantic: "error"
            message: histVm.lastError
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: histVm.lastError.length > 0 ? AppTheme.sectionSpacing : AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding

            ElevatedPane {
                anchors.fill: parent
                padding: 0
                contentSpacing: 0

                AppTableView {
                    id: histTableView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: histVm.tableModel
                    loading: histVm.loading
                    reuseItems: false
                    hasData: histVm.tableModel.rowsSize > 0
                    // col: Time, Logger, Sensor, Unit, Value, Status
                    colWeights: histVm.showLoggerColumn
                                ? [0.28, 0.12, 0.12, 0, 0.28, 0.2]
                                : [0.34, 0, 0.14, 0, 0.34, 0.18]
                    colMinimums: histVm.showLoggerColumn
                                 ? [176, 88, 88, 56, 104, 72]
                                 : [176, 0, 88, 56, 104, 72]
                    headerAlignRight: function(col) { return col === 4 }
                    emptyIconName: "history"
                    emptyMessage: !histVm.searchedOnce
                                  ? qsTr("Set filters and press Search to view history.")
                                  : qsTr("No records found for the selected filters.")

                    delegate: ItemDelegate {
                        id: histCell
                        required property int    row
                        required property int    column
                        required property string time
                        required property string logger
                        required property string sensor
                        required property string unit
                        required property string value
                        required property string status
                        required property bool   alarm
                        required property bool   stale
                        required property bool   valid

                        implicitHeight: 40
                        padding: 0
                        hoverEnabled: true
                        onHoveredChanged: if (hovered) histTableView.hoveredRow = row

                        background: TableCellBackground {
                            cellHovered: histTableView.hoveredRow === histCell.row
                        }

                        contentItem: Item {
                            Label {
                                anchors {
                                    left:            parent.left
                                    leftMargin:      histCell.column === 0 ? 16 : 8
                                    right:           parent.right
                                    rightMargin:     8
                                    verticalCenter:  parent.verticalCenter
                                }
                                visible: histCell.column !== 5
                                text: {
                                    switch (histCell.column) {
                                    case 0: return histCell.time
                                    case 1: return histCell.logger
                                    case 2: return histCell.sensor
                                    case 3: return histCell.unit
                                    case 4: return histCell.value
                                    default: return ""
                                    }
                                }
                                horizontalAlignment: histCell.column === 4
                                                     ? Text.AlignRight : Text.AlignLeft
                                font.family:  histCell.column === 4 ? AppTypography.monoFamily : ""
                                font.weight:  histCell.column === 4 ? Font.DemiBold : Font.Normal
                                color: {
                                    if (histCell.column === 3) return AppColors.tableHeaderText
                                    if (histCell.column === 0) return AppColors.tableCellMuted
                                    return AppColors.primaryText
                                }
                                elide: (histCell.column === 0 || histCell.column === 4)
                                       ? Text.ElideNone : Text.ElideRight
                            }

                            Rectangle {
                                visible:         histCell.column === 5
                                anchors {
                                    left:           parent.left
                                    leftMargin:     8
                                    verticalCenter: parent.verticalCenter
                                }
                                width:  statusLabel.implicitWidth + 16
                                height: 22
                                radius: AppTheme.chipRadius
                                color: {
                                    if (!histCell.valid) return AppColors.withAlpha(AppColors.error, 0.18)
                                    if (histCell.stale)  return AppColors.withAlpha(AppColors.warning, 0.18)
                                    if (histCell.alarm)  return AppColors.withAlpha(AppColors.error, 0.18)
                                    return AppColors.withAlpha(AppColors.success, 0.18)
                                }

                                Label {
                                    id: statusLabel
                                    anchors.centerIn: parent
                                    text: histCell.status
                                    font: AppTypography.labelSmall
                                    color: {
                                        if (!histCell.valid) return AppColors.error
                                        if (histCell.stale)  return AppColors.warning
                                        if (histCell.alarm)  return AppColors.error
                                        return AppColors.success
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component HistoryTopBar: RowLayout {
        id: historyTopBar
        spacing: AppTheme.toolbarGap

        function runSearch() {
            root.historyVm.search(fromField.text, toField.text, sensorCombo.currentValue)
        }

        function runRefresh() {
            root.historyVm.refresh(fromField.text, toField.text, sensorCombo.currentValue)
        }

        function historyRefreshIntervalMs() {
            return Math.max(1, SettingsController.historyFlushIntervalS) * 1000
        }

        // Periodic auto-refresh — interval from Settings (history_flush_interval_s).
        Timer {
            id: historyRefreshTimer
            interval: historyTopBar.historyRefreshIntervalMs()
            repeat: true
            running: root.historyVm.searchedOnce
            onTriggered: historyTopBar.runRefresh()
        }

        Connections {
            target: SettingsController
            function onHistoryFlushIntervalSChanged() {
                historyRefreshTimer.interval = historyTopBar.historyRefreshIntervalMs()
            }
            function onSaved() {
                historyRefreshTimer.interval = historyTopBar.historyRefreshIntervalMs()
            }
        }

        // Initial load only (All loggers + default date range). Filter changes require Search.
        Component.onCompleted: Qt.callLater(historyTopBar.runSearch)

        ComboBox {
            id: loggerCombo
            Layout.preferredWidth: 150
            Layout.preferredHeight: 40
            model: histVm.loggerItems
            textRole: "name"
            valueRole: "id"
            font: AppTypography.bodyMedium
            displayText: currentIndex >= 0 ? currentText : qsTr("(All loggers)")
            onActivated: {
                histVm.loggerId = currentValue
                sensorCombo.currentIndex = 0
            }
        }

        DateField {
            id: fromField
            Layout.preferredWidth: 118
            Layout.preferredHeight: 40
            placeholderText: qsTr("From")
            initialDate: {
                const d = new Date()
                d.setDate(d.getDate() - 7)
                return d
            }
        }

        DateField {
            id: toField
            Layout.preferredWidth: 118
            Layout.preferredHeight: 40
            placeholderText: qsTr("To")
            initialDate: new Date()
        }

        ComboBox {
            id: sensorCombo
            Layout.preferredWidth: 180
            Layout.maximumWidth: 280
            Layout.preferredHeight: 40
            model: histVm.sensorItems
            textRole: "name"
            valueRole: "id"
            font: AppTypography.bodyMedium
            displayText: currentIndex >= 0 ? currentText : qsTr("(All sensors)")
        }

        AppButton {
            text: qsTr("Search")
            iconName: "magnify"
            Layout.alignment: Qt.AlignVCenter
            onClicked: historyTopBar.runSearch()
        }

        Item { Layout.fillWidth: true }

        Label {
            visible: root.historyVm.recordCount > 0
            text: root.historyVm.displayedCount < root.historyVm.recordCount
                  ? qsTr("%1 record(s) (showing latest %2)")
                        .arg(root.historyVm.recordCount)
                        .arg(root.historyVm.displayedCount)
                  : qsTr("%1 record(s)").arg(root.historyVm.recordCount)
            color: AppColors.textMuted
            font: AppTypography.bodyMedium
            Layout.alignment: Qt.AlignVCenter
        }

        AppButton {
            text: qsTr("Refresh")
            iconName: "refresh"
            Layout.alignment: Qt.AlignVCenter
            onClicked: historyTopBar.runRefresh()
        }

        AppButton {
            kind: AppButton.Neutral
            forceDarkText: false
            iconOnly: true
            controlSize: 36
            iconSide: 20
            iconName: "download"
            Layout.alignment: Qt.AlignVCenter
            enabled: histVm.recordCount > 0
            tooltipText: qsTr("Export CSV")
            onClicked: csvSaveDialog.open()
        }
    }
}
