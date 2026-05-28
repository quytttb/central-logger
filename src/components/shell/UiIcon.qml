import QtQuick

import CentralLogger.Theme

/*
 * Material Symbols Outlined icon (font-based).
 * Requires MaterialSymbolsOutlined.ttf loaded via QFontDatabase in main.cpp.
 *
 * Usage:
 *   UiIcon { name: "menu"; size: 20; iconColor: AppColors.primaryText }
 */
Text {
    id: root

    property string name:      ""
    property int    size:      24
    property color  iconColor: AppColors.primaryText

    text:                    MaterialIcons.glyph(root.name)
    font.family:             "Material Symbols Outlined"
    font.pixelSize:          root.size
    // Static Outlined font: avoid variable-font axes that break some glyphs.
    font.weight:             Font.Normal
    horizontalAlignment:     Text.AlignHCenter
    verticalAlignment:       Text.AlignVCenter
    color:                   root.iconColor
}
