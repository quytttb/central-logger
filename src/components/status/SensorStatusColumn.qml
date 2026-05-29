import QtQuick

import CentralLogger.Theme

/// Sensor table Status column: operational chip + active attach-DI type chips.
Row {
    id: root

    property string displayStatus: ""
    property string alarmType: ""
    property var attachDiTypeCodes: []
    property var attachDiTypeLabels: []

    spacing: 8

    readonly property var activeTypeCodes: AttachDiType.activeTypeCodesList(attachDiTypeCodes)
    readonly property var activeTypeLabels: attachDiTypeLabels || []

    StatusChip {
        displayStatus: String(root.displayStatus || "")
        alarmType: String(root.alarmType || "")
    }

    Repeater {
        model: root.activeTypeCodes

        StatusChip {
            required property int index
            required property var modelData
            attachDiTypeCode: String(modelData)
            attachDiTypeLabel: root.activeTypeLabels.length > index
                                 ? String(root.activeTypeLabels[index]) : ""
        }
    }
}
