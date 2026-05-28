import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

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
        enabled: loggerId !== undefined && loggerId !== null
        onTapped: {
            if (loggerId !== undefined && loggerId !== null)
                root.activated(loggerId)
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
                text: message || ""
                wrapMode: Text.WordWrap
                color: AppColors.textSoft
                font: AppTypography.bodyMedium
            }
        }
    }
}
