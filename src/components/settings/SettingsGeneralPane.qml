pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import LoggerKit.Theme
import LoggerKit.Components

// General tab content for SettingsView (`app_settings` row 1).
Item {
    id: root

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
    property var    draftPrimaryLoggerId: SettingsController.primaryLoggerId

    readonly property bool dirty:
           draftTimezone !== SettingsController.systemTimezone
        || retentionSpin.value !== SettingsController.dataRetentionDays
        || historyFlushSpin.value !== SettingsController.historyFlushIntervalS
        || Number(draftPrimaryLoggerId) !== SettingsController.primaryLoggerId

    function syncFromController() {
        draftTimezone        = SettingsController.systemTimezone
        draftRetention       = SettingsController.dataRetentionDays
        draftHistoryFlush    = SettingsController.historyFlushIntervalS
        draftPrimaryLoggerId = SettingsController.primaryLoggerId
        retentionSpin.value    = draftRetention
        historyFlushSpin.value = draftHistoryFlush
        primaryLoggerCombo.selectLoggerId(SettingsController.primaryLoggerId)
        const tzIdx = root.timezones.indexOf(draftTimezone)
        if (tzIdx >= 0)
            timezoneCombo.currentIndex = tzIdx
        else
            timezoneCombo.editText = draftTimezone
    }

    function applyAndSave() {
        root.forceActiveFocus()
        SettingsController.systemTimezone        = root.draftTimezone
        SettingsController.dataRetentionDays     = retentionSpin.value
        SettingsController.historyFlushIntervalS = historyFlushSpin.value
        SettingsController.primaryLoggerId       = Number(root.draftPrimaryLoggerId)
        if (!SettingsController.save()) {
            SettingsController.load()
            root.syncFromController()
        }
    }

    Component.onCompleted: {
        SettingsController.reloadLoggerChoices()
        syncFromController()
    }

    Connections {
        target: SettingsController
        function onLoggerChoicesChanged() {
            primaryLoggerCombo.selectLoggerId(root.draftPrimaryLoggerId)
        }
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

    ScrollView {
        id: generalScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: generalScroll.availableWidth
            spacing: 0

            ElevatedPane {
                id: generalCard
                objectName: "generalSettingsCard"
                Layout.fillWidth: true
                Layout.leftMargin: AppTheme.pagePadding
                Layout.rightMargin: AppTheme.pagePadding
                Layout.topMargin: AppTheme.pageTopSpacing
                Layout.bottomMargin: AppTheme.pagePadding
                implicitHeight: implicitContentHeight + topPadding + bottomPadding
                padding: AppTheme.dialogPadding
                contentSpacing: AppTheme.spacingSM

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
                        id: timezoneCombo
                        Layout.fillWidth: true
                        Material.containerStyle: Material.Outlined
                        editable: true
                        model: root.timezones
                        Component.onCompleted: {
                            const idx = root.timezones.indexOf(root.draftTimezone)
                            if (idx >= 0) currentIndex = idx
                            else editText = root.draftTimezone
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
                        spacing: AppTheme.spacingXS

                        Label {
                            text: qsTr("Primary logger")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr(
                                "Logger shown by default on Loggers > Graph; use the Show graph dropdown to pick another one. The other loggers appear as table rows.")
                            color: AppColors.textMuted
                            font: AppTypography.labelSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                    ComboBox {
                        id: primaryLoggerCombo
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 2
                        Material.containerStyle: Material.Outlined
                        textRole: "name"
                        valueRole: "id"
                        model: [{ "id": 0, "name": qsTr("(None)") }].concat(SettingsController.loggerChoices)

                        function selectLoggerId(loggerId) {
                            const items = model
                            const target = Number(loggerId)
                            for (let i = 0; i < items.length; ++i) {
                                if (Number(items[i].id) === target) {
                                    currentIndex = i
                                    return
                                }
                            }
                            currentIndex = 0
                        }

                        onActivated: root.draftPrimaryLoggerId = Number(currentValue)
                        onModelChanged: selectLoggerId(root.draftPrimaryLoggerId)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: AppTheme.spacingXS

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

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    implicitHeight: 1
                    Layout.topMargin: AppTheme.formRowSpacing
                    color: AppColors.dividerLine
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: AppTheme.spacingXS

                    Label {
                        text: qsTr("Diagnostics")
                        font: AppTypography.labelSmall
                        color: AppColors.textMuted
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: AppTheme.spacingS

                        Label {
                            text: qsTr("Log file:")
                            color: AppColors.textMuted
                            font: AppTypography.bodySmall
                        }

                        Label {
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
                                ClipboardService.copyToClipboard(SettingsController.logFilePath)
                                AppNotifier.show(qsTr("Log path copied to clipboard."), "info")
                            }
                        }
                    }
                }
            }
        }
    }
}
