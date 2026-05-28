pragma Singleton
import QtQuick

import CentralLogger.Core

// M3 color roles + state layers. Theme via SettingsController (not Material.theme on singletons).
// ApplicationWindow sets Material.primary/accent from AppTheme — custom UI uses tokens here only.
QtObject {
    readonly property bool isLight: SettingsController.theme === "light"
    readonly property bool isDark:  !isLight

    readonly property real hoverOpacity:    0.08
    readonly property real dividerOpacity:  0.12

    function withAlpha(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function hoverLayer(base) {
        return withAlpha(base, hoverOpacity)
    }

    function divider(base) {
        return withAlpha(base, dividerOpacity)
    }

    // Brand (Teal / Indigo) — keep in sync with AppTheme Material.primary / Material.accent on window.
    readonly property color primaryColor: isLight ? "#00796B" : "#80CBC4"
    readonly property color accentColor:  isLight ? "#3949AB" : "#9FA8DA"
    readonly property color onPrimary:    "#FFFFFF"

    // Solid containers (alpha-only blends were too low-contrast on rail / tonal buttons).
    readonly property color accentContainer:   isLight ? "#E8EAF6" : "#3D4F7C"
    // Not "onAccentContainer" — QML reserves on<Property> when property "accentContainer" exists.
    readonly property color accentContainerFg: isLight ? "#283593" : "#E8EAF6"

    // primaryText — not "onSurface" (clashes with Qt Material).
    readonly property color primaryText:
        SettingsController.theme === "light" ? "#1C1B1F" : "#E6E1E5"
    readonly property color onSurfaceVariant:
        SettingsController.theme === "light" ? "#49454F" : "#CAC4D0"

    readonly property color hoverFill: hoverLayer(primaryText)
    readonly property color dividerLine: divider(primaryText)

    // Text emphasis (prefer over Label.opacity in views).
    readonly property color textMuted: onSurfaceVariant
    readonly property color textSubtle: withAlpha(primaryText, 0.75)
    readonly property color textFaint: withAlpha(primaryText, 0.6)
    readonly property color textSoft: withAlpha(primaryText, 0.85)
    readonly property color disabledContent: withAlpha(primaryText, 0.38)
    readonly property color emptyStateIcon: withAlpha(onSurfaceVariant, 0.55)
    readonly property color iconSubtle: withAlpha(onSurfaceVariant, 0.5)
    readonly property color tableCellMuted: withAlpha(primaryText, 0.8)
    readonly property color tableHeaderText: withAlpha(primaryText, 0.7)

    readonly property color surface:              isDark ? "#121212" : "#E1E4E8"
    readonly property color surfaceContainerLow:  isDark ? "#282828" : "#FFFFFF"
    readonly property color surfaceContainer:     isDark ? "#323232" : "#F0F2F5"
    readonly property color surfaceContainerHigh: isDark ? "#3D3D3D" : "#E8EBEF"

    readonly property color elevatedBorder: isDark ? "#3A3A3A" : "#D0D5DC"

    readonly property color outline:        isDark ? "#938F99" : "#79747E"
    readonly property color outlineVariant: isDark ? "#49454F" : "#CAC4D0"

    readonly property color error:            isDark ? "#FFB4AB" : "#BA1A1A"
    readonly property color errorContainer:   isDark ? "#93000A" : "#FFDAD6"
    readonly property color errorContainerFg: isDark ? "#FFDAD6" : "#410002"

    readonly property color warning:          isDark ? "#FFD270" : "#B38C00"
    readonly property color warningContainer: isDark ? "#8A6A00" : "#FFEA79"

    readonly property color success:          isDark ? "#81C784" : "#2E7D32"
    readonly property color successContainer: isDark ? "#1B5E20" : "#C8E6C9"

    readonly property color info: isDark ? "#90CAF9" : "#0288D1"

    /// Accent color for system_event rows (`displayLevel` from RecentEventsModel).
    function severityColor(displayLevel) {
        switch ((displayLevel || "").toLowerCase()) {
        case "critical":
        case "error":
            return error
        case "warning":
            return warning
        case "info":
            return info
        default:
            return onSurfaceVariant
        }
    }

    function eventLevelBackground(displayLevel) {
        const c = severityColor(displayLevel)
        return withAlpha(c, isDark ? 0.12 : 0.08)
    }

    function _diCodeColors(diCode) {
        switch (diCode) {
        case "02":
            return { fill: errorContainer, border: error, text: error }
        case "03":
            return { fill: warningContainer, border: warning, text: warning }
        case "01":
            return { fill: accentContainer, border: accentColor, text: accentContainerFg }
        default:
            if (diCode && diCode !== "00") {
                return { fill: surfaceContainer, border: outline, text: onSurfaceVariant }
            }
            return null
        }
    }

    function _isAlarmDisplay(status) {
        switch (status) {
        case "ALARM":
        case "ERR":
            return true
        default:
            return false
        }
    }

    function alarmBadgeText(alarmType) {
        if (alarmType === "min") {
            return "\u25BC MIN"
        }
        if (alarmType === "max") {
            return "\u25B2 MAX"
        }
        if (alarmType === "min+max") {
            return "ALARM"
        }
        return "ALARM"
    }

    function _shortDiCodeLabel(diCode) {
        switch (diCode) {
        case "01":
            return "Cal"
        case "02":
            return "Err"
        case "03":
            return "Mnt"
        default:
            return diCode.length <= 5 ? diCode : diCode.substring(0, 5)
        }
    }

    /// Single-line chip text for the sensor table (avoids multi-badge overflow).
    function compactStatusText(displayStatus, diStatusCode, alarmType, showAlarmBadge) {
        const status = displayStatus || ""
        const di = (diStatusCode || "").trim()
        const alarm = alarmType || ""

        if (status === "ALARM") {
            if (alarm === "min") {
                return "\u25BCMIN"
            }
            if (alarm === "max") {
                return "\u25B2MAX"
            }
            if (alarm === "min+max") {
                return "ALM"
            }
            return "ALARM"
        }

        let base = status
        if (di && di !== "00") {
            base = _shortDiCodeLabel(di)
        } else {
            switch (status) {
            case "Monitoring":
                base = "Mon"
                break
            case "Calibrating":
                base = "Cal"
                break
            case "Maintenance":
                base = "Mnt"
                break
            case "Error":
                base = "Err"
                break
            default:
                break
            }
        }

        if (showAlarmBadge) {
            if (alarm === "min") {
                return base + "\u00B7\u25BC"
            }
            if (alarm === "max") {
                return base + "\u00B7\u25B2"
            }
            return base + "\u00B7!"
        }
        return base || "\u2014"
    }

    /// Full label for tooltip when compact text is abbreviated.
    function sensorStatusTooltip(displayStatus, diStatusCode, alarmType, showAlarmBadge) {
        const status = displayStatus || ""
        const di = (diStatusCode || "").trim()
        const parts = []

        if (di && di !== "00") {
            switch (di) {
            case "01":
                parts.push("Calibrating")
                break
            case "02":
                parts.push("Error")
                break
            case "03":
                parts.push("Maintenance")
                break
            default:
                parts.push(di)
                break
            }
        } else if (status) {
            parts.push(status)
        }

        if (status === "ALARM" || showAlarmBadge) {
            const badge = alarmBadgeText(alarmType)
            if (badge && badge !== "ALARM") {
                parts.push(badge)
            } else if (showAlarmBadge || status === "ALARM") {
                parts.push("Alarm")
            }
        }

        return parts.join(" \u2014 ")
    }

    /// SensorMonitoringTableModel chip colors (`displayStatus` + optional `diStatusCode`).
    function sensorStatusFill(status, diStatusCode) {
        const di = _diCodeColors(diStatusCode || "")
        if (di) {
            return di.fill
        }
        switch (status) {
        case "OK":
        case "Monitoring":
            return successContainer
        case "WAIT":
            return surfaceContainer
        case "STALE":
            return warningContainer
        default:
            return _isAlarmDisplay(status) ? errorContainer : surfaceContainer
        }
    }

    function sensorStatusBorder(status, diStatusCode) {
        const di = _diCodeColors(diStatusCode || "")
        if (di) {
            return di.border
        }
        switch (status) {
        case "OK":
        case "Monitoring":
            return success
        case "WAIT":
            return outline
        case "STALE":
            return warning
        default:
            return _isAlarmDisplay(status) ? error : outline
        }
    }

    function sensorStatusText(status, diStatusCode) {
        const di = _diCodeColors(diStatusCode || "")
        if (di) {
            return di.text
        }
        switch (status) {
        case "OK":
        case "Monitoring":
            return success
        case "WAIT":
            return onSurfaceVariant
        case "STALE":
            return warning
        default:
            return _isAlarmDisplay(status) ? error : onSurfaceVariant
        }
    }

    readonly property var graphSeriesColorsLight: [
        "#00796B", "#3949AB", "#0288D1", "#F57C00",
        "#7B1FA2", "#C2185B", "#1976D2", "#388E3C"
    ]
    readonly property var graphSeriesColorsDark: [
        "#4DB6AC", "#9FA8DA", "#4FC3F7", "#FFB74D",
        "#CE93D8", "#F48FB1", "#64B5F6", "#81C784"
    ]
    readonly property var graphSeriesColors: isLight ? graphSeriesColorsLight : graphSeriesColorsDark
}
