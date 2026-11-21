pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Components
import LoggerKit.Theme
import LoggerKit.Components

// 3-column logger management workspace in Settings > Loggers:
//   Col 1: Logger list (search, Add/Delete, monitoring switch, live status)
//   Col 2: Local Add/Edit form (read config on Connect or Reload)
//   Col 3: Read-only sensor thresholds mirrored from the device
Item {
    id: root

    property string mode: "add" // "add" | "edit"
    property int    selectedLoggerId: -1
    property string selectedLoggerName: ""
    property var    initialData: ({})
    property int    hoveredRow: -1
    property string searchFilterText: ""

    property bool   probeLoaded: false
    property bool   probeInProgress: false
    property string saveError: ""
    property string reloadError: ""
    property string probeStatus: ""
    property string probeMessage: ""
    property bool   manualStationCode: false
    property bool   manualName: false
    property bool   manualUnitId: false

    readonly property bool wideThreeColumn: width >= 1120
    readonly property bool reloading: LoggerFormController.reloadingLoggerId > 0
    readonly property bool reloadingSelection: root.reloading
        && LoggerFormController.reloadingLoggerId === root.selectedLoggerId
    readonly property bool usesSavedEndpoint: root.mode === "edit"
        && hostField.text.trim() === String(root.initialData.host ?? "").trim()
        && apiPortSpin.value === root.initialData.apiPort
        && apiTokenField.text.trim() === String(root.initialData.apiToken ?? "").trim()

    readonly property bool hostValid:
           hostField.text.trim().length === 0
        || LoggerFormController.isValidHost(hostField.text.trim())

    readonly property bool canSave:
           stationCodeField.text.trim().length > 0
          && nameField.text.trim().length > 0
         && hostField.text.trim().length > 0
         && root.hostValid
         && (root.mode === "edit" || root.probeLoaded)
         && !root.probeInProgress
         && !root.reloading

    readonly property bool canProbe:
           root.mode === "add"
          && hostField.text.trim().length > 0
          && root.hostValid
          && apiPortSpin.value > 0
          && !root.probeInProgress
          && !root.reloading

    readonly property string connectDisabledHint: {
        if (root.probeInProgress)
            return ""
        if (hostField.text.trim().length === 0)
            return qsTr("Enter Host to connect to the device REST API.")
        if (!root.hostValid)
            return qsTr("Host must be a valid IPv4 address or hostname.")
        if (apiPortSpin.value <= 0)
            return qsTr("API port must be greater than 0.")
        return ""
    }

    function applyProbeHints() {
        if (!root.manualName)
            nameField.text = LoggerFormController.probedStationName()
        if (!root.manualStationCode)
            stationCodeField.text = LoggerFormController.probedStationCode()
        if (!root.manualUnitId) {
            const unit = LoggerFormController.probedModbusUnitId()
            modbusUnitIdSpin.value = unit >= modbusUnitIdSpin.from && unit <= modbusUnitIdSpin.to
                                   ? unit : AppDefaults.modbusUnitId
        }
    }

    function clearProbe() {
        LoggerFormController.clearProbedConfig()
        root.probeInProgress = false
        root.probeLoaded = false
        root.probeStatus = ""
        root.probeMessage = ""
    }

    function invalidateProbe() {
        if (root.mode === "add" && (root.probeLoaded || root.probeInProgress
                                    || root.probeMessage.length > 0))
            root.clearProbe()
    }

    function populateForm(nextMode, data, targetId) {
        root.clearProbe()
        root.mode = nextMode
        root.initialData = data || ({})
        root.selectedLoggerId = targetId
        root.selectedLoggerName = String(root.initialData.name ?? "")

        stationCodeField.text  = String(root.initialData.stationCode ?? "")
        nameField.text         = String(root.initialData.name ?? "")
        hostField.text         = String(root.initialData.host ?? "")
        modbusPortSpin.value   = root.initialData.modbusPort            ?? AppDefaults.modbusPort
        modbusUnitIdSpin.value = root.initialData.modbusUnitId          ?? AppDefaults.modbusUnitId
        pollIntervalSpin.value = root.initialData.centralPollIntervalS  ?? AppDefaults.pollIntervalSec
        timeoutSpin.value      = root.initialData.timeoutS              ?? AppDefaults.timeoutSec
        apiPortSpin.value      = root.initialData.apiPort               ?? AppDefaults.apiPort
        apiTokenField.text     = String(root.initialData.apiToken ?? "")
        enabledSwitch.checked  = root.initialData.enabled !== undefined
                                 ? Boolean(root.initialData.enabled) : true
        noteField.text         = String(root.initialData.note ?? "")

        root.saveError = ""
        root.reloadError = ""
        root.manualStationCode = false
        root.manualName = false
        root.manualUnitId = false
    }

    function openAddMode() {
        populateForm("add", {}, -1)
    }

    function selectLoggerForEdit(loggerId) {
        if (loggerId <= 0) {
            openAddMode()
            return
        }
        const data = LoggerFormController.getLoggerFormData(loggerId)
        if (!data || data.id === undefined) {
            openAddMode()
            return
        }
        populateForm("edit", data, loggerId)
    }

    function selectFirstOrAdd() {
        const choices = SettingsController.loggerChoices
        if (choices && choices.length > 0) {
            selectLoggerForEdit(Number(choices[0].id))
        } else {
            openAddMode()
        }
    }

    Component.onCompleted: {
        SettingsController.reloadLoggerChoices()
        selectFirstOrAdd()
    }
    Component.onDestruction: LoggerFormController.clearProbedConfig()

    Connections {
        target: LoggerFormController

        function onProbeConfigResult(ok, errorMessage) {
            if (root.mode !== "add" || !root.probeInProgress)
                return
            root.probeInProgress = false
            root.probeLoaded = ok
            if (ok) {
                root.probeStatus = "success"
                root.probeMessage = qsTr("Device config loaded.")
                root.applyProbeHints()
            } else {
                root.probeStatus = "error"
                root.probeMessage = errorMessage
            }
        }

        function onConfigReloadFinished(loggerId, ok, errorMessage) {
            if (root.mode !== "edit" || root.selectedLoggerId !== loggerId)
                return
            if (ok) {
                root.reloadError = ""
                AppNotifier.show(qsTr("Config reloaded from device."), "success")
            } else {
                root.reloadError = errorMessage
                AppNotifier.show(qsTr("Could not reload config."), "error", {
                    detailText: errorMessage,
                    detailTitle: qsTr("Reload config"),
                    contextId: loggerId
                })
            }
        }

        function onFormSaveFinished(ok, savedLoggerId, errorMessage) {
            if (!ok) {
                root.saveError = errorMessage
                return
            }

            const savedName = nameField.text.trim()
            const wasAdd = (root.mode === "add")
            SettingsController.reloadLoggerChoices()

            root.selectLoggerForEdit(savedLoggerId)
            AppNotifier.show(
                wasAdd
                    ? qsTr("Logger \"%1\" added successfully.").arg(savedName)
                    : qsTr("Logger \"%1\" saved successfully.").arg(savedName),
                "success"
            )
        }

        function onLoggerRemoved(removedId) {
            SettingsController.reloadLoggerChoices()
            if (removedId === root.selectedLoggerId) {
                root.selectFirstOrAdd()
            }
        }
    }

    AlertDialog {
        id: confirmDelete
        property int    targetId: -1
        property string targetLabel: ""
        title: qsTr("Delete logger")
        showCancel: true
        confirmText: qsTr("Delete")
        message: qsTr("Delete logger \"%1\"? Its sensor catalog and reading history will also be removed.")
                     .arg(confirmDelete.targetLabel)
        onConfirmed: {
            if (confirmDelete.targetId > 0)
                LoggerFormController.removeLogger(confirmDelete.targetId)
        }
    }

    LoggerSearchProxyModel {
        id: searchProxy
        sourceModel: DashboardController.loggers
    }

    ScrollView {
        id: workspaceScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: root.wideThreeColumn ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded

        Item {
            id: workspaceContent
            objectName: "loggersWorkspaceContent"
            width: workspaceScroll.availableWidth
            height: root.wideThreeColumn
                    ? workspaceScroll.availableHeight
                    : workspaceGrid.implicitHeight + AppTheme.pageTopSpacing + AppTheme.pagePadding

            GridLayout {
                id: workspaceGrid
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: AppTheme.pagePadding
                    rightMargin: AppTheme.pagePadding
                    topMargin: AppTheme.pageTopSpacing
                }
                height: root.wideThreeColumn
                        ? Math.max(0, workspaceContent.height - AppTheme.pageTopSpacing - AppTheme.pagePadding)
                        : implicitHeight
                columns: root.wideThreeColumn ? 3 : 1
                columnSpacing: AppTheme.sectionSpacing
                rowSpacing: AppTheme.sectionSpacing

                // ══════════════════════════════════════════════════════════════
                // Column 1 — Logger List
                // ══════════════════════════════════════════════════════════════
                ElevatedPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: root.wideThreeColumn
                    Layout.preferredWidth: root.wideThreeColumn ? 380 : -1
                    Layout.horizontalStretchFactor: root.wideThreeColumn ? 38 : 1
                    Layout.preferredHeight: root.wideThreeColumn ? -1 : 300
                    Layout.minimumWidth: root.wideThreeColumn ? 300 : 0
                    Layout.minimumHeight: 240
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
                    title: qsTr("Loggers")

                    AppButton {
                        kind: AppButton.Error
                        forceDarkText: false
                        iconOnly: true
                        controlSize: 34
                        iconSide: 18
                        iconName: "trashCan"
                        tooltipText: qsTr("Delete selected logger")
                        enabled: root.selectedLoggerId > 0 && !root.reloadingSelection
                        onClicked: {
                            confirmDelete.targetId = root.selectedLoggerId
                            confirmDelete.targetLabel = root.selectedLoggerName
                            confirmDelete.open()
                        }
                    }

                    AppButton {
                        kind: AppButton.Primary
                        iconName: "plus"
                        text: qsTr("Add")
                        enabled: !root.reloading
                        onClicked: root.openAddMode()
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    Material.containerStyle: Material.Outlined
                    placeholderText: qsTr("Search name, station, or host…")
                    leftPadding: 34
                    onTextChanged: {
                        searchProxy.filterText = text
                        root.searchFilterText = text
                    }

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
                id: loggerTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: searchProxy
                colWeights:  [0.22, 0.14, 0.20, 0.14, 0.15, 0.15]
                colMinimums: [76,   54,   76,   62,   56,   58]
                emptyIconName: root.searchFilterText.length > 0 ? "magnify" : "server"
                emptyMessage: root.searchFilterText.length > 0
                              ? qsTr("No loggers match \"%1\".").arg(root.searchFilterText)
                              : qsTr("No loggers yet. Use the form to add one.")

                HoverHandler {
                    onHoveredChanged: if (!hovered) root.hoveredRow = -1
                }

                delegate: ItemDelegate {
                    id: loggerCell

                    required property int    row
                    required property int    column
                    required property int    loggerId
                    required property string name
                    required property string stationCode
                    required property string host
                    required property int    modbusPort
                    required property int    apiPort
                    required property bool   loggerEnabled
                    required property bool   online
                    required property bool   anyAlarm

                    readonly property bool isSelected: loggerId === root.selectedLoggerId

                    implicitHeight: 48
                    padding: 0
                    hoverEnabled: true
                    onHoveredChanged: if (hovered) root.hoveredRow = row
                    onClicked: {
                        if (loggerCell.column !== 4) {
                            root.selectLoggerForEdit(loggerCell.loggerId)
                        }
                    }

                    background: Rectangle {
                        color: loggerCell.isSelected
                               ? AppColors.withAlpha(AppColors.accentColor, 0.16)
                               : (root.hoveredRow === loggerCell.row ? AppColors.hoverFill : "transparent")

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: AppColors.outlineVariant
                        }
                    }

                    contentItem: Item {
                        clip: true

                        // Text columns: 0 Name, 1 Station, 2 Host, 3 Ports
                        Label {
                            visible: loggerCell.column <= 3
                            anchors {
                                fill: parent
                                leftMargin: loggerCell.column === 0 ? 12 : 6
                                rightMargin: 6
                            }
                            verticalAlignment: Text.AlignVCenter
                            text: {
                                switch (loggerCell.column) {
                                case 0: return loggerCell.name
                                case 1: return loggerCell.stationCode
                                case 2: return loggerCell.host
                                case 3: return String(loggerCell.modbusPort) + "/" + String(loggerCell.apiPort)
                                default: return ""
                                }
                            }
                            font.weight: loggerCell.column === 0 ? Font.DemiBold : Font.Normal
                            font.family: loggerCell.column === 3 ? AppTypography.monoFamily : ""
                            color: loggerCell.column === 0 ? AppColors.primaryText : AppColors.tableCellMuted
                            elide: Text.ElideRight
                        }

                        // Col 4: Enabled switch
                        Switch {
                            id: rowSwitch
                            visible: loggerCell.column === 4
                            Accessible.name: qsTr("Monitor in Central")
                            anchors.centerIn: parent
                            onToggled: {
                                if (LoggerFormController.setLoggerEnabled(loggerCell.loggerId, checked)) {
                                    if (loggerCell.loggerId === root.selectedLoggerId)
                                        enabledSwitch.checked = checked
                                } else {
                                    checked = loggerCell.loggerEnabled
                                }
                            }

                            Binding {
                                target: rowSwitch
                                property: "checked"
                                value: loggerCell.loggerEnabled
                                restoreMode: Binding.RestoreNone
                            }
                        }

                        // Col 5: Status
                        RowLayout {
                            visible: loggerCell.column === 5
                            anchors {
                                fill: parent
                                leftMargin: 6
                                rightMargin: 8
                            }
                            spacing: 4

                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: !loggerCell.loggerEnabled ? AppColors.outline
                                     : (loggerCell.online ? AppColors.success : AppColors.error)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: !loggerCell.loggerEnabled ? qsTr("Off")
                                    : (loggerCell.online ? qsTr("Online") : qsTr("Offline"))
                                font: AppTypography.bodySmall
                                color: !loggerCell.loggerEnabled ? AppColors.textMuted
                                     : (loggerCell.online ? AppColors.success : AppColors.onSurfaceVariant)
                                elide: Text.ElideRight
                            }

                            Label {
                                visible: loggerCell.loggerEnabled && loggerCell.online && loggerCell.anyAlarm
                                text: "!"
                                color: AppColors.error
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════════════
        // Column 2 — Inline Logger Form
        // ══════════════════════════════════════════════════════════════
        ElevatedPane {
            Layout.fillWidth: true
            Layout.fillHeight: root.wideThreeColumn
            Layout.preferredWidth: root.wideThreeColumn ? 310 : -1
            Layout.horizontalStretchFactor: root.wideThreeColumn ? 31 : 1
            Layout.preferredHeight: root.wideThreeColumn ? -1 : 520
            Layout.minimumWidth: root.wideThreeColumn ? 280 : 0
            Layout.minimumHeight: 260
            padding: 0
            contentSpacing: 0

            SectionHeader {
                Layout.fillWidth: true
                Layout.leftMargin: AppTheme.sectionSpacing
                Layout.rightMargin: AppTheme.sectionSpacing
                Layout.topMargin: AppTheme.sectionSpacing
                Layout.bottomMargin: AppTheme.spacingS
                title: root.mode === "add"
                       ? qsTr("Add Logger")
                       : qsTr("Central Settings")

                RowLayout {
                    spacing: AppTheme.spacingXS

                    Label {
                        text: qsTr("Monitor")
                        font: AppTypography.bodySmall
                        color: AppColors.textMuted
                    }

                    Switch {
                        id: enabledSwitch
                        checked: true
                        Accessible.name: qsTr("Monitor in Central")
                        onToggled: {
                            if (root.mode === "edit" && root.selectedLoggerId > 0
                                    && !LoggerFormController.setLoggerEnabled(root.selectedLoggerId, checked))
                                checked = !checked
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                implicitHeight: 1
                color: AppColors.dividerLine
            }

            ScrollView {
                id: formScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: formScroll.availableWidth
                    spacing: AppTheme.spacingSM

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        Layout.topMargin: AppTheme.spacingXS
                        text: qsTr("Changes here apply only to Central.")
                        color: AppColors.textMuted
                        font: AppTypography.bodySmall
                        wrapMode: Text.WordWrap
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        columns: 2
                        columnSpacing: AppTheme.spacingSM
                        rowSpacing: AppTheme.spacingSM

                        Label { text: qsTr("Station Code *") }
                        TextField {
                            id: stationCodeField
                            objectName: "loggerStationCodeField"
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            placeholderText: qsTr("STATION-01 (auto-filled or manual)")
                            readOnly: root.mode === "edit"
                            onTextEdited: root.manualStationCode = text.trim().length > 0
                        }

                        Label { text: qsTr("Display name *") }
                        TextField {
                            id: nameField
                            objectName: "loggerDisplayNameField"
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            placeholderText: qsTr("Pumping Station 1")
                            onTextEdited: root.manualName = text.trim().length > 0
                        }

                        Label { text: qsTr("Host *") }
                        TextField {
                            id: hostField
                            objectName: "loggerHostField"
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            placeholderText: qsTr("192.168.1.50 or hostname")
                            onTextChanged: root.invalidateProbe()
                        }

                        Label {
                            visible: hostField.text.trim().length > 0 && !root.hostValid
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: qsTr("Host must be a valid IPv4 address or hostname.")
                            color: AppColors.error
                            font: AppTypography.labelSmall
                            wrapMode: Text.WordWrap
                        }

                        Label { text: qsTr("Modbus port") }
                        SpinBox {
                            id: modbusPortSpin
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            from: 1; to: 65535
                            value: AppDefaults.modbusPort
                            editable: true
                            live: true
                        }

                        Label { text: qsTr("Modbus unit ID") }
                        SpinBox {
                            id: modbusUnitIdSpin
                            objectName: "loggerModbusUnitField"
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            from: 1; to: 247
                            value: AppDefaults.modbusUnitId
                            editable: true
                            live: true
                            onValueModified: root.manualUnitId = true
                        }

                        Label { text: qsTr("Central poll (s)") }
                        SpinBox {
                            id: pollIntervalSpin
                            objectName: "centralPollIntervalField"
                            Accessible.name: qsTr("Central polling interval in seconds")
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            from: AppDefaults.minIntervalSec; to: AppDefaults.maxIntervalSec
                            value: AppDefaults.pollIntervalSec
                            editable: true
                            live: true
                        }

                        Label { text: qsTr("Timeout (s)") }
                        SpinBox {
                            id: timeoutSpin
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            from: 1; to: 60
                            value: AppDefaults.timeoutSec
                            editable: true
                            live: true
                        }

                        Label { text: qsTr("API port") }
                        SpinBox {
                            id: apiPortSpin
                            objectName: "loggerApiPortField"
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            from: 1; to: 65535
                            value: AppDefaults.apiPort
                            editable: true
                            live: true
                            onValueChanged: root.invalidateProbe()
                        }

                        Label { text: qsTr("API token") }
                        TextField {
                            id: apiTokenField
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            echoMode: TextInput.Password
                            placeholderText: qsTr("REST API Bearer token")
                            onTextChanged: root.invalidateProbe()
                        }

                        Label {
                            text: qsTr("Note")
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 8
                        }
                        TextField {
                            id: noteField
                            Layout.fillWidth: true
                            Material.containerStyle: Material.Outlined
                            placeholderText: qsTr("Optional location or maintenance note")
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        spacing: AppTheme.spacingSM
                        AppButton {
                            id: connectBtn
                            objectName: "connectLoggerConfigButton"
                            visible: root.mode === "add"
                            kind: AppButton.Tonal
                            iconName: "download"
                            text: root.probeInProgress ? qsTr("Connecting…") : qsTr("Connect && Load Config")
                            enabled: root.canProbe
                            onClicked: {
                                root.clearProbe()
                                root.probeInProgress = true
                                root.saveError = ""
                                LoggerFormController.probeConfig(
                                    hostField.text.trim(),
                                    apiPortSpin.value,
                                    apiTokenField.text.trim()
                                )
                            }

                            ToolTip.visible: !root.canProbe && root.connectDisabledHint.length > 0
                                             && (connectBtn.hovered || connectBtn.activeFocus)
                            ToolTip.text: root.connectDisabledHint
                            ToolTip.delay: 300
                        }

                        AppButton {
                            objectName: "reloadLoggerConfigButton"
                            visible: root.mode === "edit" && root.selectedLoggerId > 0
                            kind: AppButton.Secondary
                            iconName: "refresh"
                            text: root.reloadingSelection ? qsTr("Reloading…") : qsTr("Reload config")
                            tooltipText: root.usesSavedEndpoint
                                ? qsTr("Read configuration from the saved logger.")
                                : qsTr("Save connection changes in Central before reloading.")
                            enabled: root.usesSavedEndpoint && root.hostValid && !root.reloading
                            onClicked: {
                                root.forceActiveFocus()
                                root.reloadError = ""
                                LoggerFormController.reloadLoggerConfig(root.selectedLoggerId)
                            }
                        }
                    }

                    InlineBanner {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        visible: root.reloadError.length > 0
                        message: root.reloadError
                    }

                    FormNotice {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        visible: root.probeMessage.length > 0
                        semantic: root.probeStatus === "success" ? "success"
                                : root.probeStatus === "error"   ? "error"
                                : "info"
                        summary: root.probeMessage
                        onDetailRequested: (title, body) => AppNotifier.openDetail(title, body, -1)
                    }

                    FormNotice {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppTheme.sectionSpacing
                        Layout.rightMargin: AppTheme.sectionSpacing
                        visible: root.saveError.length > 0
                        semantic: "error"
                        summary: {
                            const first = root.saveError.split("\n")[0]
                            return first.length > 0 ? first : root.saveError
                        }
                        detailText: root.saveError.split("\n").length > 1 ? root.saveError : ""
                        onDetailRequested: (title, body) => AppNotifier.openDetail(title, body, -1)
                    }

                    Item { Layout.preferredHeight: AppTheme.spacingS }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                implicitHeight: 1
                color: AppColors.dividerLine
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: AppTheme.spacingSM
                spacing: AppTheme.spacingS

                Item { Layout.fillWidth: true }

                AppButton {
                    kind: AppButton.Secondary
                    text: qsTr("Reset")
                    onClicked: {
                        if (root.mode === "edit" && root.selectedLoggerId > 0)
                            root.selectLoggerForEdit(root.selectedLoggerId)
                        else
                            root.openAddMode()
                    }
                }

                AppButton {
                    id: saveBtn
                    objectName: "saveLoggerButton"
                    kind: AppButton.Primary
                    iconName: "save"
                    text: qsTr("Save")
                    enabled: root.canSave

                    ToolTip.visible: !enabled && (saveBtn.hovered || saveBtn.activeFocus)
                    ToolTip.text: root.mode === "add" && !root.probeLoaded
                        ? qsTr("Load config from the device first (Connect).")
                        : qsTr("Station code, name, and valid host are required.")
                    ToolTip.delay: 300

                    onClicked: {
                        root.forceActiveFocus()
                        root.saveError = ""
                        LoggerFormController.saveLoggerFromForm(
                            root.mode === "add",
                            root.selectedLoggerId,
                            stationCodeField.text,
                            nameField.text,
                            hostField.text,
                            modbusPortSpin.value,
                            apiPortSpin.value,
                            apiTokenField.text,
                            modbusUnitIdSpin.value,
                            pollIntervalSpin.value,
                            timeoutSpin.value,
                            enabledSwitch.checked,
                            noteField.text
                        )
                    }
                }
            }
        }

                // ══════════════════════════════════════════════════════════════
                // Column 3 — Sensor thresholds (read-only)
                // ══════════════════════════════════════════════════════════════
                SensorThresholdsPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: root.wideThreeColumn
                    Layout.preferredWidth: root.wideThreeColumn ? 310 : -1
                    Layout.horizontalStretchFactor: root.wideThreeColumn ? 31 : 1
                    Layout.preferredHeight: root.wideThreeColumn ? -1 : 380
                    Layout.minimumWidth: root.wideThreeColumn ? 280 : 0
                    Layout.minimumHeight: 260
                    loggerId: root.mode === "edit" ? root.selectedLoggerId : -1
                }
            }
        }
    }
}
