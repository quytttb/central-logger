pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import CentralLogger.Theme

Item {
    id: root

    property Component topBarToolbar: LoggersTopBar {}
    property string searchFilterText: ""

    signal selectLogger(int loggerId)

    // Show a dismissible toast when form Save POST /config fails.
    Connections {
        target: LoggerFormController
        function onConfigApplyFailed(loggerId, errorMessage) {
            AppNotifier.show(
                qsTr("Config push failed (logger #%1)").arg(loggerId),
                "error",
                {
                    detailText:  errorMessage,
                    detailTitle: qsTr("Config push error"),
                    loggerId:    loggerId,
                    durationMs:  7000
                }
            );
        }
    }

    LoggerFormDialog {
        id: formDialog
        parent: root
    }

    Dialog {
        id: confirmDelete
        property int targetId: -1
        property string targetCode: ""
        title: qsTr("Delete logger")
        modal: true
        anchors.centerIn: parent
        width: parent ? Math.min(400, parent.width - 48) : 400
        standardButtons: Dialog.Cancel | Dialog.Ok
        Label {
            width: confirmDelete.width - 48
            text: qsTr("Xóa logger \"%1\"? Catalog cảm biến và lịch sử số đo sẽ bị xóa theo.")
                    .arg(confirmDelete.targetCode)
            wrapMode: Text.WordWrap
        }
        onAccepted: LoggerFormController.removeLogger(targetId)
    }

    LoggerSearchProxyModel {
        id: searchProxy
        sourceModel: DashboardController.loggers
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: AppTheme.pagePadding
            Layout.rightMargin: AppTheme.pagePadding
            Layout.topMargin: AppTheme.pageTopSpacing
            Layout.bottomMargin: AppTheme.pagePadding

            ElevatedPane {
                anchors.fill: parent
                padding: 0

                AppTableView {
                    id: loggerTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: searchProxy
                    colWeights:  [1, 1, 0, 0, 0, 0]
                    colMinimums: [96, 96, 76, 76, 96, 92]
                    headerAlignRight: function(col) { return col === 2 || col === 3 }
                    emptyIconName: root.searchFilterText.length > 0 ? "magnify" : "server"
                    emptyMessage: root.searchFilterText.length > 0
                                  ? qsTr("No loggers match \"%1\"").arg(root.searchFilterText)
                                  : qsTr("No loggers yet. Click Add Logger to get started.")

                    delegate: ItemDelegate {
                        id: loggerCell

                        required property int  row
                        required property int  column
                        required property var  loggerId
                        required property var  name
                        required property var  host
                        required property var  modbusPort
                        required property var  sensorCount
                        required property var  online
                        required property var  polling
                        required property var  anyAlarm

                        implicitHeight: 56
                        padding: 0
                        hoverEnabled: true
                        onHoveredChanged: if (hovered) loggerTable.hoveredRow = row

                        onClicked: {
                            if (loggerCell.column !== 5)
                                root.selectLogger(loggerCell.loggerId)
                        }

                        background: TableCellBackground {
                            cellHovered: loggerTable.hoveredRow === loggerCell.row
                        }

                        contentItem: Item {
                            Label {
                                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                visible: loggerCell.column < 4
                                verticalAlignment: Text.AlignVCenter
                                text: {
                                    switch (loggerCell.column) {
                                    case 0: return loggerCell.name || ""
                                    case 1: return loggerCell.host || ""
                                    case 2: return loggerCell.modbusPort !== undefined
                                                   ? String(loggerCell.modbusPort) : ""
                                    case 3: return loggerCell.sensorCount !== undefined
                                                   ? String(loggerCell.sensorCount) : ""
                                    default: return ""
                                    }
                                }
                                horizontalAlignment: loggerCell.column >= 2
                                                     ? Text.AlignRight : Text.AlignLeft
                                color: loggerCell.column >= 1 && loggerCell.column <= 3
                                       ? AppColors.tableCellMuted
                                       : AppColors.primaryText
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                visible: loggerCell.column === 4
                                spacing: 4

                                Label {
                                    text: loggerCell.online ? qsTr("Online") : qsTr("Offline")
                                    color: loggerCell.online
                                           ? AppColors.success
                                           : AppColors.onSurfaceVariant
                                }
                                Label {
                                    text: "●"
                                    visible: loggerCell.polling
                                    color: AppColors.info
                                    ToolTip.text: qsTr("Polling")
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 500
                                    property bool hovered: false
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: parent.hovered = true
                                        onExited:  parent.hovered = false
                                    }
                                }
                                Label {
                                    text: "!"
                                    visible: loggerCell.anyAlarm
                                    color: AppColors.error
                                    font.bold: true
                                }
                            }

                            RowLayout {
                                anchors { fill: parent; leftMargin: 4; rightMargin: 4 }
                                visible: loggerCell.column === 5
                                spacing: 4

                                AppButton {
                                    kind: AppButton.Neutral
                                    forceDarkText: false
                                    iconOnly: true
                                    controlSize: 32
                                    iconName: "pencil"
                                    tooltipText: qsTr("Edit")
                                    onClicked: {
                                        const data = LoggerFormController.getLoggerFormData(loggerCell.loggerId)
                                        formDialog.open("edit", data, loggerCell.loggerId)
                                    }
                                }
                                AppButton {
                                    kind: AppButton.Error
                                    forceDarkText: false
                                    iconOnly: true
                                    controlSize: 32
                                    iconName: "trashCan"
                                    tooltipText: qsTr("Delete")
                                    onClicked: {
                                        confirmDelete.targetId   = loggerCell.loggerId
                                        confirmDelete.targetCode = loggerCell.name
                                        confirmDelete.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component LoggersTopBar: RowLayout {
        spacing: AppTheme.toolbarGap

        TextField {
            Layout.preferredWidth: 480
            Layout.maximumWidth: 480
            Layout.preferredHeight: 40
            Material.containerStyle: Material.Outlined
            placeholderText: qsTr("Search station code, name, or host…")
            onTextChanged: {
                searchProxy.filterText = text
                root.searchFilterText = text
            }
            leftPadding: 36

            UiIcon {
                name: "magnify"
                size: AppTheme.iconSizeSm
                iconColor: AppColors.iconSubtle
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10
            }
        }

        Item { Layout.fillWidth: true }

        Label {
            text: qsTr("%1 logger(s)").arg(loggerTable.rows)
            color: AppColors.textMuted
            font: AppTypography.bodyMedium
            Layout.alignment: Qt.AlignVCenter
        }

        AppButton {
            kind: AppButton.Primary
            text: qsTr("Add Logger")
            Layout.alignment: Qt.AlignVCenter
            onClicked: formDialog.open("add", {}, -1)
        }
    }
}
