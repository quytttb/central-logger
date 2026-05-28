import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

// Vertical-slice form for app_settings (row 1): timezone, retention,
// maintenance. Theme is controlled exclusively from the rail (ThemeToggle).
// Form state mirrors SettingsController so cancel = re-load.
Item {
    id: root

    property Component topBarToolbar: SettingsTopBar {}

    readonly property var timezones: [
        "Asia/Ho_Chi_Minh",
        "Asia/Bangkok",
        "Asia/Singapore",
        "Asia/Tokyo",
        "UTC"
    ]

    property string draftTimezone: SettingsController.systemTimezone
    property int    draftRetention: SettingsController.dataRetentionDays
    property bool   draftMaintenance: SettingsController.maintenanceMode

    readonly property bool dirty:
           draftTimezone    !== SettingsController.systemTimezone
        || draftRetention   !== SettingsController.dataRetentionDays
        || draftMaintenance !== SettingsController.maintenanceMode

    function syncFromController() {
        draftTimezone    = SettingsController.systemTimezone;
        draftRetention   = SettingsController.dataRetentionDays;
        draftMaintenance = SettingsController.maintenanceMode;
    }

    Connections {
        target: SettingsController
        function onSaved() { root.syncFromController() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing

            ElevatedPane {
                anchors.fill: parent
                padding: AppTheme.dialogPadding

                Label {
                    text: qsTr("Application-wide preferences (persisted in app_settings)")
                    color: AppColors.textMuted
                    font: AppTypography.bodyMedium
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: AppTheme.pagePadding
                    rowSpacing: AppTheme.formRowSpacing

                    Label { text: qsTr("System timezone") }
                    ComboBox {
                        Layout.fillWidth: true
                        Material.containerStyle: Material.Outlined
                        id: timezoneCombo
                        editable: true
                        model: root.timezones
                        Component.onCompleted: {
                            const idx = root.timezones.indexOf(root.draftTimezone);
                            if (idx >= 0) currentIndex = idx;
                            else editText = root.draftTimezone;
                        }
                        onActivated: root.draftTimezone = root.timezones[currentIndex]
                        onAccepted: root.draftTimezone = editText
                        onEditTextChanged: root.draftTimezone = editText
                    }

                    Label { text: qsTr("Data retention (days)") }
                    SpinBox {
                        Layout.fillWidth: true
                        Material.containerStyle: Material.Outlined
                        from: 1
                        to: 3650
                        value: root.draftRetention
                        onValueModified: root.draftRetention = value
                    }

                    Label { text: qsTr("Maintenance mode") }
                    Switch {
                        checked: root.draftMaintenance
                        onToggled: root.draftMaintenance = checked
                    }
                }

                Label {
                    text: SettingsController.lastError
                    color: AppColors.error
                    visible: SettingsController.lastError.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    component SettingsTopBar: RowLayout {
        spacing: AppTheme.toolbarGap

        Item { Layout.fillWidth: true }

        AppButton {
            kind: AppButton.Secondary
            text: qsTr("Reset")
            enabled: root.dirty
            onClicked: root.syncFromController()
        }

        AppButton {
            kind: AppButton.Primary
            text: qsTr("Save")
            enabled: root.dirty
            onClicked: {
                SettingsController.systemTimezone    = root.draftTimezone;
                SettingsController.dataRetentionDays = root.draftRetention;
                SettingsController.maintenanceMode   = root.draftMaintenance;
                SettingsController.save();
            }
        }
    }
}
