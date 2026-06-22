pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import CentralLogger.Theme

// Reusable data table: empty state + header + auto-resized columns.
Item {
    id: root

    property var model
    property var colWeights: []
    property var colMinimums: []
    property bool reuseItems: true
    property bool hasData: dataTable.rows > 0
    property bool loading: false
    property string emptyIconName: "informationOutline"
    property string emptyMessage: ""
    property var headerAlignRight: function(column) { return false }

    readonly property alias rows: dataTable.rows
    readonly property alias tableView: dataTable
    readonly property var colWidths: dataTable.colWidths

    // Whole-row hover: cells set `hoveredRow = row` on hover and highlight when it
    // matches; resets when the pointer leaves the table.
    property int hoveredRow: -1

    function recomputeColumns() {
        dataTable.recomputeColumns()
    }

    property alias delegate: dataTable.delegate

    HoverHandler {
        onHoveredChanged: if (!hovered) root.hoveredRow = -1
    }

    TableContentStack {
        anchors.fill: parent
        hasData: root.hasData || root.loading
        emptyIconName: root.emptyIconName
        emptyMessage: root.emptyMessage

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            HorizontalHeaderView {
                Layout.fillWidth: true
                Layout.preferredHeight: AppTheme.tableHeaderHeight
                syncView: dataTable
                textRole: "display"

                delegate: TableHeaderCell {
                    cornerRadius: AppTheme.cardRadius
                    roundTopLeft: column === 0
                    roundTopRight: column === dataTable.colWidths.length - 1
                    alignRight: root.headerAlignRight(column)
                }
            }

            TableView {
                id: dataTable
                reuseItems: root.reuseItems
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.model
                clip: true

                property var colWidths: []

                function recomputeColumns() {
                    colWidths = AppTheme.distributeColumnWidths(
                        width - vScrollBar.gutter, root.colWeights, root.colMinimums)
                    forceLayout()
                }

                Component.onCompleted: recomputeColumns()
                onWidthChanged: recomputeColumns()
                Connections {
                    target: vScrollBar
                    function onGutterChanged() { dataTable.recomputeColumns() }
                }

                columnWidthProvider: function(col) {
                    return (col >= 0 && col < colWidths.length)
                           ? colWidths[col] : 80
                }
            }
        }

        AppScrollBar {
            id: vScrollBar
            flickable: dataTable
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: parent.top
            anchors.topMargin: AppTheme.tableHeaderHeight
        }

        BusyIndicator {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            width: 20
            height: 20
            running: root.loading
            visible: running
        }
    }

    onColWeightsChanged: dataTable.recomputeColumns()
    onColMinimumsChanged: dataTable.recomputeColumns()
}
