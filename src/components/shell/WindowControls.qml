import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import CentralLogger.Theme

// Frameless window chrome — minimize / maximize / close at the bottom of the rail.
Item {
    id: root

    property var windowRef: Window.window

    readonly property bool isMaximized: root.windowRef
        && (root.windowRef.visibility === Window.Maximized
            || root.windowRef.visibility === Window.FullScreen)

    readonly property int buttonSize: AppTheme.buttonHeight

    implicitWidth: AppTheme.railWidth
    implicitHeight: buttonSize * 3 + AppTheme.navItemSpacing * 2

    ColumnLayout {
        width: parent.width
        spacing: AppTheme.navItemSpacing

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.buttonSize
            Layout.preferredHeight: root.buttonSize
            iconOnly: true
            forceDarkText: false
            kind: AppButton.Neutral
            iconName: "windowMinimize"
            tooltipText: qsTr("Minimize")
            onClicked: {
                if (!root.windowRef)
                    return
                root.windowRef.visibility = Window.Minimized
            }
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.buttonSize
            Layout.preferredHeight: root.buttonSize
            iconOnly: true
            forceDarkText: false
            kind: AppButton.Neutral
            iconName: root.isMaximized ? "windowRestore" : "windowMaximize"
            tooltipText: root.isMaximized ? qsTr("Restore") : qsTr("Maximize")
            onClicked: {
                if (!root.windowRef)
                    return
                if (root.isMaximized)
                    root.windowRef.showNormal()
                else
                    root.windowRef.showMaximized()
            }
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.buttonSize
            Layout.preferredHeight: root.buttonSize
            iconOnly: true
            forceDarkText: false
            kind: AppButton.Error
            iconName: "windowClose"
            tooltipText: qsTr("Close")
            onClicked: {
                if (root.windowRef)
                    root.windowRef.close()
            }
        }
    }
}
