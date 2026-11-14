pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme
import LoggerKit.Theme
import LoggerKit.Components

// Thresholds screen — opened from Loggers → Set Threshold.
// One row per ANALOG sensor of the selected logger; Min/Max are the alarm
// limits evaluated by SensorMerger (value <= min or >= max → ALARM).
Item {
    id: root

    property Component topBarToolbar: ThresholdsTopBar {}

    signal goBack()

    Component.onCompleted: ThresholdController.refresh()

    Connections {
        target: ThresholdController
        function onSaveFinished(ok, message) {
            if (ok) {
                AppNotifier.show(qsTr("Thresholds saved."), "success")
            } else if (message.length > 0) {
                AppNotifier.show(qsTr("Could not save thresholds"), "error",
                                 { detailText: message,
                                   detailTitle: qsTr("Threshold error") })
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        InlineBanner {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            visible: ThresholdController.lastError.length > 0
            semantic: "error"
            message: ThresholdController.lastError
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: ThresholdController.lastError.length > 0
                                ? AppTheme.sectionSpacing
                                : AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding

            ElevatedPane {
                anchors.fill: parent
                padding: 0
                contentSpacing: 0

                SectionHeader {
                    Layout.fillWidth: true
                    Layout.leftMargin: AppTheme.sectionSpacing
                    Layout.rightMargin: AppTheme.sectionSpacing
                    Layout.topMargin: AppTheme.sectionSpacing
                    Layout.bottomMargin: AppTheme.toolbarGap
                    title: ThresholdController.selectedLoggerName.length > 0
                           ? ThresholdController.selectedLoggerName
                           : qsTr("Thresholds")

                    Label {
                        text: qsTr("Alarm thresholds applied by Central")
                        font: AppTypography.labelSmall
                        color: AppColors.textMuted
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                AppTableView {
                    id: thresholdTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: ThresholdController.table
                    reuseItems: false
                    hasData: ThresholdController.table.rowsSize > 0
                    colWeights: [0.44, 0.28, 0.28]
                    colMinimums: [220, 140, 140]
                    headerAlignRight: function(col) { return col > 0 }
                    emptyIconName: "sensors"
                    emptyMessage: ThresholdController.loggerItems.length === 0
                                  ? qsTr("No loggers yet. Add a logger first.")
                                  : qsTr("No ANALOG sensors for this logger.")

                    delegate: ItemDelegate {
                        id: thresholdCell

                        required property int    row
                        required property int    column
                        required property string name
                        required property string unit
                        required property string minText
                        required property string maxText
                        required property bool   isNameColumn
                        required property bool   rowDirty

                        implicitHeight: 56
                        padding: 0
                        hoverEnabled: false

                        background: Rectangle {
                            color: thresholdCell.rowDirty
                                   ? AppColors.withAlpha(AppColors.warning, 0.10)
                                   : "transparent"

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: AppColors.outlineVariant
                            }
                        }

                        contentItem: Item {
                            Label {
                                visible: thresholdCell.isNameColumn
                                anchors {
                                    left: parent.left
                                    leftMargin: 16
                                    right: parent.right
                                    rightMargin: 8
                                    verticalCenter: parent.verticalCenter
                                }
                                text: thresholdCell.unit.length > 0
                                      ? thresholdCell.name + " (" + thresholdCell.unit + ")"
                                      : thresholdCell.name
                                font: Qt.font({ pixelSize: 16, weight: Font.DemiBold })
                                color: AppColors.primaryText
                                elide: Text.ElideRight
                            }

                            TextField {
                                id: thresholdField
                                visible: !thresholdCell.isNameColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                Material.containerStyle: Material.Outlined
                                horizontalAlignment: Text.AlignRight
                                font: AppTypography.bodyMedium
                                selectByMouse: true
                                onTextEdited: ThresholdController.table.setCellText(
                                                  thresholdCell.row,
                                                  thresholdCell.column,
                                                  text)
                                onAccepted: focus = false

                                // Keep the field in sync when the model
                                // resets (save/discard/filter) while the
                                // delegate is still alive.
                                Binding {
                                    target: thresholdField
                                    property: "text"
                                    value: thresholdCell.column === 1
                                           ? thresholdCell.minText
                                           : thresholdCell.maxText
                                    restoreMode: Binding.RestoreNone
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component ThresholdsTopBar: RowLayout {
        id: thresholdTopBar
        spacing: AppTheme.toolbarGap

        function syncLoggerIndex() {
            const items = ThresholdController.loggerItems
            const id = ThresholdController.selectedLoggerId
            for (let i = 0; i < items.length; ++i) {
                if (items[i].id === id)
                    return i
            }
            return items.length > 0 ? 0 : -1
        }

        AppButton {
            kind: AppButton.Neutral
            forceDarkText: false
            iconOnly: true
            controlSize: 36
            iconSide: 20
            iconName: "arrowLeft"
            tooltipText: qsTr("Back to loggers")
            Layout.alignment: Qt.AlignVCenter
            onClicked: root.goBack()
        }

        TextField {
            Layout.preferredWidth: 320
            Layout.maximumWidth: 320
            Layout.preferredHeight: 40
            Material.containerStyle: Material.Outlined
            placeholderText: qsTr("Search sensors…")
            leftPadding: 36
            onTextChanged: ThresholdController.setFilterText(text)

            UiIcon {
                name: "magnify"
                size: AppTheme.iconSizeSm
                iconColor: AppColors.iconSubtle
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10
            }
        }

        ComboBox {
            id: loggerCombo
            Layout.preferredWidth: 220
            Layout.maximumWidth: 280
            Layout.preferredHeight: 40
            model: ThresholdController.loggerItems
            textRole: "name"
            valueRole: "id"
            font: AppTypography.bodyMedium
            currentIndex: thresholdTopBar.syncLoggerIndex()
            displayText: currentIndex >= 0 ? currentText : qsTr("(No loggers)")
            onActivated: ThresholdController.selectedLoggerId = currentValue
        }

        Item { Layout.fillWidth: true }

        Label {
            text: qsTr("%1 sensor(s)").arg(ThresholdController.table.rowsSize)
            color: AppColors.textMuted
            font: AppTypography.bodyMedium
            Layout.alignment: Qt.AlignVCenter
        }

        AppButton {
            kind: AppButton.Primary
            text: ThresholdController.saving ? qsTr("Saving…") : qsTr("Save")
            iconName: "save"
            visible: ThresholdController.table.dirty
            enabled: !ThresholdController.saving
            Layout.alignment: Qt.AlignVCenter
            onClicked: ThresholdController.save()
        }
    }
}
