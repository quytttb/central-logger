pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Theme
import LoggerKit.Theme
import LoggerKit.Components

// One ANALOG sensor tile on the Dashboard summary view.
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
    /// [{ x: epochMs, y: value, time: "HH:mm:ss" }]
    property var points: []

    property bool   hasThresholds: false
    property bool   hasMaxThreshold: false
    property double thresholdMax: 0
    property bool   outOfRange: false

    property double axisYMin: 0
    property double axisYMax: 1
    property double xMinMs: 0
    property double xMaxMs: 0

    readonly property color stateColor: !hasThresholds
                                        ? AppColors.surfaceContainerLow
                                        : (outOfRange ? AppColors.errorContainer
                                                      : AppColors.successContainer)
    readonly property color gridColor: AppColors.divider(AppColors.primaryText)
    /// Tiles shrink to ~150 px tall when a logger has 8 ANALOG sensors; the
    /// header and plot margins must follow instead of eating the trend area.
    readonly property bool compact: root.height < 190 || root.width < 280

    radius: AppTheme.cardRadius
    color: stateColor
    border.width: 1
    border.color: AppColors.elevatedBorder

    function resolvedXRange() {
        const pts = root.points || []
        let x0 = 0
        let x1 = 0
        // The tile's own samples always win: the shared chart axis window can
        // be stale when a logger stops reporting.
        if (pts.length === 1) {
            // Single sample: centre it instead of pinning it to the left edge.
            return { x0: pts[0].x - 500, x1: pts[0].x + 500 }
        }
        if (pts.length > 1) {
            x0 = pts[0].x
            x1 = pts[pts.length - 1].x
        } else {
            x0 = root.xMinMs
            x1 = root.xMaxMs
        }
        if (!x0)
            x0 = 0
        if (!x1 || x1 <= x0)
            x1 = x0 + 1000
        return { x0: x0, x1: x1 }
    }

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

    function formatClock(epochMs, withSeconds) {
        const date = new Date(epochMs)
        const hours = String(date.getHours()).padStart(2, "0")
        const minutes = String(date.getMinutes()).padStart(2, "0")
        if (!withSeconds)
            return hours + ":" + minutes
        const seconds = String(date.getSeconds()).padStart(2, "0")
        return hours + ":" + minutes + ":" + seconds
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

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (width < 60 || height < 54)
                    return

                const pts = root.points || []
                const xr = root.resolvedXRange()
                const yr = root.resolvedYRange()
                const xSpan = xr.x1 - xr.x0
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

                // Left gutter sized to the widest tick label, capped so the
                // plot still gets most of the tile on narrow cards.
                const labelWidth = Math.max(ctx.measureText(yTopText).width,
                                            ctx.measureText(yMidText).width,
                                            ctx.measureText(yBottomText).width)
                const gutter = Math.min(Math.max(24, Math.ceil(labelWidth) + 8),
                                        Math.max(24, Math.floor(width * 0.3)))
                const plotTop = root.compact ? 4 : 8
                const plotBottom = root.compact ? 14 : 16
                const plotX = gutter
                const plotY = plotTop
                const plotW = width - gutter - 2
                const plotH = height - plotTop - plotBottom
                if (plotW < 40 || plotH < 30)
                    return

                function px(x) { return plotX + (x - xr.x0) / xSpan * plotW }
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
                ctx.fillStyle = AppColors.textFaint
                ctx.textAlign = "right"
                ctx.textBaseline = "middle"
                ctx.fillText(yTopText, plotX - 6, plotY)
                ctx.fillText(yMidText, plotX - 6, plotY + plotH / 2)
                ctx.fillText(yBottomText, plotX - 6, plotY + plotH)

                // --- x tick labels (HH:mm, or HH:mm:ss on short windows) ---
                const withSeconds = xSpan < 5 * 60 * 1000
                const tStart = root.formatClock(xr.x0, withSeconds)
                const tMid = root.formatClock((xr.x0 + xr.x1) / 2, withSeconds)
                const tEnd = root.formatClock(xr.x1, withSeconds)
                const tickY = plotY + plotH + 4
                ctx.textBaseline = "top"
                ctx.textAlign = "left"
                ctx.fillText(tStart, plotX, tickY)
                if (!root.compact && tMid !== tStart && tMid !== tEnd) {
                    ctx.textAlign = "center"
                    ctx.fillText(tMid, plotX + plotW / 2, tickY)
                }
                ctx.textAlign = "right"
                ctx.fillText(tEnd, plotX + plotW, tickY)

                // --- plot contents clipped to the plot rect ---
                ctx.save()
                ctx.beginPath()
                ctx.rect(plotX, plotY, plotW, plotH)
                ctx.clip()

                const maxVisible = root.hasMaxThreshold
                                   && root.thresholdMax >= yr.yMin
                                   && root.thresholdMax <= yr.yMax
                if (maxVisible) {
                    const ty = py(root.thresholdMax)
                    ctx.strokeStyle = AppColors.warning
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.moveTo(plotX, ty)
                    ctx.lineTo(plotX + plotW, ty)
                    ctx.stroke()

                    ctx.fillStyle = AppColors.warning
                    ctx.textAlign = "right"
                    const maxText = root.formatAxisValue(root.thresholdMax)
                    if (ty - plotY > 14) {
                        ctx.textBaseline = "bottom"
                        ctx.fillText(maxText, plotX + plotW - 2, ty - 2)
                    } else {
                        ctx.textBaseline = "top"
                        ctx.fillText(maxText, plotX + plotW - 2, ty + 2)
                    }
                } else if (root.hasMaxThreshold && root.thresholdMax > yr.yMax) {
                    // Limit above the data-driven window: show it as a small
                    // marker instead of stretching the axis and flattening
                    // the trend.
                    const maxText = "max " + root.formatAxisValue(root.thresholdMax)
                    const textX = plotX + plotW - ctx.measureText(maxText).width - 2
                    const textY = plotY + 7
                    ctx.fillStyle = AppColors.warning
                    ctx.textAlign = "left"
                    ctx.textBaseline = "middle"
                    ctx.fillText(maxText, textX, textY)
                    ctx.beginPath()
                    ctx.moveTo(textX - 10, textY - 3)
                    ctx.lineTo(textX - 14, textY + 3)
                    ctx.lineTo(textX - 6, textY + 3)
                    ctx.closePath()
                    ctx.fill()
                } else if (root.hasMaxThreshold && root.thresholdMax < yr.yMin) {
                    // Reading is above the limit: the limit is below the
                    // visible window, mark it at the bottom edge.
                    const maxText = "max " + root.formatAxisValue(root.thresholdMax)
                    const textX = plotX + plotW - ctx.measureText(maxText).width - 2
                    const textY = plotY + plotH - 7
                    ctx.fillStyle = AppColors.warning
                    ctx.textAlign = "left"
                    ctx.textBaseline = "middle"
                    ctx.fillText(maxText, textX, textY)
                    ctx.beginPath()
                    ctx.moveTo(textX - 10, textY + 3)
                    ctx.lineTo(textX - 14, textY - 3)
                    ctx.lineTo(textX - 6, textY - 3)
                    ctx.closePath()
                    ctx.fill()
                }

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
            }

            Connections {
                target: root
                function onPointsChanged() { spark.requestPaint() }
                function onAxisYMinChanged() { spark.requestPaint() }
                function onAxisYMaxChanged() { spark.requestPaint() }
                function onDecimalsChanged() { spark.requestPaint() }
                function onOutOfRangeChanged() { spark.requestPaint() }
                function onHasThresholdsChanged() { spark.requestPaint() }
                function onThresholdMaxChanged() { spark.requestPaint() }
                function onHasMaxThresholdChanged() { spark.requestPaint() }
                function onXMinMsChanged() { spark.requestPaint() }
                function onXMaxMsChanged() { spark.requestPaint() }
            }

            Connections {
                target: SettingsController
                function onThemeChanged() { spark.requestPaint() }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }
}
