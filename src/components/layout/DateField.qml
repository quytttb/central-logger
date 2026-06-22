import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import CentralLogger.Components
import CentralLogger.Theme

// Read-only date field (dd/MM/yyyy) — opens DatePickerPopup anchored below the input.
Item {
    id: root

    property alias text: field.text
    property date date: parseDate(field.text)
    property var initialDate: new Date()
    property string placeholderText: ""

    implicitWidth: field.implicitWidth
    implicitHeight: field.implicitHeight

    function formatDate(d) {
        const dd = String(d.getDate()).padStart(2, "0")
        const mm = String(d.getMonth() + 1).padStart(2, "0")
        const yyyy = d.getFullYear()
        return dd + "/" + mm + "/" + yyyy
    }

    function parseDate(s) {
        const parts = String(s || "").split("/")
        if (parts.length !== 3)
            return new Date(NaN)
        const day = parseInt(parts[0], 10)
        const month = parseInt(parts[1], 10) - 1
        const year = parseInt(parts[2], 10)
        return new Date(year, month, day)
    }

    function openPicker() {
        const parsed = parseDate(field.text)
        picker.selectedDate = isNaN(parsed.getTime()) ? initialDate : parsed

        const gap = 4
        const mapped = field.mapToItem(Overlay.overlay, 0, field.height + gap)
        picker.x = mapped.x
        picker.y = mapped.y
        picker.width = Math.max(280, field.width)
        picker.open()
    }

    Component.onCompleted: {
        if (!field.text.length)
            field.text = formatDate(initialDate)
    }

    DatePickerPopup {
        id: picker
        parent: Overlay.overlay
        onDatePicked: d => { field.text = root.formatDate(d) }
    }

    TextField {
        id: field
        anchors.fill: parent
        readOnly: true
        Material.containerStyle: Material.Outlined
        Material.theme: AppTheme.materialTheme
        font: AppTypography.bodyMedium
        placeholderText: root.placeholderText
        horizontalAlignment: Text.AlignHCenter
        leftPadding: 8
        rightPadding: 32

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openPicker()
        }
    }

    UiIcon {
        name: "schedule"
        size: AppTheme.iconSizeSm
        iconColor: AppColors.iconSubtle
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 10
    }
}
