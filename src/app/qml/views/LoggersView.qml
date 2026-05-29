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

    Component.onCompleted: DashboardController.reloadLoggers()

    // Show a dismissible toast when form Save POST /config fails.
    Connections {
        target: DashboardController
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
        onAccepted: DashboardController.removeLogger(targetId)
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

                TableContentStack {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    hasData: loggerTable.rows > 0
                    emptyIconName: root.searchFilterText.length > 0 ? "magnify" : "server"
                    emptyMessage: root.searchFilterText.length > 0
                                  ? qsTr("No loggers match \"%1\"").arg(root.searchFilterText)
                                  : qsTr("No loggers yet. Click Add Logger to get started.")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        HorizontalHeaderView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: AppTheme.tableHeaderHeight
                            syncView: loggerTable
                            textRole: "display"

                            delegate: TableHeaderCell {
                                cornerRadius: AppTheme.cardRadius
                                roundTopLeft: column === 0
                                roundTopRight: column === loggerTable.colWidths.length - 1
                                alignRight: column === 2 || column === 3
                            }
                        }

                        TableView {
                            id: loggerTable
                            reuseItems: true
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: searchProxy
                            clip: true

                            readonly property var colWeights:  [1, 1, 0, 0, 0, 0]
                            readonly property var colMinimums: [96, 96, 76, 76, 96, 92]
                            property var colWidths: []

                            function recomputeColumns() {
                                colWidths = AppTheme.distributeColumnWidths(
                                    width, colWeights, colMinimums)
                                forceLayout()
                            }

                            Component.onCompleted: recomputeColumns()
                            onWidthChanged: recomputeColumns()

                            columnWidthProvider: function(col) {
                                return (col >= 0 && col < colWidths.length)
                                       ? colWidths[col] : 80
                            }

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

                            onClicked: {
                                if (loggerCell.column !== 5)
                                    root.selectLogger(loggerCell.loggerId)
                            }

                            background: TableCellBackground {
                                cellHovered: loggerCell.hovered
                                rowIndex: loggerCell.row
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

                                    ToolButton {
                                        implicitWidth: 32
                                        implicitHeight: 32
                                        ToolTip.text: qsTr("Edit")
                                        ToolTip.visible: hovered
                                        onClicked: {
                                            const data = DashboardController.getLoggerFormData(loggerCell.loggerId)
                                            formDialog.open("edit", data, loggerCell.loggerId)
                                        }
                                        contentItem: UiIcon {
                                            name: "pencil"
                                            size: 18
                                            iconColor: AppColors.primaryText
                                            anchors.centerIn: parent
                                        }
                                    }
                                    ToolButton {
                                        implicitWidth: 32
                                        implicitHeight: 32
                                        ToolTip.text: qsTr("Delete")
                                        ToolTip.visible: hovered
                                        onClicked: {
                                            confirmDelete.targetId   = loggerCell.loggerId
                                            confirmDelete.targetCode = loggerCell.name
                                            confirmDelete.open()
                                        }
                                        contentItem: UiIcon {
                                            name: "trashCan"
                                            size: 18
                                            iconColor: AppColors.primaryText
                                            anchors.centerIn: parent
                                        }
                                    }
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
            Layout.fillWidth: true
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
                size: 18
                iconColor: AppColors.iconSubtle
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10
            }
        }

        Label {
            text: qsTr("%1 logger(s)").arg(loggerTable.rows)
            color: AppColors.textMuted
            font: AppTypography.bodyMedium
        }

        AppButton {
            kind: AppButton.Primary
            text: qsTr("Add Logger")
            onClicked: formDialog.open("add", {}, -1)
        }
    }
}
