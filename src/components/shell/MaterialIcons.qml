pragma Singleton
import QtQuick

/*
 * Codepoints from MaterialIcons-Regular.codepoints (Google).
 * Font: resources/fonts/MaterialSymbols/MaterialSymbolsOutlined.ttf
 *
 * Ported from central-logger-app/src/central_logger/ui/components/common/MaterialIcons.qml
 */
QtObject {
    readonly property string menu:                "\uE5D2"  // menu
    readonly property string magnify:             "\uE8B6"  // search
    readonly property string closeCircle:         "\uE5C9"  // cancel
    readonly property string whiteBalanceSunny:   "\uE518"  // light_mode
    readonly property string weatherNight:        "\uE51C"  // dark_mode
    readonly property string windowMinimize:      "\uE931"  // minimize
    readonly property string windowClose:         "\uE5CD"  // close
    readonly property string viewDashboard:       "\uE871"  // dashboard
    readonly property string server:              "\uF56E"  // loggers (sidebar Edge Loggers)
    readonly property string cog:                 "\uE8B8"  // settings
    readonly property string wifi:                "\uE63E"  // wifi
    readonly property string alertOutline:        "\uE002"  // warning
    readonly property string plus:                "\uE145"  // add
    readonly property string close:               "\uE5CD"  // close
    readonly property string chip:                "\uE30D"  // developer_board (logger row)
    readonly property string arrowLeft:           "\uE5C4"  // arrow_back
    readonly property string pencil:              "\uE254"  // mode_edit
    readonly property string trashCan:            "\uE872"  // delete
    readonly property string informationOutline:  "\uE88E"  // info
    readonly property string save:                "\uE161"  // save
    readonly property string download:            "\uE2C4"  // download
    readonly property string qrCode:              "\uF206"  // qr_code_scanner
    readonly property string link:                "\uE157"  // link
    readonly property string refresh:           "\uE5D5"  // refresh
    readonly property string showChart:         "\uE6E1"  // show_chart (Material Symbols Outlined)
    readonly property string schedule:          "\uE192"  // schedule
    readonly property string inbox:             "\uE156"  // inbox
    readonly property string description:       "\uE873"  // description
    readonly property string wifiOff:           "\uE648"  // wifi_off
    readonly property string arrowUpward:       "\uE5D8"  // arrow_upward
    readonly property string arrowDownward:     "\uE5DB"  // arrow_downward
    readonly property string checkCircle:       "\uE92D"  // check_circle_outline
    readonly property string warning:           "\uE002"  // warning (same glyph as alertOutline)
    readonly property string error:             "\uE000"  // error
    readonly property string info:              "\uE88E"  // info (same glyph as informationOutline)
    readonly property string sensors:           "\uE51F"  // sensors

    function glyph(name) {
        switch (name) {
        case "menu":                return menu
        case "magnify":             return magnify
        case "closeCircle":         return closeCircle
        case "whiteBalanceSunny":   return whiteBalanceSunny
        case "weatherNight":        return weatherNight
        case "windowMinimize":      return windowMinimize
        case "windowClose":         return windowClose
        case "viewDashboard":       return viewDashboard
        case "server":              return server
        case "cog":                 return cog
        case "wifi":                return wifi
        case "alertOutline":        return alertOutline
        case "plus":                return plus
        case "close":               return close
        case "chip":                return chip
        case "arrowLeft":           return arrowLeft
        case "pencil":              return pencil
        case "trashCan":            return trashCan
        case "informationOutline":  return informationOutline
        case "save":                return save
        case "download":            return download
        case "qrCode":              return qrCode
        case "link":                return link
        case "refresh":             return refresh
        case "showChart":           return showChart
        case "schedule":            return schedule
        case "inbox":               return inbox
        case "description":         return description
        case "wifiOff":             return wifiOff
        case "arrowUpward":         return arrowUpward
        case "arrowDownward":       return arrowDownward
        case "checkCircle":         return checkCircle
        case "warning":             return warning
        case "error":               return error
        case "info":                return info
        case "sensors":             return sensors
        default:                    return close
        }
    }
}
