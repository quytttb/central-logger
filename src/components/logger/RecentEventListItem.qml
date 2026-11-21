pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import LoggerKit.Theme
import LoggerKit.Components

Pane {
    id: root

    required property var loggerId
    required property string loggerName
    required property string eventType
    required property string message
    required property string displayLevel
    required property var createdAt

    signal activated(int loggerId)
    signal detailRequested(string title, string body, int loggerId)

    readonly property bool _isAudit: {
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
               : AppColors.eventLevelBackground(root.displayLevel)
    }

    HoverHandler { id: hoverHandler }

    TapHandler {
        onTapped: {
            if (root._isAudit) {
                if (root.loggerId !== undefined && root.loggerId !== null && root.loggerId > 0)
                    root.activated(root.loggerId)
                else
                    root.detailRequested(root.eventType || "", root.message || "", -1)
            } else {
                root.detailRequested(
                    root.eventType || "",
                    root.message   || "",
                    (root.loggerId !== undefined && root.loggerId !== null && root.loggerId > 0) ? root.loggerId : -1
                )
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: AppTheme.toolbarGap

        Rectangle {
            Layout.preferredWidth: 8
            Layout.fillHeight: true
            radius: AppTheme.radiusTiny
            color: root.levelColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: AppTheme.toolbarGap

                Label {
                    text: root.eventType || ""
                    font: AppTypography.titleMediumBold
                    color: root.levelColor
                }
                Label {
                    text: root.loggerName && root.loggerName.length > 0 ? "\u00B7 " + root.loggerName : ""
                    visible: text.length > 0
                    color: AppColors.textSubtle
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: {
                        if (!root.createdAt || isNaN(root.createdAt.getTime()))
                            return ""
                        return SettingsController.formatTimestamp(root.createdAt)
                    }
                    color: AppColors.textFaint
                    font: AppTypography.labelMedium
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.message || ""
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: AppColors.textSoft
                font: AppTypography.bodyMedium
            }
        }
    }
}
