import QtGraphs
import QtQuick

import CentralLogger.Core

// GraphsView with app theme + system timezone binding for DateTimeAxis.
// Root must be GraphsView so LineSeries children enter seriesList (not Item.data).
GraphsView {
    id: root

    marginBottom: 8
    marginLeft: 8
    theme: ChartGraphsTheme {}

    readonly property alias chart: root

    function bindSystemTimezone(dateTimeAxis) {
        if (!dateTimeAxis)
            return
        const tzId = SettingsController.systemTimezone
        dateTimeAxis.timeZone = dateTimeAxis.timeZoneFromString(
            (tzId && tzId.length > 0) ? tzId : "UTC")
    }

    Connections {
        target: SettingsController
        function onSystemTimezoneChanged() {
            if (root.axisX)
                root.bindSystemTimezone(root.axisX)
        }
    }
}
