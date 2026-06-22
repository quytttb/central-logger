pragma Singleton
import QtQuick

// M3 typography scale (1 dp ≈ 1 logical px on desktop).
QtObject {
    // Monospace family token — use for numeric / raw value columns instead of
    // hardcoding the "monospace" family string at call sites.
    readonly property string monoFamily: "monospace"

    // Display / large numbers (StatCard values)
    readonly property font displaySmall: Qt.font({
        pixelSize: 36,
        weight: Font.Normal
    })

    // Largest title step (dialog / page headers)
    readonly property font titleLarge: Qt.font({
        pixelSize: 18,
        weight: Font.Medium
    })

    // Section titles, table headers
    readonly property font titleMedium: Qt.font({
        pixelSize: 16,
        weight: Font.Medium
    })

    // Compact title (dense panels / detail headers)
    readonly property font titleSmall: Qt.font({
        pixelSize: 15,
        weight: Font.Medium
    })

    readonly property font titleMediumBold: Qt.font({
        pixelSize: 16,
        weight: Font.DemiBold
    })

    // Body text in tables, secondary info in top bars
    readonly property font bodyMedium: Qt.font({
        pixelSize: 14,
        weight: Font.Normal
    })

    // Dense body text (compact tables / secondary rows)
    readonly property font bodySmall: Qt.font({
        pixelSize: 13,
        weight: Font.Normal
    })

    // Buttons, prominent labels
    readonly property font labelLarge: Qt.font({
        pixelSize: 14,
        weight: Font.Medium
    })

    // Navigation rail labels (NavItem)
    readonly property font labelMedium: Qt.font({
        pixelSize: 12,
        weight: Font.Medium
    })

    // Chips, tooltips, form hints
    readonly property font labelSmall: Qt.font({
        pixelSize: 11,
        weight: Font.Medium
    })

    // Smallest legible label step (badges / micro-captions)
    readonly property font labelTiny: Qt.font({
        pixelSize: 10,
        weight: Font.Medium
    })

    // StatCard labels — overline style
    readonly property font overline: Qt.font({
        pixelSize: 12,
        weight: Font.Medium,
        letterSpacing: 1.0
    })
}
