pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import CentralLogger.Core
import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

// Global History page — sidebar navigation.
Item {
    id: root

    property string exportFromDate: ""
    property string exportToDate: ""

    property Component topBarToolbar: HistoryTopBar {}

    HistoryViewModel {
        id: histVm

        onExportFinished: (ok, message) => {
            if (ok) {
                AppNotifier.show(
                    qsTr("CSV exported: %1").arg(ClipboardService.fileBaseName(message)),
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

    FileDialog {
        id: averageSaveDialog
        title: qsTr("Export hourly averages (CSV)")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV files (*.csv)"), qsTr("All files (*)")]
        defaultSuffix: "csv"
        onAccepted: histVm.exportAverageCsv(selectedFile, root.exportFromDate, root.exportToDate)
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
                    objectName: "historyTable"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: histVm.tableModel
                    loading: histVm.loading && !hasData
                    reuseItems: true
                    hasData: histVm.tableModel.rowsSize > 0
                    // col: Time, Logger, Sensor, Unit, Value, Status
                    colWeights: histVm.showLoggerColumn
                                ? [0.28, 0.12, 0.12, 0, 0.28, 0.2]
                                : [0.34, 0, 0.14, 0, 0.34, 0.18]
                    colMinimums: histVm.showLoggerColumn
                                 ? [176, 88, 88, 56, 104, 104]
                                 : [176, 0, 88, 56, 104, 104]
                    headerAlignRight: function(col) { return col === 4 }
                    emptyIconName: "history"
                    emptyMessage: !histVm.searchedOnce
                                  ? qsTr("Set filters and press Search to view history.")
                                  : qsTr("No records found for the selected filters.")

                    // TableView creates cells near the viewport only. Capture
                    // the top reading ID before prepending/culling rows.
                    property double anchorId: 0
                    property int anchorRow: -1
                    property real anchorOffset: 0

                    Connections {
                        target: histVm.tableModel

                        function onAboutToMergeRows() {
                            const table = histTableView.tableView
                            const row = table.topRow
                            const cell = table.itemAtCell(Qt.point(0, row))
                            histTableView.anchorId = (!table.atYBeginning && !table.moving && cell)
                                ? histVm.tableModel.readingIdAt(row) : 0
                            histTableView.anchorRow = row
                            histTableView.anchorOffset = cell ? table.contentY - cell.y : 0
                        }

                        function onRowsMerged() {
                            const table = histTableView.tableView
                            if (histTableView.anchorId <= 0)
                                return
                            const row = histVm.tableModel.rowForReadingId(histTableView.anchorId)
                            if (row >= 0 && row !== histTableView.anchorRow) {
                                table.positionViewAtRow(row, TableView.AlignTop, histTableView.anchorOffset)
                            }
                        }
                    }

                    delegate: Item {
                        id: histCell
                        required property int    column
                        required property string display
                        required property string statusCode

                        implicitHeight: 40
                        HoverHandler { id: cellHover }
                        TableCellBackground {
                            cellHovered: cellHover.hovered
                        }

                        Label {
                            anchors {
                                left: parent.left
                                leftMargin: histCell.column === 0 ? 16 : 8
                                right: parent.right
                                rightMargin: 8
                                verticalCenter: parent.verticalCenter
                            }
                            visible: histCell.column !== 5
                            text: histCell.column === 5 ? "" : histCell.display
                            horizontalAlignment: histCell.column === 4 ? Text.AlignRight : Text.AlignLeft
                            font.family: histCell.column === 4 ? AppTypography.monoFamily : ""
                            font.weight: histCell.column === 4 ? Font.DemiBold : Font.Normal
                            color: {
                                if (histCell.column === 3) return AppColors.tableHeaderText
                                if (histCell.column === 0) return AppColors.tableCellMuted
                                return AppColors.primaryText
                            }
                            elide: Text.ElideRight
                        }

                        Loader {
                            active: histCell.column === 5
                            anchors.left: parent.left
                            anchors.leftMargin: AppTheme.spacingS
                            anchors.verticalCenter: parent.verticalCenter
                            sourceComponent: SensorStatusChip {
                                displayStatus: histCell.statusCode
                                // A stored 03 does not identify the device-specific variant.
                                chipText: histCell.display.length > 0 ? histCell.display : "—"
                            }
                        }
                    }
                }
            }
        }
    }

    component HistoryTopBar: ColumnLayout {
        id: historyTopBar
        spacing: AppTheme.toolbarGap
        readonly property bool compact: width < AppDefaults.widePaneWidth

        function runSearch() {
            root.historyVm.search(fromField.text, toField.text, sensorCombo.currentValue)
        }

        function runRefresh() {
            root.historyVm.refresh(fromField.text, toField.text, sensorCombo.currentValue)
        }

        // Periodic auto-refresh — interval from Settings (history_flush_interval_s).
        Timer {
            interval: Math.max(1, SettingsController.historyFlushIntervalS) * 1000
            repeat: true
            running: root.historyVm.searchedOnce
            onTriggered: {
                if (!histTableView.tableView.moving)
                    histVm.refreshCurrent()
            }
        }

        // Initial load only (All loggers + default date range). Filter changes require Search.
        Component.onCompleted: Qt.callLater(historyTopBar.runSearch)

        RowLayout {
            Layout.fillWidth: true
            spacing: AppTheme.toolbarGap

            ComboBox {
                id: loggerCombo
                Layout.preferredWidth: 180
                Layout.preferredHeight: AppTheme.buttonHeight
                model: histVm.loggerItems
                textRole: "name"
                valueRole: "id"
                font: AppTypography.bodyMedium
                Accessible.name: qsTr("Logger")
                displayText: currentIndex >= 0 ? currentText : qsTr("(All loggers)")
                onActivated: {
                    histVm.loggerId = currentValue
                    sensorCombo.currentIndex = 0
                }
            }

            DateField {
                id: fromField
                Layout.preferredWidth: 136
                Layout.preferredHeight: AppTheme.buttonHeight
                placeholderText: qsTr("From")
                Accessible.name: qsTr("Start date")
                initialDate: {
                    const d = new Date()
                    d.setDate(d.getDate() - 7)
                    return d
                }
            }

            DateField {
                id: toField
                Layout.preferredWidth: 136
                Layout.preferredHeight: AppTheme.buttonHeight
                placeholderText: qsTr("To")
                Accessible.name: qsTr("End date")
                initialDate: new Date()
            }

            ComboBox {
                id: sensorCombo
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                Layout.minimumWidth: 120
                Layout.maximumWidth: 320
                Layout.preferredHeight: AppTheme.buttonHeight
                model: histVm.sensorItems
                textRole: "name"
                valueRole: "id"
                font: AppTypography.bodyMedium
                Accessible.name: qsTr("Sensor")
                displayText: currentIndex >= 0 ? currentText : qsTr("(All sensors)")
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: AppTheme.toolbarGap

            AppButton {
                text: qsTr("Search")
                iconName: "magnify"
                enabled: !histVm.loading
                onClicked: historyTopBar.runSearch()
            }

            Label {
                visible: !historyTopBar.compact && histVm.recordCount > 0
                text: {
                    const count = histVm.recordCount === 1 ? qsTr("1 record")
                        : qsTr("%1 records").arg(histVm.recordCount)
                    return histVm.displayedCount < histVm.recordCount
                        ? qsTr("%1 (showing latest %2)").arg(count).arg(histVm.displayedCount)
                        : count
                }
                color: AppColors.textMuted
                font: AppTypography.bodyMedium
            }

            Item { Layout.fillWidth: true }

            AppButton {
                visible: !historyTopBar.compact
                kind: AppButton.Neutral
                text: qsTr("Refresh")
                iconName: "refresh"
                enabled: !histVm.loading
                onClicked: historyTopBar.runRefresh()
            }

            AppButton {
                id: exportButton
                objectName: "historyActions"
                kind: AppButton.Neutral
                text: historyTopBar.compact ? qsTr("Actions") : qsTr("Export CSV")
                iconName: historyTopBar.compact ? "menu" : "download"
                enabled: historyTopBar.compact || histTableView.hasData || histVm.loggerId > 0
                onClicked: exportMenu.popup(exportButton, 0, exportButton.height)
            }
        }

        Menu {
            id: exportMenu
            MenuItem {
                visible: historyTopBar.compact
                text: qsTr("Refresh")
                enabled: !histVm.loading
                onTriggered: historyTopBar.runRefresh()
            }

            MenuItem {
                text: qsTr("Export table CSV")
                enabled: histTableView.hasData && !histVm.loading
                onTriggered: csvSaveDialog.open()
            }

            MenuItem {
                text: qsTr("Export hourly averages + 24h")
                enabled: histVm.loggerId > 0
                onTriggered: {
                    root.exportFromDate = fromField.text
                    root.exportToDate = toField.text
                    averageSaveDialog.open()
                }
            }
        }
    }
}
