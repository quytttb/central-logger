pragma ComponentBehavior: Bound

import QtQuick

import LoggerKit.Components
import CentralLogger.Theme

/// Domain wrapper over the generic StatusChip: operational status, attach-DI
/// type, or connection indicator. Feeds computed text/colors from the
/// OperationalStatus / AttachDiType singletons into LoggerKit's StatusChip.
StatusChip {
    id: root

    // Indicator mode passes through `label` + indicator* from StatusChip directly.

    property string displayStatus: ""
    property string alarmType: ""

    /// Attach-DI `di_type` code; empty = operational / indicator only.
    property string attachDiTypeCode: ""
    /// Optional label from catalog (custom `di_type`); falls back to standard table.
    property string attachDiTypeLabel: ""

    readonly property bool attachDiTypeMode: label.length === 0
        && String(attachDiTypeCode || "").trim().length > 0
    readonly property bool operationalMode: label.length === 0 && !attachDiTypeMode

    readonly property string normalizedStatus: String(displayStatus || "").toUpperCase()
    readonly property string normalizedAlarmType: String(alarmType || "").toLowerCase()

    chipText: {
        if (label.length > 0)
            return ""
        if (attachDiTypeMode)
            return AttachDiType.typeLabel(attachDiTypeCode, attachDiTypeLabel)
        return OperationalStatus.statusText(normalizedStatus, normalizedAlarmType)
    }

    statusIconName: operationalMode
        ? OperationalStatus.statusIconName(normalizedAlarmType) : ""

    chipFill: attachDiTypeMode ? AttachDiType.chipFill(attachDiTypeCode)
                               : OperationalStatus.chipFill(normalizedStatus)
    chipBorder: attachDiTypeMode ? AttachDiType.chipBorder(attachDiTypeCode)
                                 : OperationalStatus.chipBorder(normalizedStatus)
    chipTextColor: attachDiTypeMode ? AttachDiType.chipTextColor(attachDiTypeCode)
                                    : OperationalStatus.chipTextColor(normalizedStatus)
}
