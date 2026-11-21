pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

// Loggers screen:
//   - Tab "Data": live values for all loggers (Cards via LoggerLiveSection or List via LiveSensorListTable)
//   - Tab "Graph": real-time per-sensor mini charts ("Show graph" selector) + "Other loggers" overview table
Item {
    id: root

    property string initialTab: "data" // "data" | "graph"
    property int    tabIndex: initialTab === "graph" ? 1 : 0
    property string searchText: ""

    property Component topBarToolbar: LoggersTopBar {}

    readonly property bool isGraphTab: root.tabIndex === 1
    readonly property bool compactToolbar: width < AppDefaults.wideToolbarWidth
    readonly property bool hasGraphLogger: DashboardController.graphLoggerId > 0
    readonly property bool isListMode: SettingsController.loggersViewMode === "list"
    readonly property int  overviewRows: DashboardController.overviewTable.rowsSize
    readonly property int  overviewPaneHeight: Math.max(180,
        Math.min(360, AppTheme.cardPadding * 2 + 56
                      + AppTheme.tableHeaderHeight + overviewRows * 40))

    function openGraphForLogger(loggerId) {
        if (loggerId > 0)
            DashboardController.graphLoggerId = loggerId
        root.tabIndex = 1
        Qt.callLater(root.syncGraphLogger)
    }

    function graphChartCount() {
        return sensorChartGrid.count
    }

    function graphGridColumns() {
        const count = Math.max(1, graphChartCount())
        const fit = Math.max(1, Math.floor(sensorChartGrid.width / 260))
        if (count <= 4)
            return Math.min(count, fit)
        if (count <= 6)
            return Math.min(3, fit)
        return Math.min(4, fit)
    }

    function graphGridRows() {
        const count = graphChartCount()
        if (count <= 0)
            return 1
        return Math.ceil(count / graphGridColumns())
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

    function overviewColWeights() {
        const sensors = DashboardController.overviewTable.sensorColumnCount
        const weights = [0.28]
        if (sensors > 0) {
            const each = 0.72 / sensors
            for (let i = 0; i < sensors; ++i)
                weights.push(each)
        }
        return weights
    }

    function overviewColMinimums() {
        const sensors = DashboardController.overviewTable.sensorColumnCount
        const minimums = [160]
        for (let i = 0; i < sensors; ++i)
            minimums.push(96)
        return minimums
    }

    onIsGraphTabChanged: {
        DashboardController.setGraphActive(root.isGraphTab)
        if (root.isGraphTab) {
            Qt.callLater(root.syncGraphLogger)
        }
    }

    Component.onCompleted: {
        DashboardController.setLiveFilterText("")
        DashboardController.setGraphActive(root.isGraphTab)
        if (root.isGraphTab) {
            Qt.callLater(root.syncGraphLogger)
        }
    }

    Component.onDestruction: {
        DashboardController.setGraphActive(false)
        DashboardController.setLiveFilterText("")
    }

    Connections {
        target: DashboardController
        function onGraphLoggerChanged() {
            Qt.callLater(root.syncGraphLogger)
        }
        function onGraphLoggerOptionsChanged() {
            Qt.callLater(root.syncGraphLogger)
        }
    }

    // ══════════════════════════════════════════════════════════════════
    // Content Area
    // ══════════════════════════════════════════════════════════════════
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Tab 0: Data (Cards | List) ────────────────────────────────
        Item {
            visible: !root.isGraphTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding

            // Empty state for Cards mode (List mode uses AppTableView's built-in empty state)
            ElevatedPane {
                visible: !root.isListMode && DashboardController.liveCards.count === 0
                anchors.fill: parent

                EmptyStatePlaceholder {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    iconName: root.searchText.trim().length > 0 ? "magnify" : "server"
                    message: root.searchText.trim().length > 0
                             ? qsTr("No loggers or sensors match \"%1\".").arg(root.searchText.trim())
                             : qsTr("No loggers configured yet. Open Settings > Loggers to add a device.")
                }
            }

            // Cards mode: scrollable per-logger sections (in-place updates preserve scroll)
            Item {
                visible: !root.isListMode && DashboardController.liveCards.count > 0
                anchors.fill: parent

                ListView {
                    id: cardsListView
                    anchors.fill: parent
                    anchors.rightMargin: cardsScrollBar.visible ? cardsScrollBar.width + 6 : 0
                    clip: true
                    spacing: AppTheme.sectionSpacing
                    cacheBuffer: 2400
                    boundsBehavior: Flickable.StopAtBounds
                    model: DashboardController.liveCards

                    delegate: LoggerLiveSection {
                        required loggerId
                        required loggerName
                        required stationCode
                        required host
                        required loggerEnabled
                        required online
                        required polling
                        required anyAlarm
                        required sensorCount
                        required lastUpdate
                        required sensors

                        width: cardsListView.width
                        onOpenGraphRequested: targetId => root.openGraphForLogger(targetId)
                    }
                }

                AppScrollBar {
                    id: cardsScrollBar
                    flickable: cardsListView
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }
            }

            // List mode: flat 6-column sensor table across all loggers
            LiveSensorListTable {
                visible: root.isListMode
                anchors.fill: parent
                filterText: root.searchText.trim()
                onOpenGraphRequested: targetId => root.openGraphForLogger(targetId)
            }
        }

        // ── Tab 1: Graph ("Show graph" + "Other loggers") ─────────────
        ColumnLayout {
            id: graphLayout
            visible: root.isGraphTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding
            spacing: AppTheme.sectionSpacing

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: root.graphGridRows() > 1 ? 2 : 3
                Layout.minimumHeight: 220

                ElevatedPane {
                    anchors.fill: parent
                    contentSpacing: AppTheme.spacingS

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

                        Label {
                            text: DashboardController.graphWindowLabel
                            visible: root.hasGraphLogger
                            color: AppColors.textMuted
                            font: AppTypography.labelSmall
                        }

                        RowLayout {
                            visible: root.hasGraphLogger
                            spacing: AppTheme.spacingXS

                            AppButton {
                                objectName: "graphZoomOutButton"
                                kind: AppButton.Neutral
                                text: "−"
                                Layout.preferredWidth: controlSize
                                leftPadding: 0
                                rightPadding: 0
                                tooltipText: qsTr("Zoom out (longer time window)")
                                Accessible.name: tooltipText
                                enabled: DashboardController.canZoomGraphOut
                                onClicked: DashboardController.zoomGraphOut()
                            }

                            AppButton {
                                objectName: "graphZoomInButton"
                                kind: AppButton.Neutral
                                iconOnly: true
                                iconName: "plus"
                                tooltipText: qsTr("Zoom in (shorter time window)")
                                Accessible.name: tooltipText
                                enabled: DashboardController.canZoomGraphIn
                                onClicked: DashboardController.zoomGraphIn()
                            }

                            AppButton {
                                objectName: "graphResetZoomButton"
                                kind: AppButton.Neutral
                                iconOnly: true
                                iconName: "restartAlt"
                                tooltipText: qsTr("Reset graph zoom")
                                Accessible.name: tooltipText
                                enabled: !DashboardController.atDefaultGraphZoom
                                onClicked: DashboardController.resetGraphZoom()
                            }
                        }

                        AppButton {
                            kind: AppButton.Neutral
                            forceDarkText: false
                            iconOnly: true
                            controlSize: 36
                            iconSide: 20
                            iconName: "refresh"
                            iconSpinning: root.isGraphTab && DashboardController.graphHistoryLoading
                            enabled: root.hasGraphLogger && !DashboardController.graphHistoryLoading
                            tooltipText: qsTr("Refresh graph")
                            Accessible.name: tooltipText
                            onClicked: DashboardController.refreshGraphHistory()
                        }
                    }

                    InlineBanner {
                        Layout.fillWidth: true
                        message: DashboardController.graphHistoryError
                        visible: message.length > 0
                    }

                    TableContentStack {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hasData: root.graphChartCount() > 0
                        emptyIconName: "showChart"
                        emptyMessage: !root.hasGraphLogger
                                      ? qsTr("No logger selected. Add a logger in Settings > Loggers.")
                                      : DashboardController.graphHistoryLoading
                                        ? qsTr("Loading stored readings…")
                                        : qsTr("No readings in this time window.")

                        Item {
                            anchors.fill: parent

                            GridView {
                                id: sensorChartGrid
                                anchors.fill: parent
                                anchors.rightMargin: sensorChartScroll.gutter
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                cellHeight: {
                                    const rows = root.graphGridRows()
                                    const maxCell = rows > 1 ? 240 : 300
                                    return Math.round(Math.max(112,
                                        Math.min(maxCell, parent.height / rows)))
                                }
                                cellWidth: {
                                    const cols = root.graphGridColumns()
                                    return Math.min(680, Math.max(1, Math.floor(width / cols)))
                                }
                                 model: DashboardController.graphCharts
                                 reuseItems: true

                                 delegate: Item {
                                     id: sensorChartCell
                                     required property var chart

                                    width: sensorChartGrid.cellWidth
                                    height: sensorChartGrid.cellHeight

                                     SensorMiniChart {
                                         objectName: "sensorMiniChart"
                                         anchors.fill: parent
                                         anchors.margins: 3
                                         compact: sensorChartGrid.cellHeight < 190 || sensorChartGrid.cellWidth < 280
                                         label: sensorChartCell.chart.label
                                         unit: sensorChartCell.chart.unit
                                         latestText: sensorChartCell.chart.latestText
                                         decimals: sensorChartCell.chart.decimals
                                         points: sensorChartCell.chart.points
                                         hasThresholds: sensorChartCell.chart.hasThresholds
                                         hasMaxThreshold: sensorChartCell.chart.hasMaxThreshold
                                         thresholdMax: sensorChartCell.chart.thresholdMax
                                         outOfRange: sensorChartCell.chart.outOfRange
                                         axisYMin: sensorChartCell.chart.axisYMin
                                         axisYMax: sensorChartCell.chart.axisYMax
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
                Layout.fillHeight: root.graphGridRows() <= 1
                Layout.preferredHeight: root.graphGridRows() > 1
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
                        colWeights: root.overviewColWeights()
                        colMinimums: root.overviewColMinimums()
                        headerAlignRight: function(col) { return col > 0 }
                        emptyIconName: "server"
                        emptyMessage: DashboardController.graphLoggerId > 0
                                      ? qsTr("No other loggers configured.")
                                      : qsTr("Add more loggers in Settings > Loggers to compare live values here.")

                        delegate: ItemDelegate {
                            id: overviewCell
                            required property int    row
                            required property int    column
                            required property int    loggerId
                            required property string value
                            required property bool   online
                            required property bool   isLoggerColumn

                            implicitHeight: 40
                            padding: 0
                            hoverEnabled: false
                            onDoubleClicked: {
                                if (overviewCell.loggerId > 0)
                                    DashboardController.graphLoggerId = overviewCell.loggerId
                            }

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
                                    radius: 4
                                    color: overviewCell.online ? AppColors.success : AppColors.error
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════
    // Top Bar Toolbar
    // ══════════════════════════════════════════════════════════════════
    component LoggersTopBar: RowLayout {
        spacing: AppTheme.toolbarGap

        TabBar {
            id: modeTabs
            Layout.preferredWidth: 216
            Layout.alignment: Qt.AlignVCenter
            currentIndex: root.tabIndex
            onCurrentIndexChanged: root.tabIndex = currentIndex
            spacing: AppTheme.spacingXS
            padding: 3

            background: Rectangle {
                color: AppColors.surfaceContainer
                radius: AppTheme.listItemRadius
                border.color: AppColors.elevatedBorder
                border.width: 1
            }

            contentItem: ListView {
                model: modeTabs.contentModel
                currentIndex: modeTabs.currentIndex
                spacing: modeTabs.spacing
                orientation: ListView.Horizontal
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.AutoFlickIfNeeded
                snapMode: ListView.SnapToItem
            }

            ThemedTabButton {
                text: qsTr("Data")
                iconName: "sensors"
            }

            ThemedTabButton {
                text: qsTr("Graph")
                iconName: "showChart"
            }
        }

        TextField {
            visible: !root.isGraphTab
            Layout.fillWidth: true
            Layout.minimumWidth: 150
            Layout.preferredWidth: 340
            Layout.maximumWidth: 420
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignVCenter
            Material.containerStyle: Material.Outlined
            placeholderText: qsTr("Search logger, station, sensor, or unit…")
            text: root.searchText
            leftPadding: 36
            onTextEdited: {
                root.searchText = text
                DashboardController.setLiveFilterText(text)
            }

            UiIcon {
                name: "magnify"
                size: AppTheme.iconSizeSm
                iconColor: AppColors.iconSubtle
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            visible: !root.isGraphTab
            spacing: AppTheme.spacingXS
            Layout.alignment: Qt.AlignVCenter

            AppButton {
                kind: !root.isListMode ? AppButton.Primary : AppButton.Neutral
                text: qsTr("Cards")
                iconName: "viewDashboard"
                iconOnly: root.compactToolbar
                tooltipText: text
                Accessible.name: text
                onClicked: SettingsController.saveLoggersViewMode("cards")
            }

            AppButton {
                kind: root.isListMode ? AppButton.Primary : AppButton.Neutral
                text: qsTr("List")
                iconName: "filterList"
                iconOnly: root.compactToolbar
                tooltipText: text
                Accessible.name: text
                onClicked: SettingsController.saveLoggersViewMode("list")
            }
        }

        Label {
            visible: !root.compactToolbar
            text: AppState.totalLoggers === 1 ? qsTr("1 logger")
                : qsTr("%1 loggers").arg(AppState.totalLoggers)
            color: AppColors.textMuted
            font: AppTypography.bodyMedium
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: AppTheme.spacingS
        }
    }
}
