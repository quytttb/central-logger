pragma Singleton
import QtQuick

/// Operational sensor chip text, icons, and colors (OK / ALARM / MIN / MAX / …).
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

    function statusText(displayStatus, alarmType) {
        const status = String(displayStatus || "").toUpperCase()
        const type = String(alarmType || "").toLowerCase()
        if (status === "ALARM") {
            if (type === "min") {
                return "MIN"
            }
            if (type === "max") {
                return "MAX"
            }
            if (type === "min+max") {
                return "MIN+MAX"
            }
            if (type === "device") {
                return "Alarm"
            }
            return "Alarm"
        }
        switch (status) {
        case "ERR":
            return "Error"
        case "WAIT":
            return "Wait"
        case "STALE":
            return "Stale"
        case "OK":
            return "OK"
        default:
            return status || "\u2014"
        }
    }

    function chipFill(displayStatus) {
        const status = String(displayStatus || "").toUpperCase()
        switch (status) {
        case "OK":
        case "Monitoring":
            return AppColors.successContainer
        case "WAIT":
            return AppColors.surfaceContainer
        case "STALE":
            return AppColors.warningContainer
        case "ALARM":
        case "ERR":
            return AppColors.errorContainer
        default:
            return AppColors.surfaceContainer
        }
    }

    function chipBorder(displayStatus) {
        const status = String(displayStatus || "").toUpperCase()
        switch (status) {
        case "OK":
        case "Monitoring":
            return AppColors.success
        case "WAIT":
            return AppColors.outline
        case "STALE":
            return AppColors.warning
        case "ALARM":
        case "ERR":
            return AppColors.error
        default:
            return AppColors.outline
        }
    }

    function chipTextColor(displayStatus) {
        const status = String(displayStatus || "").toUpperCase()
        switch (status) {
        case "OK":
        case "Monitoring":
            return AppColors.success
        case "WAIT":
            return AppColors.onSurfaceVariant
        case "STALE":
            return AppColors.warning
        case "ALARM":
        case "ERR":
            return AppColors.error
        default:
            return AppColors.onSurfaceVariant
        }
    }
}
