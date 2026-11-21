pragma ComponentBehavior: Bound

import QtQuick
import QtGraphs

import CentralLogger.Core
import LoggerKit.Theme
import LoggerKit.Components

// Binds ChartGraphsView to C++ chart presentation (single bucket series or multi trending).
Item {
    id: root

    property bool multiSeries: false

    /// Dashboard: [{ bucketMs, count, label }]
    property var plotPoints: []

    /// Detail: [{ label, points: [{ x, y, time }] }]
    property var series: []

    /// Single: { xMin, xMax, yMax }. Multi: { xMin, xMax, yMin, yMax } from computeTrendingAxisRange.
    property var axis: ({})

    property string xLabelFormat: "HH:mm"
    property string yLabelFormat: ""
    property int lineWidth: 2

    /// function(mouseX, mouseY) -> tooltip hit map or null
    property var snapAt: null

    readonly property alias chart: chartHost.chart
    readonly property color primarySeriesColor: multiSeries
        ? AppColors.graphSeriesColors[0]
        : singleSeries.color

    property var _multiSeriesObjects: ({})
    property var _deadSeries: []
    property bool _ready: false

    function updateAxis() {
        if (!_ready)
            return
        if (axis.xMax > axis.xMin) {
            axisX.min = new Date(axis.xMin)
            axisX.max = new Date(axis.xMax)
        }
        const yMin = multiSeries ? axis.yMin ?? 0 : 0
        axisY.min = yMin
        axisY.max = axis.yMax > yMin ? axis.yMax : yMin + 1
    }

    function updatePoints(line, next) {
        if (next.length === 0) {
            if (line.count > 0)
                line.clear()
            return
        }
        let trim = 0
        while (trim < line.count && line.at(trim).x < next[0].x)
            ++trim
        const overlap = line.count - trim
        if (overlap > 0 && overlap <= next.length) {
            let unchanged = true
            for (let i = 0; i < overlap && unchanged; ++i) {
                const point = line.at(trim + i)
                unchanged = point.x === next[i].x && point.y === next[i].y
            }
            if (unchanged) {
                if (trim > 0)
                    line.removeMultiple(0, trim)
                if (overlap < next.length)
                    line.append(next.slice(overlap))
                return
            }
        }
        // Changed bucket values / filters: replace in bulk, don't clear then
        // reappend one by one (which briefly empties the chart).
        line.replace(next)
    }

    function updateSeries() {
        if (!_ready)
            return
        // Qt Graphs can still reference a detached series during polish.
        // Only destroy series left detached by a previous update.
        for (const retired of _deadSeries)
            retired.destroy()
        _deadSeries = []

        const next = ({})
        if (multiSeries) {
            const colors = AppColors.graphSeriesColors
            for (let i = 0; i < series.length; ++i) {
                const row = series[i]
                const key = String(row.edgeSensorId ?? row.id ?? i)
                let line = _multiSeriesObjects[key]
                if (!line) {
                    line = lineSeriesComponent.createObject(chartHost.chart)
                    if (!line)
                        continue
                    chartHost.chart.addSeries(line)
                }
                line.name = row.label
                line.color = colors[i % colors.length]
                updatePoints(line, row.points.map(point => Qt.point(point.x, point.y)))
                next[key] = line
            }
        } else {
            updatePoints(singleSeries, plotPoints.map(row => Qt.point(row.bucketMs, row.count)))
        }

        for (const key in _multiSeriesObjects) {
            if (!next[key]) {
                const extra = _multiSeriesObjects[key]
                chartTooltip.hide()
                chartHost.chart.removeSeries(extra)
                _deadSeries.push(extra)
            }
        }
        _multiSeriesObjects = next
    }

    function scheduleSeriesUpdate() {
        if (_ready)
            Qt.callLater(root.updateSeries)
    }

    Component {
        id: lineSeriesComponent
        LineSeries {
            width: root.lineWidth
            hoverable: true
            pointDelegate: ChartLinePointMarker {}
        }
    }

    ChartGraphsView {
        id: chartHost
        anchors.fill: parent
        timezoneId: SettingsController.systemTimezone

        axisX: DateTimeAxis {
            id: axisX
            labelFormat: root.xLabelFormat
            Component.onCompleted: chartHost.bindSystemTimezone(this)
        }

        axisY: ValueAxis {
            id: axisY
            min: 0
            labelFormat: root.yLabelFormat
        }

        LineSeries {
            id: singleSeries
            visible: !root.multiSeries
            width: root.lineWidth
            hoverable: true
            pointDelegate: ChartLinePointMarker {}
        }

    }

    ChartHoverTooltip {
        id: chartTooltip
        chart: chartHost.chart
        snapAt: function (mouseX, mouseY) {
            return root.snapAt ? root.snapAt(mouseX, mouseY) : null
        }
    }

    Component.onCompleted: {
        _ready = true
        updateAxis()
        scheduleSeriesUpdate()
    }
    onPrimarySeriesColorChanged: if (multiSeries)
        scheduleSeriesUpdate()
    onPlotPointsChanged: if (!multiSeries)
        scheduleSeriesUpdate()
    onSeriesChanged: if (multiSeries)
        scheduleSeriesUpdate()
    onAxisChanged: updateAxis()
    onMultiSeriesChanged: {
        updateAxis()
        scheduleSeriesUpdate()
    }
}
