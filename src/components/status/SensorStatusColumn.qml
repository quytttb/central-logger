pragma ComponentBehavior: Bound

import QtQuick

import CentralLogger.Theme
import LoggerKit.Components

/// Sensor table Status column: operational chip + active attach-DI type chips.
Row {
    id: root

    property string displayStatus: ""
    property string alarmType: ""
    property bool status03Auto: false
    property var attachDiTypeCodes: []
    property var attachDiTypeLabels: []

    spacing: 8

    readonly property var activeTypeCodes: AttachDiType.activeTypeCodesList(attachDiTypeCodes)
    readonly property var activeTypeLabels: attachDiTypeLabels || []

    SensorStatusChip {
        displayStatus: String(root.displayStatus || "")
        alarmType: String(root.alarmType || "")
        status03Auto: root.status03Auto
    }

    Repeater {
        model: root.activeTypeCodes

        SensorStatusChip {
            required property int index
            required property var modelData
            attachDiTypeCode: String(modelData)
            attachDiTypeLabel: root.activeTypeLabels.length > index
                                 ? String(root.activeTypeLabels[index]) : ""
        }
    }
}
