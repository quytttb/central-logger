import QtQuick
import QtQuick.Window

import CentralLogger.Theme

// Per-view toolbar host (no page title — navigation rail shows the screen).
Item {
    id: bar

    property Component toolbarSource: null

    readonly property int barHeight: AppTheme.topBarHeight
    readonly property int hPad: AppTheme.pagePadding

    implicitHeight: barHeight
    height: barHeight

    Rectangle {
        anchors.fill: parent
        color: AppColors.surface

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: AppColors.dividerLine
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        onPressed: mouse => {
            if (mouse.button === Qt.LeftButton && Window.window)
                Window.window.startSystemMove()
        }
        onDoubleClicked: mouse => {
            if (mouse.button === Qt.LeftButton && Window.window) {
                if (Window.window.visibility === Window.Maximized)
                    Window.window.showNormal()
                else
                    Window.window.showMaximized()
            }
        }
    }

    Loader {
        id: toolbarLoader
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: bar.hPad
            rightMargin: bar.hPad
        }
        sourceComponent: bar.toolbarSource

        onItemChanged: {
            if (item)
                item.width = width
        }
        onWidthChanged: {
            if (item)
                item.width = width
        }
    }
}
