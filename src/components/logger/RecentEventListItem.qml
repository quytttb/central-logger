import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Components
import CentralLogger.Core
import CentralLogger.Theme

Pane {
    id: root

    required property int index
    required property var loggerId
    required property string loggerName
    required property string eventType
    required property string message
    required property string level
    required property string displayLevel
    required property var createdAt

    signal activated(int loggerId)
    signal detailRequested(string title, string body, int loggerId)

    readonly property string _reportSavedPrefix: DesktopService.reportSavedMessagePrefix()

    readonly property bool _isReportSaved: {
        const m = message || ""
        return m.length > _reportSavedPrefix.length
               && m.indexOf(_reportSavedPrefix) === 0
    }

    readonly property string _reportSavedPath: {
        if (!_isReportSaved)
            return ""
        return (message || "").substring(_reportSavedPrefix.length).trim()
    }

    /// List shows basename only; click still copies full path from `_reportSavedPath`.
    readonly property string displayMessage: {
        if (!_isReportSaved)
            return message || ""
        const base = DesktopService.fileBaseName(_reportSavedPath)
        return _reportSavedPrefix + (base.length > 0 ? base : _reportSavedPath)
    }

    readonly property bool _isAudit: {
        if (_isReportSaved)
            return false
        const t = (eventType || "").toLowerCase()
        return t === "online" || t === "offline" || t === "info"
    }

    readonly property color levelColor: AppColors.severityColor(displayLevel)

    width: ListView.view ? ListView.view.width : implicitWidth
    Material.elevation: 0
    padding: 12

    background: Rectangle {
        radius: AppTheme.listItemRadius
        color: hoverHandler.hovered
               ? AppColors.hoverFill
               : AppColors.eventLevelBackground(displayLevel)
    }

    HoverHandler { id: hoverHandler }

    TapHandler {
        onTapped: {
            if (root._isReportSaved) {
                if (DesktopService.copyToClipboard(root._reportSavedPath)) {
                    AppNotifier.show(qsTr("Path copied to clipboard"), "success", { durationMs: 2500 })
                }
                return
            }
            if (root._isAudit) {
                if (loggerId !== undefined && loggerId !== null && loggerId > 0)
                    root.activated(loggerId)
                else
                    root.detailRequested(eventType || "", message || "", -1)
            } else {
                root.detailRequested(
                    eventType || "",
                    message   || "",
                    (loggerId !== undefined && loggerId !== null && loggerId > 0) ? loggerId : -1
                )
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: AppTheme.toolbarGap

        Rectangle {
            width: 8
            Layout.fillHeight: true
            radius: 4
            color: levelColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: AppTheme.toolbarGap

                Label {
                    text: eventType || ""
                    font: AppTypography.titleMediumBold
                    color: levelColor
                }
                Label {
                    text: loggerName && loggerName.length > 0 ? "\u00B7 " + loggerName : ""
                    visible: text.length > 0
                    color: AppColors.textSubtle
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: {
                        if (!createdAt || isNaN(createdAt.getTime()))
                            return ""
                        return SettingsController.formatTimestamp(createdAt)
                    }
                    color: AppColors.textFaint
                    font: AppTypography.labelMedium
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.displayMessage
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: AppColors.textSoft
                font: AppTypography.bodyMedium
            }
        }
    }
}
