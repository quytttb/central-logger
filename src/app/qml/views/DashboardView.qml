pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme
import LoggerKit.Theme
import LoggerKit.Components

Item {
    id: root

    property Component topBarToolbar: DashboardTopBar {}

    // 0 = detailed dashboard (traffic chart + events), 1 = summary dashboard
    // (selected logger real-time charts + secondary loggers table).
    property int viewMode: 0

    readonly property bool summaryMode: viewMode === 1
    readonly property int overviewRows: DashboardController.overviewTable.rowsSize
    /// Compact "Other loggers" pane: header + table header + one row per
    /// logger, capped so a long list scrolls inside the table instead of
    /// squeezing the chart grid.
    readonly property int overviewPaneHeight: Math.max(180,
        Math.min(360, AppTheme.cardPadding * 2 + 56
                      + AppTheme.tableHeaderHeight + overviewRows * 40))

    function summaryChartCount() {
        return DashboardController.graphSensorCharts.length
    }

    // Real loggers expose up to 8 ANALOG sensors: prefer a compact 4-column
    // grid (2 rows) instead of filling every available column, which would
    // leave an unbalanced last row.
    function summaryGridColumns() {
        const count = Math.max(1, summaryChartCount())
        const fit = Math.max(1, Math.floor(sensorChartGrid.width / 260))
        if (count <= 4)
            return Math.min(count, fit)
        if (count <= 6)
            return Math.min(3, fit)
        return Math.min(4, fit)
    }

    function summaryGridRows() {
        const count = summaryChartCount()
        if (count <= 0)
            return 1
        return Math.ceil(count / summaryGridColumns())
    }

    function setViewMode(mode) {
        if (viewMode === mode)
            return
        viewMode = mode
        if (mode === 1)
            DashboardController.refreshSummary()
    }

    function selectGraphLogger(loggerId) {
        const items = graphLoggerCombo.model
        const target = Number(loggerId)
        for (let i = 0; i < items.length; ++i) {
            if (Number(items[i].id) === target) {
                graphLoggerCombo.currentIndex = i
                return
            }
        }
        graphLoggerCombo.currentIndex = -1
    }

    function syncGraphLogger() {
        selectGraphLogger(DashboardController.graphLoggerId)
    }

    function summaryColWeights() {
        const sensors = DashboardController.overviewTable.sensorColumnCount
        const weights = [0.28]
        if (sensors > 0) {
            const each = 0.72 / sensors
            for (let i = 0; i < sensors; ++i)
                weights.push(each)
        }
        return weights
    }

    function summaryColMinimums() {
        const sensors = DashboardController.overviewTable.sensorColumnCount
        const minimums = [160]
        for (let i = 0; i < sensors; ++i)
            minimums.push(96)
        return minimums
    }

    signal selectLogger(int loggerId)

    Component.onCompleted: {
        DashboardController.reloadRecentEvents()
        DashboardController.refreshReadingsChart()
        if (root.summaryMode)
            DashboardController.refreshSummary()
    }

    // Refresh chart only while Dashboard tab is open (replaces global C++ timer).
    Timer {
        interval: 30000
        running: !root.summaryMode
        repeat: true
        onTriggered: DashboardController.refreshReadingsChart()
    }

    Connections {
        target: DashboardController
        function onReadingsChartChanged() {
            readingsChart.rebuild()
        }
        function onGraphLoggerChanged() {
            Qt.callLater(root.syncGraphLogger)
        }
        function onGraphLoggerOptionsChanged() {
            Qt.callLater(root.syncGraphLogger)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            spacing: AppTheme.sectionSpacing
            visible: !root.summaryMode

            StatCard {
                Layout.fillWidth: true
                label: qsTr("Total Loggers")
                value: AppState.totalLoggers
            }

            StatCard {
                Layout.fillWidth: true
                label: qsTr("Online")
                value: AppState.onlineLoggers
                semanticNumberColor: true
                numberColor: AppColors.success
            }

            StatCard {
                Layout.fillWidth: true
                label: qsTr("Active Alarms")
                value: AppState.alarmCount
                semanticNumberColor: true
                numberColor: AppState.alarmCount > 0
                             ? AppColors.error
                             : AppColors.onSurfaceVariant
            }
        }

        RowLayout {
            id: bottomRow
            visible: !root.summaryMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.sectionSpacing
            Layout.bottomMargin: AppTheme.pagePadding
            spacing: AppTheme.sectionSpacing

            Item {
                id: chartSlot
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 7
                Layout.minimumWidth: 280
                Layout.minimumHeight: 220

                ElevatedPane {
                    anchors.fill: parent
                    contentSpacing: 8

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Traffic Readings")

                        AppButton {
                            kind: AppButton.Neutral
                            forceDarkText: false
                            iconOnly: true
                            controlSize: 36
                            iconSide: 20
                            iconName: "refresh"
                            tooltipText: qsTr("Refresh")
                            onClicked: DashboardController.refreshReadingsChart()
                        }
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: DashboardController.readingsChartHasData
                        emptyIconName: "showChart"
                        emptyMessage: qsTr("No readings stored yet.")

                        ChartTimeSeriesPanel {
                            id: readingsChart
                            anchors.fill: parent
                            plotPoints: DashboardController.readingsChartPlotPoints
                            axis: DashboardController.readingsChartAxis
                            snapAt: function (mouseX, mouseY) {
                                const pa = readingsChart.chart.plotArea
                                if (!pa || pa.width <= 0 || pa.height <= 0)
                                    return null
                                const hit = DashboardController.snapReadingsChart(
                                    mouseX, mouseY, pa.x, pa.y, pa.width, pa.height)
                                if (!hit || !hit.position)
                                    return null
                                if (hit.valueRows && hit.valueRows.length > 0)
                                    hit.valueRows[0].color = readingsChart.primarySeriesColor
                                return hit
                            }
                        }
                    }
                }
            }

            Item {
                id: eventsSlot
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                Layout.minimumWidth: 200

                ElevatedPane {
                    anchors.fill: parent
                    contentSpacing: AppTheme.toolbarGap

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Recent events")

                        AppButton {
                            kind: AppButton.Neutral
                            forceDarkText: false
                            iconOnly: true
                            controlSize: 36
                            iconSide: 20
                            iconName: "refresh"
                            tooltipText: qsTr("Refresh")
                            onClicked: DashboardController.reloadRecentEvents()
                        }
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: recentEventsList.count > 0
                        emptyIconName: "inbox"
                        emptyMessage: qsTr("No events yet.")

                        ListView {
                            id: recentEventsList
                            anchors.fill: parent
                            anchors.rightMargin: eventsScrollBar.visible ? eventsScrollBar.width + 4 : 0
                            clip: true
                            spacing: 6
                            model: DashboardController.recentEvents
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: RecentEventListItem {
                                onActivated: loggerId => root.selectLogger(loggerId)
                                onDetailRequested: (title, body, loggerId) =>
                                    AppNotifier.openDetail(title, body, loggerId)
                            }
                        }

                        AppScrollBar {
                            id: eventsScrollBar
                            flickable: recentEventsList
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                        }
                    }
                }
            }
        }

        // --- Summary mode: primary logger chart + secondary loggers table ---
        ColumnLayout {
            id: summaryLayout
            visible: root.summaryMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.sectionSpacing
            Layout.bottomMargin: AppTheme.pagePadding
            spacing: AppTheme.sectionSpacing

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Multi-row grids (8-sensor loggers) need the chart pane to
                // win vertical space; single-row grids keep the 3:2 split.
                Layout.preferredHeight: root.summaryGridRows() > 1 ? 2 : 3
                Layout.minimumHeight: 220

                ElevatedPane {
                    anchors.fill: parent
                    contentSpacing: 8

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Show graph")

                        ComboBox {
                            id: graphLoggerCombo
                            Layout.preferredWidth: 240
                            Layout.maximumWidth: 320
                            Layout.preferredHeight: 40
                            Material.containerStyle: Material.Outlined
                            model: DashboardController.graphLoggerOptions
                            textRole: "name"
                            valueRole: "id"
                            font: AppTypography.bodyMedium
                            displayText: count > 0
                                         ? currentText
                                         : qsTr("(No loggers)")
                            onActivated: DashboardController.graphLoggerId =
                                             Number(currentValue)
                            Component.onCompleted: root.syncGraphLogger()
                            onModelChanged: root.syncGraphLogger()
                        }

                        AppButton {
                            kind: AppButton.Neutral
                            forceDarkText: false
                            iconOnly: true
                            controlSize: 36
                            iconSide: 20
                            iconName: "refresh"
                            tooltipText: qsTr("Refresh")
                            onClicked: DashboardController.refreshSummary()
                        }
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: root.summaryChartCount() > 0
                        emptyIconName: "showChart"
                        emptyMessage: DashboardController.graphLoggerId > 0
                                      ? qsTr("Waiting for live data from the selected logger…")
                                      : qsTr("No logger selected. Add a logger, then pick the primary logger in Settings.")

                        Item {
                            anchors.fill: parent

                            GridView {
                                id: sensorChartGrid
                                anchors.fill: parent
                                anchors.rightMargin: sensorChartScroll.gutter
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                // Compact multi-row tiles (real loggers have 8
                                // ANALOG sensors); single-row grids can breathe.
                                cellHeight: {
                                    const rows = root.summaryGridRows()
                                    const maxCell = rows > 1 ? 240 : 300
                                    return Math.round(Math.max(112,
                                        Math.min(maxCell, height / rows)))
                                }
                                cellWidth: {
                                    const cols = root.summaryGridColumns()
                                    // Prefer filling the row when there are fewer
                                    // sensors than fit, but keep very wide tiles
                                    // from stretching a one- or two-sensor grid.
                                    return Math.min(680, Math.max(1, Math.floor(width / cols)))
                                }
                                model: DashboardController.graphSensorCharts

                                delegate: Item {
                                    id: sensorChartCell
                                    required property var modelData

                                    width: sensorChartGrid.cellWidth
                                    height: sensorChartGrid.cellHeight

                                    SensorMiniChart {
                                        anchors.fill: parent
                                        anchors.margins: 3
                                        label: sensorChartCell.modelData.label ?? ""
                                        unit: sensorChartCell.modelData.unit ?? ""
                                        latestText: sensorChartCell.modelData.latestText ?? ""
                                        decimals: sensorChartCell.modelData.decimals ?? 2
                                        points: sensorChartCell.modelData.points ?? []
                                        hasThresholds: sensorChartCell.modelData.hasThresholds ?? false
                                        hasMaxThreshold: sensorChartCell.modelData.hasMaxThreshold ?? false
                                        thresholdMax: sensorChartCell.modelData.thresholdMax ?? 0
                                        outOfRange: sensorChartCell.modelData.outOfRange ?? false
                                        axisYMin: sensorChartCell.modelData.axisYMin ?? 0
                                        axisYMax: sensorChartCell.modelData.axisYMax ?? 1
                                        xMinMs: DashboardController.graphChartAxis.xMin ?? 0
                                        xMaxMs: DashboardController.graphChartAxis.xMax ?? 0
                                    }
                                }
                            }

                            AppScrollBar {
                                id: sensorChartScroll
                                flickable: sensorChartGrid
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                // Multi-row grids get the compact table pane and hand the
                // remaining height to the chart tiles; a single row keeps
                // the original 3:2 split so the page doesn't look empty.
                Layout.fillHeight: root.summaryGridRows() <= 1
                Layout.preferredHeight: root.summaryGridRows() > 1
                                        ? root.overviewPaneHeight
                                        : 2
                Layout.minimumHeight: 160

                ElevatedPane {
                    anchors.fill: parent
                    padding: 0

                    SectionHeader {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        Layout.topMargin: AppTheme.sectionSpacing
                        Layout.bottomMargin: AppTheme.toolbarGap
                        title: qsTr("Other loggers")
                    }

                    AppTableView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: DashboardController.overviewTable
                        hasData: DashboardController.overviewTable.rowsSize > 0
                        colWeights: root.summaryColWeights()
                        colMinimums: root.summaryColMinimums()
                        headerAlignRight: function(col) { return col > 0 }
                        emptyIconName: "server"
                        emptyMessage: DashboardController.graphLoggerId > 0
                                      ? qsTr("No other loggers configured.")
                                      : qsTr("Add more loggers to see their live values here.")

                        delegate: ItemDelegate {
                            id: overviewCell
                            required property int    row
                            required property int    column
                            required property string value
                            required property bool   online
                            required property bool   isLoggerColumn

                            implicitHeight: 40
                            padding: 0
                            hoverEnabled: false

                            background: TableCellBackground {
                                cellHovered: false
                            }

                            contentItem: Item {
                                Label {
                                    anchors {
                                        left: parent.left
                                        leftMargin: overviewCell.isLoggerColumn ? 28 : 8
                                        right: parent.right
                                        rightMargin: 8
                                        verticalCenter: parent.verticalCenter
                                    }
                                    text: overviewCell.value
                                    horizontalAlignment: overviewCell.isLoggerColumn
                                                         ? Text.AlignLeft : Text.AlignRight
                                    font.family: overviewCell.isLoggerColumn ? "" : AppTypography.monoFamily
                                    font.weight: overviewCell.isLoggerColumn ? Font.DemiBold : Font.Normal
                                    color: overviewCell.isLoggerColumn
                                           ? AppColors.primaryText
                                           : (overviewCell.value.length > 0
                                              ? AppColors.primaryText
                                              : AppColors.tableCellMuted)
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: overviewCell.isLoggerColumn
                                    anchors {
                                        left: parent.left
                                        leftMargin: 12
                                        verticalCenter: parent.verticalCenter
                                    }
                                    width: 8
                                    height: 8
                                    radius: width / 2
                                    color: overviewCell.online ? AppColors.success : AppColors.error
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component DashboardTopBar: RowLayout {
        spacing: AppTheme.toolbarGap

        Rectangle {
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: width / 2
            color: AppColors.success
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            text: AppState.statusText
            font: AppTypography.bodyMedium
            color: AppColors.textSoft
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        AppButton {
            kind: root.summaryMode ? AppButton.Neutral : AppButton.Primary
            text: qsTr("Detail")
            iconName: "showChart"
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.setViewMode(0)
        }

        AppButton {
            kind: root.summaryMode ? AppButton.Primary : AppButton.Neutral
            text: qsTr("Summary")
            iconName: "filterList"
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.setViewMode(1)
        }
    }
}
