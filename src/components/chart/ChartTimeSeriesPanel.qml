pragma ComponentBehavior: Bound

import QtQuick
import QtGraphs

import CentralLogger.Core
import CentralLogger.Theme

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

    property var _multiSeriesObjects: []

    function rebuild() {
        if (multiSeries)
            rebuildMultiSeries()
        else
            rebuildSingleSeries()
    }

    function clearMultiSeries() {
        chartTooltip.hide()
        while (_multiSeriesObjects.length > 0) {
            const extra = _multiSeriesObjects.pop()
            chartHost.chart.removeSeries(extra)
            extra.destroy()
        }
    }

    function rebuildSingleSeries() {
        clearMultiSeries()
        singleSeries.visible = true

        singleSeries.clear()
        for (let i = 0; i < plotPoints.length; ++i) {
            const row = plotPoints[i]
            singleSeries.append(new Date(row.bucketMs), row.count)
        }

        if (axis.xMin !== undefined && axis.xMax !== undefined) {
            axisX.min = new Date(axis.xMin)
            axisX.max = new Date(axis.xMax)
            axisY.min = 0
            axisY.max = axis.yMax > 0 ? axis.yMax : 1
        }
    }

    function rebuildMultiSeries() {
        singleSeries.visible = false
        singleSeries.clear()

        const data = series
        if (!data || data.length === 0) {
            clearMultiSeries()
            return
        }

        if (axis.yMin !== undefined && axis.yMax !== undefined) {
            axisY.min = axis.yMin
            axisY.max = axis.yMax
        }
        if (axis.xMin !== undefined && axis.xMax !== undefined) {
            axisX.min = new Date(axis.xMin)
            axisX.max = new Date(axis.xMax)
        }

        const colors = AppColors.graphSeriesColors

        while (_multiSeriesObjects.length < data.length) {
            const lineSeries = lineSeriesComponent.createObject(chartHost.chart)
            if (!lineSeries)
                break
            chartHost.chart.addSeries(lineSeries)
            _multiSeriesObjects.push(lineSeries)
        }
        while (_multiSeriesObjects.length > data.length) {
            const extra = _multiSeriesObjects.pop()
            chartHost.chart.removeSeries(extra)
            extra.destroy()
        }

        for (let s = 0; s < data.length; ++s) {
            const seriesData = data[s]
            const pts = seriesData.points
            const lineSeries = _multiSeriesObjects[s]
            lineSeries.name = seriesData.label
            lineSeries.color = colors[s % colors.length]
            lineSeries.clear()
            for (let p = 0; p < pts.length; ++p)
                lineSeries.append(new Date(pts[p].x), pts[p].y)
        }
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

        Component.onCompleted: root.rebuild()
    }

    ChartHoverTooltip {
        id: chartTooltip
        chart: chartHost.chart
        snapAt: function (mouseX, mouseY) {
            return root.snapAt ? root.snapAt(mouseX, mouseY) : null
        }
    }

    Connections {
        target: SettingsController
        function onThemeChanged() {
            root.rebuild()
        }
    }

    onPlotPointsChanged: if (!multiSeries)
        rebuild()
    onSeriesChanged: if (multiSeries)
        rebuild()
    onAxisChanged: rebuild()
    onMultiSeriesChanged: rebuild()
}
