pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

// Vertical-slice form for app_settings (row 1): timezone, retention.
// Theme is controlled exclusively from the rail (ThemeToggle).
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
    property int    draftHistoryFlush: SettingsController.historyFlushIntervalS

    readonly property bool dirty:
           draftTimezone !== SettingsController.systemTimezone
        || retentionSpin.value !== SettingsController.dataRetentionDays
        || historyFlushSpin.value !== SettingsController.historyFlushIntervalS

    function syncFromController() {
        draftTimezone    = SettingsController.systemTimezone;
        draftRetention   = SettingsController.dataRetentionDays;
        draftHistoryFlush = SettingsController.historyFlushIntervalS;
        retentionSpin.value = draftRetention;
        historyFlushSpin.value = draftHistoryFlush;
        const tzIdx = root.timezones.indexOf(draftTimezone);
        if (tzIdx >= 0)
            timezoneCombo.currentIndex = tzIdx;
        else
            timezoneCombo.editText = draftTimezone;
    }

    Component.onCompleted: syncFromController()

    Connections {
        target: SettingsController
        function onSaved() {
            root.syncFromController()
            AppNotifier.show(qsTr("Settings saved successfully."), "success")
        }
        function onError(message) {
            AppNotifier.show(
                qsTr("Failed to save settings"),
                "error",
                { detailText: message, detailTitle: qsTr("Settings save error") }
            )
        }
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
                        id: retentionSpin
                        Layout.fillWidth: true
                        Material.containerStyle: Material.Outlined
                        from: 1
                        to: 3650
                        editable: true
                        live: true
                        onValueModified: root.draftRetention = value
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("History update interval (seconds)")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr(
                                "How often new readings are flushed to the database and the History table auto-refreshes.")
                            color: AppColors.textMuted
                            font: AppTypography.labelSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                    SpinBox {
                        id: historyFlushSpin
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 2
                        Material.containerStyle: Material.Outlined
                        from: 1
                        to: 3600
                        editable: true
                        live: true
                        onValueModified: root.draftHistoryFlush = value
                    }
                }

                Label {
                    text: SettingsController.lastError
                    color: AppColors.error
                    visible: SettingsController.lastError.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                // Diagnostics row — log file path for support / bug reports.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    Layout.topMargin: AppTheme.formRowSpacing
                    height: 1
                    color: AppColors.dividerLine
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    spacing: 4

                    Label {
                        text: qsTr("Diagnostics")
                        font: AppTypography.labelSmall
                        color: AppColors.textMuted
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Log file:")
                            color: AppColors.textMuted
                            font: AppTypography.bodySmall
                        }

                        Label {
                            id: logPathLabel
                            Layout.fillWidth: true
                            text: SettingsController.logFilePath.length > 0
                                  ? SettingsController.logFilePath
                                  : qsTr("(logging not available)")
                            font: AppTypography.bodySmall
                            color: AppColors.textMuted
                            wrapMode: Text.WrapAnywhere
                            elide: Text.ElideNone
                        }

                        AppButton {
                            kind: AppButton.Text
                            text: qsTr("Copy")
                            visible: SettingsController.logFilePath.length > 0
                            onClicked: {
                                DesktopService.copyToClipboard(SettingsController.logFilePath)
                                AppNotifier.show(qsTr("Log path copied to clipboard."), "info")
                            }
                        }
                    }
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
                root.forceActiveFocus();
                SettingsController.systemTimezone        = root.draftTimezone;
                SettingsController.dataRetentionDays     = retentionSpin.value;
                SettingsController.historyFlushIntervalS = historyFlushSpin.value;
                if (!SettingsController.save()) {
                    SettingsController.load();
                    root.syncFromController();
                }
            }
        }
    }
}
