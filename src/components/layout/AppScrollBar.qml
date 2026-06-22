pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import CentralLogger.Theme

// Thin, M3-styled vertical scrollbar shared across tables and lists.
//
// Standalone usage: bind `flickable` to a ListView / TableView / Flickable and
// anchor this bar to the right edge of the scroll area. Reserve room for it so
// rows never sit under the bar:
//
//     SomeList { anchors.rightMargin: vbar.gutter }
//     AppScrollBar { id: vbar; flickable: someList; anchors { top; bottom; right } }
ScrollBar {
    id: control

    required property var flickable

    // Width the content should leave on the right so it never sits under the bar.
    readonly property real gutter: visible ? width + 4 : 0

    orientation: Qt.Vertical
    policy: ScrollBar.AsNeeded
    visible: flickable && flickable.contentHeight > flickable.height
    size: flickable ? flickable.visibleArea.heightRatio : 0
    position: flickable ? flickable.visibleArea.yPosition : 0
    minimumSize: 0.08
    onPositionChanged: {
        if (pressed && flickable)
            flickable.contentY = position * flickable.contentHeight + flickable.originY
    }

    padding: 2

    contentItem: Rectangle {
        implicitWidth: 6
        radius: width / 2
        color: AppColors.withAlpha(
            AppColors.primaryText,
            control.pressed ? 0.5 : (control.hovered ? 0.38 : 0.24))

        Behavior on color { ColorAnimation { duration: AppTheme.motionFast } }
    }
}
