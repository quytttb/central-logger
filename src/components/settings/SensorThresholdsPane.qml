pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

ElevatedPane {
    id: root
    objectName: "sensorThresholdsPane"

    property int loggerId: -1

    onLoggerIdChanged: {
        if (filterField)
            filterField.text = ""
    }

    Binding {
        target: ThresholdController
        property: "selectedLoggerId"
        value: Math.max(0, root.loggerId)
    }
    Component.onDestruction: ThresholdController.setFilterText("")

    padding: 0
    contentSpacing: 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: AppTheme.sectionSpacing
        Layout.rightMargin: AppTheme.sectionSpacing
        Layout.topMargin: AppTheme.sectionSpacing
        Layout.bottomMargin: AppTheme.spacingS
        spacing: AppTheme.spacingS

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Sensor Thresholds")

            Label {
                text: qsTr("Read-only")
                font: AppTypography.labelSmall
                color: AppColors.textMuted
            }
        }

        Label {
            visible: root.loggerId > 0
            Layout.fillWidth: true
            text: qsTr("Limits come from the logger. Use Reload config to refresh this copy.")
            font: AppTypography.bodySmall
            color: AppColors.textMuted
            wrapMode: Text.WordWrap
        }

        InlineBanner {
            Layout.fillWidth: true
            visible: root.loggerId > 0 && ThresholdController.lastError.length > 0
            message: ThresholdController.lastError
        }

        TextField {
            id: filterField
            visible: root.loggerId > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            Material.containerStyle: Material.Outlined
            placeholderText: qsTr("Search analog sensors…")
            leftPadding: 34
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
    }

    AppTableView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: ThresholdController.table
        hasData: root.loggerId > 0 && ThresholdController.table.rowsSize > 0
        colWeights: [0.44, 0.28, 0.28]
        colMinimums: [108, 70, 70]
        headerAlignRight: function(col) { return col > 0 }
        emptyIconName: filterField.text.trim().length > 0 ? "magnify" : "sensors"
        emptyMessage: root.loggerId <= 0
                      ? qsTr("Select or add a logger to view its sensor thresholds.")
                      : filterField.text.trim().length > 0
                        ? qsTr("No analog sensors match \"%1\".").arg(filterField.text.trim())
                        : qsTr("No active analog sensors for this logger.")

        delegate: Item {
            id: thresholdCell

            required property int column
            required property string display
            required property string unit
            readonly property bool isSensor: column === 0

            implicitHeight: 52

            Label {
                objectName: thresholdCell.isSensor ? "thresholdSensor" : "thresholdValue"
                anchors.fill: parent
                anchors.leftMargin: thresholdCell.isSensor ? 12 : 6
                anchors.rightMargin: 6
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: thresholdCell.isSensor ? Text.AlignLeft : Text.AlignRight
                text: thresholdCell.isSensor && thresholdCell.unit.length > 0
                      ? qsTr("%1 (%2)").arg(thresholdCell.display).arg(thresholdCell.unit)
                      : (thresholdCell.display.length > 0 ? thresholdCell.display : "—")
                font: AppTypography.bodyMedium
                color: AppColors.primaryText
                elide: Text.ElideRight
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: AppColors.outlineVariant
            }
        }
    }

    Label {
        visible: root.loggerId > 0
        Layout.margins: AppTheme.spacingSM
        text: qsTr("%1 sensor(s)").arg(ThresholdController.table.rowsSize)
        font: AppTypography.bodySmall
        color: AppColors.textMuted
    }
}
