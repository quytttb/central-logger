pragma ComponentBehavior: Bound

pragma Singleton
import QtQuick

import LoggerKit.Theme

/// Operational sensor chip text, icons, and colors (TT10: 00/01/02/03).
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
        const status = String(displayStatus || "")
        const type = String(alarmType || "").toLowerCase()
        if (status.length === 0) {
            return "\u2014"
        }
        // Hiển thị trực tiếp mã TT10: "00"/"01"/"02"/"03"
        if (type === "min" || type === "max" || type === "min+max" || type === "device") {
            return status
        }
        return status
    }

    function chipFill(displayStatus) {
        const status = String(displayStatus || "")
        if (status.length === 0) {
            return AppColors.surfaceContainer
        }
        // TT10: 00=xanh lá (đang đo), 01/03=vàng (hiệu chuẩn/maintenance), 02=đỏ (lỗi)
        if (status === "02") {
            return AppColors.errorContainer
        }
        if (status === "01" || status === "03") {
            return AppColors.warningContainer
        }
        return AppColors.successContainer
    }

    function chipBorder(displayStatus) {
        const status = String(displayStatus || "")
        if (status.length === 0) {
            return AppColors.outline
        }
        if (status === "02") {
            return AppColors.error
        }
        if (status === "01" || status === "03") {
            return AppColors.warning
        }
        return AppColors.success
    }

    function chipTextColor(displayStatus) {
        const status = String(displayStatus || "")
        if (status.length === 0) {
            return AppColors.onSurfaceVariant
        }
        if (status === "02") {
            return AppColors.error
        }
        if (status === "01" || status === "03") {
            return AppColors.warning
        }
        return AppColors.success
    }
}
