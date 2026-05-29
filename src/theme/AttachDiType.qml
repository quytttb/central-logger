pragma Singleton
import QtQuick

/// Attach-DI `di_type` chip presentation: standard 00–03 + custom catalog labels.
QtObject {
    function _palette(diTypeCode) {
        const code = String(diTypeCode || "").trim()
        switch (code) {
        case "00":
            return { fill: AppColors.successContainer, border: AppColors.success,
                     text: AppColors.success }
        case "02":
            return { fill: AppColors.errorContainer, border: AppColors.error,
                     text: AppColors.error }
        case "03":
            return { fill: AppColors.warningContainer, border: AppColors.warning,
                     text: AppColors.warning }
        case "01":
            return { fill: AppColors.accentContainer, border: AppColors.accentColor,
                     text: AppColors.accentContainerFg }
        default:
            if (code.length > 0 && code !== "00") {
                return { fill: AppColors.surfaceContainer, border: AppColors.outline,
                         text: AppColors.onSurfaceVariant }
            }
            return null
        }
    }

    function typeLabel(diTypeCode, catalogLabel) {
        const code = String(diTypeCode || "").trim()
        switch (code) {
        case "00":
            return "Monitoring"
        case "01":
            return "Calibrating"
        case "02":
            return "Error"
        case "03":
            return "Maintenance"
        default:
            const name = String(catalogLabel || "").trim()
            return name.length > 0 ? name : code
        }
    }

    function activeTypeCodesList(raw) {
        if (raw === undefined || raw === null) {
            return []
        }
        if (Array.isArray(raw)) {
            return raw.map(function (c) { return String(c).trim() }).filter(function (c) {
                return c.length > 0
            })
        }
        if (typeof raw === "string") {
            const t = raw.trim()
            return t.length ? t.split(",").map(function (c) { return c.trim() })
                .filter(function (c) { return c.length > 0 }) : []
        }
        if (typeof raw === "object" && raw.length !== undefined) {
            const out = []
            for (let i = 0; i < raw.length; ++i) {
                const item = raw[i]
                if (item !== undefined && item !== null) {
                    const s = String(item).trim()
                    if (s.length > 0) {
                        out.push(s)
                    }
                }
            }
            return out
        }
        return []
    }

    function chipFill(diTypeCode) {
        const di = _palette(diTypeCode)
        return di ? di.fill : AppColors.surfaceContainer
    }

    function chipBorder(diTypeCode) {
        const di = _palette(diTypeCode)
        return di ? di.border : AppColors.outline
    }

    function chipTextColor(diTypeCode) {
        const di = _palette(diTypeCode)
        return di ? di.text : AppColors.onSurfaceVariant
    }
}
