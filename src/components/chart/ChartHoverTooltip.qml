import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtGraphs

import CentralLogger.Theme

// Floating panel for Qt Graphs charts — snap to nearest point via snapAt(mouseX, mouseY).
Item {
    id: root

    anchors.fill: parent
    z: 10

    property GraphsView chart
    /// function(mouseX, mouseY) → { series, position: {x,y}, captionText, valueRows } | null
    property var snapAt: null

    property bool tooltipVisible: false
    property string captionText: ""
    /// { text: string, color?: color }
    property var valueRows: []

    property real tooltipX: 8
    property real tooltipY: 8

    property real _lastMouseX: 0
    property real _lastMouseY: 0

    function hide() {
        tooltipVisible = false
    }

    function showSnap(mouseX, mouseY, hit) {
        if (!hit || !hit.position)
            return
        _lastMouseX = mouseX
        _lastMouseY = mouseY
        captionText = hit.captionText || ""
        valueRows = hit.valueRows || []
        tooltipVisible = true
        Qt.callLater(function() { positionNearMouse(_lastMouseX, _lastMouseY) })
    }

    function positionNearMouse(mouseX, mouseY) {
        const pad = 8
        const tw = tooltipRect.width
        const th = tooltipRect.height
        if (tw <= 0 || th <= 0)
            return
        const pa = chart ? chart.plotArea : null
        const minX = pa && pa.width > 0 ? pa.x + pad : pad
        const maxX = pa && pa.width > 0 ? pa.x + pa.width - tw - pad : width - tw - pad
        const minY = pa && pa.height > 0 ? pa.y + pad : pad
        const maxY = pa && pa.height > 0 ? pa.y + pa.height - th - pad : height - th - pad
        tooltipX = Math.min(Math.max(minX, mouseX + 12), maxX)
        tooltipY = Math.min(Math.max(minY, mouseY - th - 10), maxY)
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        enabled: root.chart && root.snapAt

        onPositionChanged: function(mouse) {
            if (!root.snapAt)
                return
            const hit = root.snapAt(mouse.x, mouse.y)
            if (!hit) {
                root.hide()
                return
            }
            root.showSnap(mouse.x, mouse.y, hit)
        }

        onExited: root.hide()
    }

    Rectangle {
        id: tooltipRect
        x: root.tooltipX
        y: root.tooltipY
        width: contentColumn.implicitWidth + 24
        height: contentColumn.implicitHeight + 16
        radius: AppTheme.listItemRadius
        color: AppColors.surfaceContainerHigh
        border.width: 1
        border.color: AppColors.elevatedBorder
        visible: root.tooltipVisible && (root.captionText.length > 0 || root.valueRows.length > 0)
        z: 20

        onWidthChanged: if (root.tooltipVisible)
                            Qt.callLater(function() {
                                root.positionNearMouse(root._lastMouseX, root._lastMouseY)
                            })
        onHeightChanged: if (root.tooltipVisible)
                             Qt.callLater(function() {
                                 root.positionNearMouse(root._lastMouseX, root._lastMouseY)
                             })

        ColumnLayout {
            id: contentColumn
            anchors.centerIn: parent
            spacing: 4

            Label {
                visible: root.captionText.length > 0
                text: root.captionText
                font: AppTypography.labelSmall
                color: AppColors.textMuted
            }

            Repeater {
                model: root.valueRows

                RowLayout {
                    required property var modelData
                    spacing: 6

                    Rectangle {
                        visible: modelData.color !== undefined
                        width: visible ? 8 : 0
                        height: 8
                        radius: 4
                        color: visible ? modelData.color : "transparent"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Label {
                        text: modelData.text
                        font: AppTypography.labelSmall
                        color: AppColors.primaryText
                    }
                }
            }
        }
    }
}
