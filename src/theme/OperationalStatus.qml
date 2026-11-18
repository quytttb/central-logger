pragma ComponentBehavior: Bound

pragma Singleton
import QtQuick

import LoggerKit.Theme

/// Operational sensor chip text, icons, and colors (TT10: 00/01/02/03).
/// Each TT10 code maps to a display status text, not the raw code itself.
QtObject {
    function statusIconName(alarmType) {
        const type = String(alarmType || "").toLowerCase()
        if (type === "min") {
            return "arrowDownward"
        }
        if (type === "max") {
            return "arrowUpward"
        }
        return ""
    }

    // Map TT10 code → human-readable status text (shown on the chip)
    function statusText(displayStatus, alarmType, status03Auto) {
        const code = String(displayStatus || "")
        const type = String(alarmType || "").toLowerCase()
        if (code.length === 0) {
            return "\u2014"
        }
        // TT10 code → status text
        switch (code) {
        case "00": return qsTr("Measuring")
        case "01": return qsTr("Calibrating")
        case "02": return qsTr("Error")
        case "03": return status03Auto ? qsTr("Auto") : qsTr("Maintenance")
        case "UNKNOWN": return qsTr("Unknown")
        default:   return code || "\u2014"
        }
    }

    function chipFill(displayStatus) {
        const code = String(displayStatus || "")
        if (code.length === 0) {
            return AppColors.surfaceContainer
        }
        // TT10: 00=xanh lá (đang đo), 01/03=vàng (hiệu chuẩn/maintenance), 02=đỏ (lỗi)
        if (code === "02") {
            return AppColors.errorContainer
        }
        if (code === "01" || code === "03") {
            return AppColors.warningContainer
        }
        return code === "00" ? AppColors.successContainer : AppColors.surfaceContainer
    }

    function chipBorder(displayStatus) {
        const code = String(displayStatus || "")
        if (code.length === 0) {
            return AppColors.outline
        }
        if (code === "02") {
            return AppColors.error
        }
        if (code === "01" || code === "03") {
            return AppColors.warning
        }
        return code === "00" ? AppColors.success : AppColors.outline
    }

    function chipTextColor(displayStatus) {
        const code = String(displayStatus || "")
        if (code.length === 0) {
            return AppColors.onSurfaceVariant
        }
        if (code === "02") {
            return AppColors.error
        }
        if (code === "01" || code === "03") {
            return AppColors.warning
        }
        return code === "00" ? AppColors.success : AppColors.onSurfaceVariant
    }
}
