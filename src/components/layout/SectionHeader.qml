import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import CentralLogger.Theme

// Section title row — titleMedium + optional trailing actions slot.
RowLayout {
    id: root

    property string title: ""

    default property alias actions: actionsHost.data

    spacing: AppTheme.toolbarGap

    Label {
        text: root.title
        font: AppTypography.titleMedium
        Layout.fillWidth: true
    }

    RowLayout {
        id: actionsHost
        spacing: AppTheme.toolbarGap
    }
}
