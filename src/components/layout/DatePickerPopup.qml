pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Components
import CentralLogger.Theme

// M3 outlined calendar popup — non-modal, positioned by DateField below the input.
Popup {
    id: datePicker

    width: 320
    height: 380
    padding: AppTheme.sectionSpacing
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property date selectedDate: new Date()

    signal datePicked(date picked)

    Material.theme: AppTheme.materialTheme

    background: Rectangle {
        color: AppColors.surfaceContainerLow
        radius: AppTheme.cardRadius
        border.color: AppColors.elevatedBorder
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: AppTheme.toolbarGap

        RowLayout {
            Layout.fillWidth: true
            spacing: AppTheme.toolbarGap

            AppButton {
                kind: AppButton.Neutral
                forceDarkText: false
                iconOnly: true
                controlSize: 36
                iconName: "arrowLeft"
                tooltipText: "Previous month"
                onClicked: {
                    let m = monthGrid.month - 1
                    let y = monthGrid.year
                    if (m < 0) { m = 11; y-- }
                    monthGrid.month = m
                    monthGrid.year = y
                }
            }

            Label {
                Layout.fillWidth: true
                text: monthGrid.title
                horizontalAlignment: Text.AlignHCenter
                font: AppTypography.titleMedium
                color: AppColors.primaryText
            }

            AppButton {
                kind: AppButton.Neutral
                forceDarkText: false
                iconOnly: true
                controlSize: 36
                iconName: "arrowLeft"
                rotation: 180
                tooltipText: "Next month"
                onClicked: {
                    let m = monthGrid.month + 1
                    let y = monthGrid.year
                    if (m > 11) { m = 0; y++ }
                    monthGrid.month = m
                    monthGrid.year = y
                }
            }
        }

        DayOfWeekRow {
            locale: monthGrid.locale
            Layout.fillWidth: true

            delegate: Text {
                required property string shortName
                text: shortName
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: AppTypography.labelSmall.pixelSize
                font.weight: Font.Bold
                color: AppColors.onSurfaceVariant
            }
        }

        MonthGrid {
            id: monthGrid
            month: datePicker.selectedDate.getMonth()
            year: datePicker.selectedDate.getFullYear()
            locale: Qt.locale("en_US")
            Layout.fillWidth: true
            Layout.fillHeight: true

            delegate: Rectangle {
                id: dayCell
                required property var model

                width: Math.floor(monthGrid.width / 7)
                height: Math.floor(monthGrid.height / 6)
                radius: AppTheme.listItemRadius

                readonly property bool isCurrentMonth: dayCell.model.month === monthGrid.month
                readonly property bool isToday: {
                    const now = new Date()
                    return dayCell.model.day === now.getDate()
                        && dayCell.model.month === now.getMonth()
                        && dayCell.model.year === now.getFullYear()
                }
                readonly property bool isSelected: {
                    const s = datePicker.selectedDate
                    return dayCell.model.day === s.getDate()
                        && dayCell.model.month === s.getMonth()
                        && dayCell.model.year === s.getFullYear()
                }

                color: dayCell.isSelected ? AppColors.accentContainer
                     : dayMouse.containsMouse && dayCell.isCurrentMonth ? AppColors.hoverFill
                     : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: dayCell.model.day
                    font.pixelSize: AppTypography.bodyMedium.pixelSize
                    font.weight: dayCell.isToday ? Font.DemiBold : Font.Normal
                    color: {
                        if (!dayCell.isCurrentMonth)
                            return AppColors.textFaint
                        if (dayCell.isSelected)
                            return AppColors.accentContainerFg
                        if (dayCell.isToday)
                            return AppColors.accentColor
                        return AppColors.primaryText
                    }
                }

                MouseArea {
                    id: dayMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        datePicker.selectedDate = dayCell.model.date
                        datePicker.datePicked(dayCell.model.date)
                        datePicker.close()
                    }
                }
            }
        }

        AppButton {
            text: "Today"
            kind: AppButton.Tonal
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                const now = new Date()
                datePicker.selectedDate = now
                datePicker.datePicked(now)
                datePicker.close()
            }
        }
    }
}
