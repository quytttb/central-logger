pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

Item {
    id: root

    property Component topBarToolbar: DashboardTopBar {}

    signal selectLogger(int loggerId)

    Component.onCompleted: {
        DashboardController.reloadRecentEvents()
        DashboardController.refreshReadingsChart()
    }

    // Refresh chart only while Dashboard tab is open (replaces global C++ timer).
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: DashboardController.refreshReadingsChart()
    }

    Connections {
        target: DashboardController
        function onReadingsChartChanged() {
            readingsChart.rebuild()
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
    }
}
