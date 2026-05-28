import QtQuick

import CentralLogger.Theme

// Marker at each LineSeries data point (Qt Graphs pointDelegate).
// Graphs injects pointColor (see XYSeries::pointDelegate in Qt 6.11 docs).
Item {
    id: root

    width: 12
    height: 12

    property color pointColor: AppColors.primaryText

    Rectangle {
        anchors.centerIn: parent
        width: root.active ? 8 : 6
        height: width
        radius: width / 2
        color: AppColors.surfaceContainerHigh
        border.width: 2
        border.color: root.pointColor
    }
}
