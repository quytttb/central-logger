import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

Item {
    id: root

    property Component topBarToolbar: LoggerDetailTopBar {}

    property int loggerId: -1
    property var formData: ({})

    signal goBack()

    readonly property bool isWide: width > AppTheme.detailWideBreakpoint

    onLoggerIdChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: DashboardController
        function onLoggerUpdated(id) { if (id === root.loggerId) root.refresh() }
        function onLoggerRemoved(id) { if (id === root.loggerId) root.goBack() }
    }

    function refresh() {
        formData = loggerId >= 0 ? DashboardController.getLoggerFormData(loggerId) : ({});
    }

    LoggerDetailViewModel {
        id: detailVm
        loggerId: root.loggerId

        onReportDownloaded: (ok, savePath, errorMessage) => {
            if (ok) {
                reportSuccessLabel.text = qsTr("Report saved to: %1").arg(savePath);
                reportSuccessLabel.visible = true;
            }
        }
    }

    FileDialog {
        id: reportSaveDialog
        title: qsTr("Save Report")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        onAccepted: {
            // M-21: pass the QUrl directly so C++ can call toLocalFile(),
            // which correctly decodes %20 and other percent-encoded chars.
            detailVm.downloadReport(selectedFile);
        }
    }

    Connections {
        target: detailVm
        function onTrendingSeriesChanged() {
            trendingChart.rebuild()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        InlineBanner {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            visible: root.loggerId >= 0 && !detailVm.online
            semantic: "warning"
            message: {
                var err = detailVm.lastModbusError
                if (err && err.length > 0)
                    return qsTr("Modbus polling failed: %1. REST/config may still work on the API port — verify Modbus host, port (default 5020), and unit ID.").arg(err)
                if (detailVm.hasApiToken)
                    return qsTr("Modbus live data is not available yet. Values in the table below are from config (WAIT) until polling succeeds. REST fetch/apply still works.")
                return qsTr("Logger is offline for Modbus polling. Check host, Modbus port, and that the device is reachable.")
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.toolbarGap
            visible: detailVm.lastError.length > 0
            text: detailVm.lastError
            color: AppColors.error
            wrapMode: Text.WordWrap
        }

        Label {
            id: reportSuccessLabel
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: 4
            visible: false
            color: AppColors.success
            wrapMode: Text.WordWrap
        }

        // Responsive container: side-by-side or stacked at detailWideBreakpoint
        GridLayout {
            id: contentGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding

            columns: root.isWide ? 2 : 1
            columnSpacing: root.isWide ? 16 : 0
            rowSpacing: root.isWide ? 0 : 16

            Item {
                id: sensorWrap
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 280
                Layout.minimumHeight: root.isWide ? 220 : 200

                ElevatedPane {
                    id: sensorPane
                    anchors.fill: parent
                    padding: 0

                    SectionHeader {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        Layout.topMargin: AppTheme.sectionSpacing
                        Layout.bottomMargin: AppTheme.toolbarGap
                        title: qsTr("Sensors")
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: sensorTableView.rows > 0
                        emptyIconName: "sensors"
                        emptyMessage: qsTr("No sensors in catalog. Fetch config from the device to load the sensor list.")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            HorizontalHeaderView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: AppTheme.tableHeaderHeight
                                    syncView: sensorTableView
                                    textRole: "display"

                                    delegate: TableHeaderCell {
                                        cornerRadius: AppTheme.cardRadius
                                        roundTopLeft: column === 0
                                        roundTopRight: column === sensorTableView.colWidths.length - 1
                                        alignRight: column === 0 || column === 2 || column === 3
                                    }
                                }

                                TableView {
                                    id: sensorTableView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: detailVm.sensorTable
                                    clip: true
                                    reuseItems: true

                                    readonly property var colWeights:  [0, 1, 0, 0, 0.35]
                                    readonly property var colMinimums: [52, 96, 88, 52, 72]
                                    property var colWidths: []

                                    function recomputeColumns() {
                                        colWidths = AppTheme.distributeColumnWidths(
                                            width, colWeights, colMinimums)
                                        forceLayout()
                                    }

                                    Component.onCompleted: recomputeColumns()
                                    onWidthChanged: recomputeColumns()

                                    columnWidthProvider: function(col) {
                                        return (col >= 0 && col < colWidths.length)
                                               ? colWidths[col] : 80
                                    }

                                    delegate: ItemDelegate {
                                        id: sensorCell
                                        required property int row
                                        required property int column
                                        required property var sensorId
                                        required property var name
                                        required property var value
                                        required property var unit
                                        required property var displayStatus
                                        required property var diStatusCode
                                        required property var alarmType
                                        required property var showAlarmBadge

                                        implicitHeight: 40
                                        padding: 0
                                        hoverEnabled: true

                                        background: TableCellBackground {
                                            cellHovered: sensorCell.hovered
                                            rowIndex: sensorCell.row
                                        }

                                        contentItem: Item {
                                            Label {
                                                anchors {
                                                    left: parent.left;   leftMargin: sensorCell.column === 0 ? 16 : 8
                                                    right: parent.right; rightMargin: 8
                                                    verticalCenter: parent.verticalCenter
                                                }
                                                visible: sensorCell.column !== 4
                                                text: {
                                                    switch (sensorCell.column) {
                                                    case 0: return sensorCell.sensorId !== undefined ? String(sensorCell.sensorId) : ""
                                                    case 1: return sensorCell.name || ""
                                                    case 2: return sensorCell.value !== undefined ? String(sensorCell.value) : ""
                                                    case 3: return sensorCell.unit || ""
                                                    default: return ""
                                                    }
                                                }
                                                horizontalAlignment: sensorCell.column === 0 || sensorCell.column === 2
                                                                     ? Text.AlignRight : Text.AlignLeft
                                                font.family: sensorCell.column === 0 || sensorCell.column === 2
                                                             ? "monospace" : ""
                                                font.weight: sensorCell.column === 2 ? Font.DemiBold : Font.Normal
                                                color: sensorCell.column === 3 ? AppColors.tableHeaderText
                                                       : sensorCell.column === 0 ? AppColors.tableCellMuted
                                                       : AppColors.primaryText
                                                elide: Text.ElideRight
                                            }

                                            SensorStatusChip {
                                                visible: sensorCell.column === 4
                                                anchors {
                                                    left: parent.left
                                                    leftMargin: 8
                                                    right: parent.right
                                                    rightMargin: 16
                                                    verticalCenter: parent.verticalCenter
                                                }
                                                displayStatus: sensorCell.displayStatus
                                                diStatusCode: sensorCell.diStatusCode || ""
                                                alarmType: sensorCell.alarmType || ""
                                                showAlarmBadge: !!sensorCell.showAlarmBadge
                                            }
                                        }
                                    }
                                }
                            }
                    }
                }
            }

            Item {
                id: chartWrap
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 280
                Layout.preferredHeight: root.isWide ? -1 : 220
                Layout.minimumHeight: 220

                ElevatedPane {
                    id: chartPane
                    anchors.fill: parent
                    contentSpacing: AppTheme.toolbarGap

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Analog trending (last %1 samples)").arg(20)
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: detailVm.trendingSeries.length > 0
                        emptyIconName: "showChart"
                        emptyMessage: detailVm.online
                                    ? qsTr("Waiting for poll data…")
                                    : qsTr("No trending history yet. Chart fills in after successful Modbus polls.")

                        ChartTimeSeriesPanel {
                                id: trendingChart
                                anchors.fill: parent
                                multiSeries: true
                                series: detailVm.trendingSeries
                                axis: detailVm.chartAxisRange
                                xLabelFormat: "HH:mm:ss"
                                yLabelFormat: "%.1f"
                                snapAt: function (mouseX, mouseY) {
                                    const pa = trendingChart.chart.plotArea
                                    if (!pa || pa.width <= 0 || pa.height <= 0)
                                        return null
                                    const hit = detailVm.snapTrendingChart(
                                        mouseX, mouseY, pa.x, pa.y, pa.width, pa.height)
                                    if (!hit || !hit.position)
                                        return null
                                    const colors = AppColors.graphSeriesColors
                                    for (let i = 0; i < hit.valueRows.length; ++i) {
                                        const si = hit.valueRows[i].seriesIndex
                                        if (si !== undefined)
                                            hit.valueRows[i].color = colors[si % colors.length]
                                    }
                                    return hit
                                }
                            }
                        }
                    }
                }
            }
        }

    component LoggerDetailTopBar: RowLayout {
        spacing: 12

        ToolButton {
            implicitHeight: 36
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.goBack()
            contentItem: RowLayout {
                spacing: 4
                UiIcon {
                    name: "arrowLeft"
                    size: 20
                    iconColor: AppColors.primaryText
                }
                Label {
                    text: qsTr("Back")
                    color: AppColors.primaryText
                    font: AppTypography.labelLarge
                }
            }
        }

        ColumnLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: Math.min(420, parent.width * 0.4)

            Label {
                text: root.formData.name
                      ? root.formData.name
                      : qsTr("Logger #%1").arg(root.loggerId >= 0 ? root.loggerId : "\u2014")
                font: AppTypography.titleMedium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                text: root.formData.host
                      ? qsTr("Modbus %1:%2  \u00B7  API port %3")
                            .arg(root.formData.host)
                            .arg(root.formData.modbusPort)
                            .arg(root.formData.apiPort)
                      : qsTr("Detail placeholder")
                visible: text.length > 0
                color: AppColors.textMuted
                font: AppTypography.bodyMedium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            spacing: AppTheme.toolbarGap
            Layout.alignment: Qt.AlignVCenter

            StatusBadge {
                label: detailVm.online ? qsTr("Online") : qsTr("Offline")
                active: detailVm.online
            }
            StatusBadge {
                visible: detailVm.online
                label: qsTr("Polling")
                active: detailVm.polling
                activeColor: AppColors.info
            }
            StatusBadge {
                visible: detailVm.online
                label: qsTr("RTU")
                active: detailVm.rtuConnected
                activeColor: AppColors.graphSeriesColors[0]
            }
            StatusBadge {
                visible: detailVm.online
                label: qsTr("Alarm")
                active: detailVm.anyAlarm
                activeColor: AppColors.error
            }

            Label {
                text: qsTr("%1 sensor(s)").arg(detailVm.sensorTable
                                                    ? detailVm.sensorTable.rowsSize
                                                    : 0)
                color: AppColors.textMuted
                font: AppTypography.bodyMedium
            }

            IconToolButton {
                iconName: "download"
                enabled: !detailVm.reportBusy && detailVm.hasApiToken
                         && detailVm.online && root.loggerId >= 0
                iconColor: enabled ? AppColors.primaryText : AppColors.disabledContent
                tooltipText: !detailVm.hasApiToken
                                ? qsTr("Device REST token empty \u2014 scan QR on logger")
                                : !detailVm.online
                                    ? qsTr("Logger must be online to download report")
                                    : qsTr("Download report")
                onClicked: {
                    reportSuccessLabel.visible = false;
                    reportSaveDialog.open();
                }
            }

            BusyIndicator {
                running: detailVm.reportBusy
                visible: running
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
        }
    }
}
