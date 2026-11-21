pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import CentralLogger.Core
import LoggerKit.Theme

// One ANALOG sensor tile on Loggers > Graph.
//
// Background follows the latest live value vs. the configured thresholds
// (success container = within limits, error container = out of range, neutral
// surface when no threshold is set). A yellow horizontal line marks the max
// threshold, matching the CEMS reference layout. When the limit is far above
// the current readings the axis stays data-driven (so the trend fills the
// tile) and the limit is shown as a small off-scale marker instead.
//
// The sparkline is painted on a Canvas instead of a GraphsView: Qt Graphs
// aborts (SIGFPE in QGraphsView::updatePolish) when a small tile is polished
// with a degenerate axis domain, which is unavoidable for tiles created before
// the first live sample arrives. A canvas keeps the summary grid robust.
Rectangle {
    id: root

    property string label: ""
    property string unit: ""
    property string latestText: ""
    property int    decimals: 2
    property bool   compact: false
    /// [{ x: epochMs, y: value }]
    property var points: []

    property bool   hasThresholds: false
    property bool   hasMaxThreshold: false
    property double thresholdMax: 0
    property bool   outOfRange: false

    property double axisYMin: 0
    property double axisYMax: 1
    property double xMinMs: 0
    property double xMaxMs: 0

    readonly property color stateColor: outOfRange ? AppColors.errorContainer
        : hasThresholds ? AppColors.successContainer : AppColors.surfaceContainerLow
    readonly property color gridColor: AppColors.divider(AppColors.primaryText)

    radius: AppTheme.cardRadius
    color: stateColor
    border.width: 1
    border.color: AppColors.elevatedBorder

    function resolvedYRange() {
        let yMin = root.axisYMin
        let yMax = root.axisYMax
        if (!(yMax > yMin))
            yMax = yMin + 1
        return { yMin: yMin, yMax: yMax }
    }

    function formatAxisValue(value, digits) {
        const places = digits === undefined
                      ? Math.max(0, Math.min(6, root.decimals))
                      : Math.max(0, Math.min(6, digits))
        return Number(value).toFixed(places)
    }

    // A tight data window (e.g. temperature moving 24.48–24.52) can make the
    // min/mid/max ticks round to the same string at the configured decimals.
    // Add precision until the three labels differ (capped at 6 places).
    function axisLabelDigits(yMin, yMax) {
        let digits = Math.max(0, Math.min(6, root.decimals))
        while (digits < 6) {
            const top = Number(yMax).toFixed(digits)
            const mid = Number((yMax + yMin) / 2).toFixed(digits)
            const bottom = Number(yMin).toFixed(digits)
            if (top !== mid || mid !== bottom)
                break
            ++digits
        }
        return digits
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 12
        spacing: root.compact ? 4 : 6

        RowLayout {
            Layout.fillWidth: true
            spacing: root.compact ? 4 : 6

            Label {
                Layout.alignment: Qt.AlignBaseline
                text: root.latestText
                font: Qt.font({
                    family: AppTypography.monoFamily,
                    pixelSize: root.compact ? 22 : 28,
                    weight: Font.DemiBold
                })
                color: AppColors.primaryText
            }

            Label {
                visible: root.unit.length > 0
                Layout.alignment: Qt.AlignBaseline
                text: root.unit
                font: root.compact ? AppTypography.labelTiny : AppTypography.labelSmall
                color: AppColors.onSurfaceVariant
            }

            Item { Layout.fillWidth: true }

            Label {
                Layout.alignment: Qt.AlignBaseline
                Layout.maximumWidth: Math.max(80, root.width * 0.42)
                text: root.label
                font: root.compact ? AppTypography.labelSmall : AppTypography.labelLarge
                color: AppColors.onSurfaceVariant
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }
        }

        Canvas {
            id: spark
            Layout.fillWidth: true
            Layout.fillHeight: true
            renderStrategy: Canvas.Immediate

            function paintIfVisible() {
                if (spark.available && spark.visible)
                    spark.requestPaint()
            }

            function scheduleRepaint() {
                if (visible)
                    Qt.callLater(spark.paintIfVisible)
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (width < 60 || height < 54)
                    return

                const pts = root.points || []
                const yr = root.resolvedYRange()
                const xSpan = root.xMaxMs - root.xMinMs
                const ySpan = yr.yMax - yr.yMin
                if (!(xSpan > 0) || !(ySpan > 0))
                    return

                const axisDigits = root.axisLabelDigits(yr.yMin, yr.yMax)
                const yTopText = root.formatAxisValue(yr.yMax, axisDigits)
                const yMidText = root.formatAxisValue((yr.yMax + yr.yMin) / 2,
                                                      axisDigits)
                const yBottomText = root.formatAxisValue(yr.yMin, axisDigits)

                ctx.font = AppTypography.labelTiny.pixelSize + "px "
                           + AppTypography.monoFamily

                // Reserve space for signed ticks instead of moving the plot
                // every time a reading crosses a decimal boundary.
                const labelGap = AppTheme.spacingXS
                const gutter = Math.min(Math.ceil(ctx.measureText("-0000.000000").width) + 2 * labelGap,
                                        Math.floor(width * 0.29))
                const maxLabel = root.hasMaxThreshold
                    ? qsTr("Max %1").arg(root.formatAxisValue(root.thresholdMax)) : ""
                const rightGutter = maxLabel.length > 0
                    ? Math.min(Math.ceil(ctx.measureText(maxLabel).width) + 2 * labelGap,
                               Math.floor(width * 0.32)) : labelGap
                const plotTop = AppTypography.labelTiny.pixelSize / 2 + 1
                const plotBottom = AppTypography.labelTiny.pixelSize + AppTheme.spacingS
                const plotX = gutter
                const plotY = plotTop
                const plotW = width - gutter - rightGutter
                const plotH = height - plotTop - plotBottom
                if (plotW < 40 || plotH < 30)
                    return

                function px(x) { return plotX + (x - root.xMinMs) / xSpan * plotW }
                function py(y) { return plotY + (1 - (y - yr.yMin) / ySpan) * plotH }

                // --- grid ---
                ctx.strokeStyle = root.gridColor
                ctx.lineWidth = 1
                ctx.beginPath()
                for (let i = 0; i <= 2; ++i) {
                    const gy = Math.round(plotY + plotH * i / 2) + 0.5
                    ctx.moveTo(plotX, gy)
                    ctx.lineTo(plotX + plotW, gy)
                    const gx = Math.round(plotX + plotW * i / 2) + 0.5
                    ctx.moveTo(gx, plotY)
                    ctx.lineTo(gx, plotY + plotH)
                }
                ctx.stroke()

                // --- y tick labels (min / mid / max) ---
                ctx.fillStyle = AppColors.onSurfaceVariant
                ctx.textAlign = "right"
                ctx.textBaseline = "middle"
                ctx.fillText(yTopText, plotX - labelGap, plotY, gutter - labelGap)
                ctx.fillText(yMidText, plotX - labelGap, plotY + plotH / 2, gutter - labelGap)
                ctx.fillText(yBottomText, plotX - labelGap, plotY + plotH, gutter - labelGap)

                // Like Data Logger Trending, use seconds on short windows
                // and include the date when the span exceeds a day.
                const tStart = SettingsController.formatGraphTime(root.xMinMs, xSpan)
                const tMid = SettingsController.formatGraphTime(root.xMinMs + xSpan / 2, xSpan)
                const tEnd = SettingsController.formatGraphTime(root.xMaxMs, xSpan)
                const tickY = plotY + plotH + labelGap
                ctx.textBaseline = "top"
                ctx.textAlign = "left"
                const leftWidth = ctx.measureText(tStart).width
                const rightWidth = ctx.measureText(tEnd).width
                const midWidth = ctx.measureText(tMid).width
                if (leftWidth + rightWidth + labelGap <= plotW)
                    ctx.fillText(tStart, plotX, tickY)
                if (2 * Math.max(leftWidth, rightWidth) + midWidth + 2 * labelGap <= plotW) {
                    ctx.textAlign = "center"
                    ctx.fillText(tMid, plotX + plotW / 2, tickY)
                }
                ctx.textAlign = "right"
                ctx.fillText(tEnd, plotX + plotW, tickY, plotW)

                // --- plot contents clipped to the plot rect ---
                ctx.save()
                ctx.beginPath()
                ctx.rect(plotX, plotY, plotW, plotH)
                ctx.clip()

                // --- trend line + soft area fill ---
                if (pts.length >= 2) {
                    const firstX = px(pts[0].x)
                    const lastX = px(pts[pts.length - 1].x)

                    ctx.beginPath()
                    ctx.moveTo(firstX, py(pts[0].y))
                    for (let i = 1; i < pts.length; ++i)
                        ctx.lineTo(px(pts[i].x), py(pts[i].y))
                    ctx.lineTo(lastX, plotY + plotH)
                    ctx.lineTo(firstX, plotY + plotH)
                    ctx.closePath()
                    ctx.fillStyle = AppColors.withAlpha(AppColors.primaryText, 0.08)
                    ctx.fill()

                    ctx.beginPath()
                    ctx.moveTo(firstX, py(pts[0].y))
                    for (let i = 1; i < pts.length; ++i)
                        ctx.lineTo(px(pts[i].x), py(pts[i].y))
                    ctx.strokeStyle = AppColors.primaryText
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.lineCap = "round"
                    ctx.stroke()
                }

                const maxY = py(root.thresholdMax)
                const maxVisible = root.hasMaxThreshold && maxY >= plotY && maxY <= plotY + plotH
                if (maxVisible) {
                    ctx.save()
                    ctx.strokeStyle = AppColors.warning
                    ctx.lineWidth = 1.5
                    ctx.setLineDash([4, 3])
                    ctx.beginPath()
                    ctx.moveTo(plotX, maxY)
                    ctx.lineTo(plotX + plotW, maxY)
                    ctx.stroke()
                    ctx.restore()
                }

                // --- latest sample dot ---
                if (pts.length > 0) {
                    const last = pts[pts.length - 1]
                    const dotX = px(last.x)
                    const dotY = py(last.y)
                    ctx.fillStyle = root.stateColor
                    ctx.beginPath()
                    ctx.arc(dotX, dotY, 4, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.fillStyle = root.outOfRange ? AppColors.error
                                                    : AppColors.primaryText
                    ctx.beginPath()
                    ctx.arc(dotX, dotY, 2.5, 0, Math.PI * 2)
                    ctx.fill()
                }

                ctx.restore()

                // Draw the max label in a gutter outside the line/area clip.
                if (maxLabel.length > 0) {
                    const labelY = Math.max(plotY, Math.min(plotY + plotH, maxY))
                    const caption = maxVisible ? maxLabel
                        : (maxY < plotY ? "▲ " : "▼ ") + maxLabel
                    ctx.fillStyle = AppColors.primaryText
                    ctx.textAlign = "left"
                    ctx.textBaseline = "middle"
                    ctx.fillText(caption, plotX + plotW + labelGap, labelY, rightGutter - labelGap)
                }
            }

            Connections {
                target: root
                function onPointsChanged() { spark.scheduleRepaint() }
                function onAxisYMinChanged() { spark.scheduleRepaint() }
                function onAxisYMaxChanged() { spark.scheduleRepaint() }
                function onDecimalsChanged() { spark.scheduleRepaint() }
                function onStateColorChanged() { spark.scheduleRepaint() }
                function onOutOfRangeChanged() { spark.scheduleRepaint() }
                function onThresholdMaxChanged() { spark.scheduleRepaint() }
                function onHasMaxThresholdChanged() { spark.scheduleRepaint() }
                function onXMinMsChanged() { spark.scheduleRepaint() }
                function onXMaxMsChanged() { spark.scheduleRepaint() }
            }

            Connections {
                target: SettingsController
                function onSystemTimezoneChanged() { spark.scheduleRepaint() }
            }
            Connections {
                target: AppColors
                function onIsDarkChanged() { spark.scheduleRepaint() }
            }
            onWidthChanged: scheduleRepaint()
            onHeightChanged: scheduleRepaint()
            onVisibleChanged: scheduleRepaint()
            onAvailableChanged: scheduleRepaint()
        }
    }
}
