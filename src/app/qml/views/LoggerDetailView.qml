pragma ComponentBehavior: Bound

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

    onLoggerIdChanged: {
        root._modbusToastKey = ""
        refresh()
        Qt.callLater(root.maybeShowModbusOfflineToast)
    }
    Component.onCompleted: refresh()

    Connections {
        target: LoggerFormController
        function onLoggerUpdated(id) { if (id === root.loggerId) root.refresh() }
        function onLoggerRemoved(id) { if (id === root.loggerId) root.goBack() }
    }

    function refresh() {
        formData = loggerId >= 0 ? LoggerFormController.getLoggerFormData(loggerId) : ({});
    }

    LoggerDetailViewModel {
        id: detailVm
        loggerId: root.loggerId

        onReportDownloaded: (ok, savePath, errorMessage) => {
            if (ok) {
                AppNotifier.show(
                    qsTr("Report saved: %1").arg(DesktopService.fileBaseName(savePath)),
                    "success",
                    { copyPath: savePath }
                );
            } else if (errorMessage.length > 0) {
                AppNotifier.show(
                    qsTr("Report download failed"),
                    "error",
                    { detailText: errorMessage, detailTitle: qsTr("Report download error") }
                );
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

    // Modbus offline / poll fail → toast (not InlineBanner). Edge-trigger per logger.
    property string _modbusToastKey: ""

    function modbusOfflineDetail() {
        const err = detailVm.lastModbusError
        if (err && err.length > 0) {
            return qsTr("Modbus polling failed: %1. REST/config may still work on the API port — verify Modbus host, port (default 5020), and unit ID.").arg(err)
        }
        if (detailVm.hasApiToken) {
            return qsTr("Modbus live data is not available yet. Values in the table below are from config (WAIT) until polling succeeds. REST fetch/apply still works.")
        }
        return qsTr("Logger is offline for Modbus polling. Check host, Modbus port, and that the device is reachable.")
    }

    function maybeShowModbusOfflineToast() {
        if (root.loggerId < 0 || detailVm.online)
            return
        const key = root.loggerId + "|" + detailVm.lastModbusError
        if (key === root._modbusToastKey)
            return
        root._modbusToastKey = key
        const body = root.modbusOfflineDetail()
        AppNotifier.show(
            body.split("\n")[0],
            "warning",
            {
                detailText:  body,
                detailTitle: qsTr("Modbus / logger offline"),
                loggerId:    root.loggerId,
                durationMs:  8000
            }
        )
    }

    Connections {
        target: detailVm
        function onLiveStateChanged() { root.maybeShowModbusOfflineToast() }
        function onLastModbusErrorChanged() { root.maybeShowModbusOfflineToast() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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

                        AppTableView {
                            id: sensorTableView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: detailVm.sensorTable
                            reuseItems: false
                            colWeights:  [0, 0.5, 0, 0, 0.5]
                            colMinimums: [52, 72, 88, 52, 112]
                            headerAlignRight: function(col) { return col === 0 || col === 2 || col === 3 }
                            emptyIconName: "sensors"
                            emptyMessage: qsTr("No sensors in catalog. Fetch config from the device to load the sensor list.")

                            delegate: ItemDelegate {
                                id: sensorCell
                                required property int row
                                required property int column
                                required property var sensorId
                                required property var name
                                required property var value
                                required property var unit
                                required property var displayStatus
                                required property var attachDiTypeCodes
                                required property var attachDiTypeLabels
                                required property var alarmType

                                implicitHeight: 40
                                padding: 0
                                hoverEnabled: false

                                background: TableCellBackground {
                                    cellHovered: false
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

                                    SensorStatusColumn {
                                        visible: sensorCell.column === 4
                                        anchors {
                                            left: parent.left
                                            leftMargin: 8
                                            verticalCenter: parent.verticalCenter
                                        }
                                        displayStatus: sensorCell.displayStatus
                                        alarmType: sensorCell.alarmType
                                        attachDiTypeCodes: sensorCell.attachDiTypeCodes
                                        attachDiTypeLabels: sensorCell.attachDiTypeLabels
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

        AppButton {
            kind: AppButton.Neutral
            forceDarkText: false
            controlSize: 36
            iconSide: 20
            iconName: "arrowLeft"
            text: qsTr("Back")
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.goBack()
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

            StatusChip {
                label: detailVm.online ? qsTr("Online") : qsTr("Offline")
                indicatorActive: detailVm.online
            }
            StatusChip {
                visible: detailVm.online
                label: qsTr("Polling")
                indicatorActive: detailVm.polling
                indicatorActiveColor: AppColors.info
            }
            StatusChip {
                visible: detailVm.online
                label: qsTr("RTU")
                indicatorActive: detailVm.rtuConnected
                indicatorActiveColor: AppColors.graphSeriesColors[0]
            }
            StatusChip {
                visible: detailVm.online
                label: qsTr("Alarm")
                indicatorActive: detailVm.anyAlarm
                indicatorActiveColor: AppColors.error
            }

            Label {
                text: qsTr("%1 sensor(s)").arg(detailVm.sensorTable
                                                    ? detailVm.sensorTable.rowsSize
                                                    : 0)
                color: AppColors.textMuted
                font: AppTypography.bodyMedium
            }

            AppButton {
                kind: AppButton.Neutral
                forceDarkText: false
                iconOnly: true
                controlSize: 36
                iconSide: 20
                iconName: "download"
                enabled: !detailVm.reportBusy && detailVm.hasApiToken
                         && detailVm.online && root.loggerId >= 0
                tooltipText: !detailVm.hasApiToken
                                ? qsTr("Device REST token empty \u2014 scan QR on logger")
                                : !detailVm.online
                                    ? qsTr("Logger must be online to download report")
                                    : qsTr("Download report")
                onClicked: reportSaveDialog.open()
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
