import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

import CentralLogger.Core
import CentralLogger.Theme

// Add/Edit logger form. Config REST only here: Connect (Add / Edit retry) loads GET
// snapshot; Save uses saveLoggerFromForm (DB transaction + POST patch).
Dialog {
    id: root

    property string mode: "add" // "add" | "edit"
    property var    initialData: ({})
    property int    loggerId: -1

    property bool   configLoaded: false
    property bool   configLoading: false
    property string configLoadError: ""
    property bool   saveInProgress: false

    property string probeStatus: ""
    property string probeMessage: ""

    property bool   _suppressInvalidate: false

    signal saved(int loggerId)

    readonly property bool formWide: parent
        && parent.width >= AppTheme.dialogFormWideBreakpoint
    readonly property int formColumns: formWide ? 4 : 2

    readonly property bool showConnect: root.mode === "add" || !root.configLoaded

    title: mode === "add" ? qsTr("Add Logger") : qsTr("Edit Logger")
    modal: true
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape
    width: parent
        ? Math.max(360, Math.min(parent.width - 64,
            formWide ? AppTheme.dialogMaxWidth : AppTheme.dialogNarrowMaxWidth))
        : AppTheme.dialogMaxWidth
    anchors.centerIn: parent

    Material.roundedScale: Material.ExtraLargeScale

    function applyLoadedConfigToFields() {
        const devicePoll = DashboardController.probedPollInterval();
        if (devicePoll > 0)
            pollIntervalSpin.value = devicePoll;
        const edgeName = DashboardController.probedStationName();
        if (edgeName.length > 0)
            nameField.text = edgeName;
        const edgeUnit = DashboardController.probedModbusUnitId();
        if (edgeUnit > 0)
            modbusUnitIdSpin.value = edgeUnit;
    }

    function invalidateConfig() {
        if (root._suppressInvalidate)
            return;
        if (!root.configLoaded && !DashboardController.hasProbedConfig())
            return;
        DashboardController.clearProbedConfig();
        root.configLoaded = false;
        root.configLoadError = "";
        root.probeStatus = "";
        root.probeMessage = "";
    }

    function onConfigLoadSuccess() {
        root.configLoaded = true;
        root.configLoadError = "";
        root.probeStatus = "success";
        root.probeMessage = qsTr("Device config loaded.");
        root.applyLoadedConfigToFields();
    }

    function open(mode, initialData, loggerId) {
        root._suppressInvalidate = true;
        root.mode = mode || "add";
        root.initialData = initialData || ({});
        root.loggerId = loggerId !== undefined ? loggerId : -1;
        nameField.text           = root.initialData.name                  ?? "";
        hostField.text           = root.initialData.host                  ?? "";
        modbusPortSpin.value     = root.initialData.modbusPort            ?? 5020;
        modbusUnitIdSpin.value   = root.initialData.modbusUnitId          ?? 1;
        pollIntervalSpin.value   = root.initialData.centralPollIntervalS ?? 2;
        timeoutSpin.value        = root.initialData.timeoutS              ?? 2;
        apiPortSpin.value        = root.initialData.apiPort               ?? 8080;
        apiTokenField.text       = root.initialData.apiToken              ?? "";
        root.configLoaded = false;
        root.configLoading = false;
        root.configLoadError = "";
        root.saveInProgress = false;
        root.probeStatus = "";
        root.probeMessage = "";
        DashboardController.clearProbedConfig();
        DashboardController.clearLastError();
        visible = true;
        root._suppressInvalidate = false;

        if (root.mode === "edit" && root.loggerId >= 0) {
            root.configLoading = true;
            DashboardController.loadConfigForForm(root.loggerId);
        }
    }

    readonly property bool hostValid:
           hostField.text.trim().length === 0
        || DashboardController.isValidHost(hostField.text.trim())

    readonly property bool canSave:
           nameField.text.trim().length > 0
        && hostField.text.trim().length > 0
        && root.hostValid

    readonly property bool canProbe:
           hostField.text.trim().length > 0
        && root.hostValid
        && apiPortSpin.value > 0
        && !root.configLoading
        && !root.saveInProgress

    readonly property string connectDisabledHint: {
        if (root.configLoading || root.saveInProgress)
            return "";
        if (hostField.text.trim().length === 0)
            return qsTr("Enter Host to connect to the device REST API.");
        if (!root.hostValid)
            return qsTr("Host must be a valid IPv4 address or hostname.");
        if (apiPortSpin.value <= 0)
            return qsTr("API port must be greater than 0.");
        return "";
    }

    Connections {
        target: DashboardController
        function onProbeConfigResult(ok, errorMessage) {
            root.configLoading = false;
            if (ok) {
                root.onConfigLoadSuccess();
            } else {
                root.configLoaded = false;
                root.configLoadError = errorMessage;
                root.probeStatus = "error";
                root.probeMessage = errorMessage;
            }
        }
        function onConfigLoadForFormFinished(ok, errorMessage) {
            root.configLoading = false;
            if (ok) {
                root.onConfigLoadSuccess();
                if (root.initialData.status === "offline") {
                    root.probeMessage = qsTr("Device config loaded from REST API.");
                }
            } else {
                root.configLoaded = false;
                root.configLoadError = errorMessage;
                root.probeStatus = "error";
                root.probeMessage = errorMessage;
                if (root.initialData.status === "offline") {
                    root.probeMessage = errorMessage + "\n"
                        + qsTr("Device may be unreachable — fix host/network, then Connect.");
                }
            }
        }
        function onFormSaveFinished(ok, savedLoggerId, errorMessage) {
            root.saveInProgress = false;
            if (ok) {
                root.saved(savedLoggerId);
                root.close();
            } else if (errorMessage.length > 0) {
                root.configLoadError = errorMessage;
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        GridLayout {
            id: formFields
            Layout.fillWidth: true
            columns: root.formColumns
            columnSpacing: 16
            rowSpacing: AppTheme.formRowSpacing

            // Row 0 — Name | Modbus port (wide: cols 2–3)
            Label {
                text: qsTr("Name *")
                Layout.row: 0
                Layout.column: 0
            }
            TextField {
                id: nameField
                Layout.row: 0
                Layout.column: 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                placeholderText: qsTr("Trạm bơm số 1")
                enabled: root.configLoaded
            }
            Label {
                text: qsTr("Modbus port")
                visible: root.formWide
                Layout.row: 0
                Layout.column: 2
            }
            SpinBox {
                id: modbusPortSpin
                Layout.row: root.formWide ? 0 : 6
                Layout.column: root.formWide ? 3 : 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                from: 1; to: 65535
                value: 5020
                editable: true
            }
            Label {
                text: qsTr("Modbus port")
                visible: !root.formWide
                Layout.row: 6
                Layout.column: 0
            }

            // Row 1 — Host | Modbus unit ID (wide: cols 2–3)
            Label {
                text: qsTr("Host *")
                Layout.row: 1
                Layout.column: 0
            }
            TextField {
                id: hostField
                Layout.row: 1
                Layout.column: 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                placeholderText: "192.168.1.50 hoặc hostname"
                onTextChanged: if (root.visible) root.invalidateConfig()
            }
            Label {
                text: qsTr("Modbus unit ID")
                visible: root.formWide
                Layout.row: 1
                Layout.column: 2
            }
            SpinBox {
                id: modbusUnitIdSpin
                Layout.row: root.formWide ? 1 : 7
                Layout.column: root.formWide ? 3 : 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                from: 1; to: 247
                value: 1
                editable: true
            }
            Label {
                text: qsTr("Modbus unit ID")
                visible: !root.formWide
                Layout.row: 7
                Layout.column: 0
            }

            // Row 2 — host validation (full width)
            Label {
                text: qsTr("Host phải là IPv4 hoặc hostname hợp lệ")
                visible: hostField.text.trim().length > 0 && !root.hostValid
                color: AppColors.error
                font: AppTypography.labelSmall
                Layout.row: 2
                Layout.column: 0
                Layout.columnSpan: root.formColumns
                wrapMode: Text.WordWrap
            }

            // Row 3 — Poll | Timeout (wide) or Poll only (narrow label)
            Label {
                text: qsTr("Poll interval (s)")
                Layout.row: 3
                Layout.column: 0
            }
            SpinBox {
                id: pollIntervalSpin
                Layout.row: 3
                Layout.column: 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                from: 1; to: 3600
                value: 2
                editable: true
                enabled: root.configLoaded
            }
            Label {
                text: qsTr("Timeout (s)")
                visible: root.formWide
                Layout.row: 3
                Layout.column: 2
            }
            SpinBox {
                id: timeoutSpin
                Layout.row: root.formWide ? 3 : 4
                Layout.column: root.formWide ? 3 : 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                from: 1; to: 60
                value: 2
                editable: true
            }
            Label {
                text: qsTr("Timeout (s)")
                visible: !root.formWide
                Layout.row: 4
                Layout.column: 0
            }

            // Row 4 — API port | API token (wide)
            Label {
                text: qsTr("API port")
                Layout.row: root.formWide ? 4 : 5
                Layout.column: 0
            }
            SpinBox {
                id: apiPortSpin
                Layout.row: root.formWide ? 4 : 5
                Layout.column: 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                from: 1; to: 65535
                value: 8080
                editable: true
                onValueChanged: if (root.visible) root.invalidateConfig()
            }
            Label {
                text: qsTr("API token")
                visible: root.formWide
                Layout.row: 4
                Layout.column: 2
            }
            TextField {
                id: apiTokenField
                Layout.row: root.formWide ? 4 : 6
                Layout.column: root.formWide ? 3 : 1
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                echoMode: TextInput.Password
                placeholderText: qsTr("REST API token from edge (or scan QR)")
                onTextChanged: if (root.visible) root.invalidateConfig()
            }
            Label {
                text: qsTr("API token")
                visible: !root.formWide
                Layout.row: 6
                Layout.column: 0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: root.showConnect

            AppButton {
                id: connectBtn
                kind: AppButton.Tonal
                iconName: "download"
                text: root.configLoading ? qsTr("Connecting…") : qsTr("Connect && Load Config")
                enabled: root.canProbe
                onClicked: {
                    root.configLoading = true;
                    root.configLoadError = "";
                    root.probeStatus = "";
                    root.probeMessage = "";
                    DashboardController.probeConfig(
                        hostField.text.trim(),
                        apiPortSpin.value,
                        apiTokenField.text.trim());
                }

                ToolTip.visible: !root.canProbe && root.connectDisabledHint.length > 0
                                 && (connectBtn.hovered || connectBtn.activeFocus)
                ToolTip.text: root.connectDisabledHint
                ToolTip.delay: 300
            }

            BusyIndicator {
                running: root.configLoading
                visible: root.configLoading
                implicitWidth: 24
                implicitHeight: 24
            }
        }

        BusyIndicator {
            running: root.configLoading && !root.showConnect
            visible: root.configLoading && !root.showConnect
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: root.probeMessage
            visible: root.probeMessage.length > 0
            color: root.probeStatus === "success"
                   ? AppColors.success
                   : AppColors.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font: AppTypography.labelMedium
        }

        Label {
            text: root.configLoadError.length > 0
                  ? root.configLoadError
                  : DashboardController.lastError
            visible: root.configLoadError.length > 0 || DashboardController.lastError.length > 0
            color: AppColors.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    footer: DialogButtonBox {
        spacing: 12

        AppButton {
            kind: AppButton.Text
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            enabled: !root.saveInProgress
            onClicked: root.close()
        }
        AppButton {
            id: saveBtn
            kind: AppButton.Primary
            text: root.saveInProgress ? qsTr("Saving…") : qsTr("Save")
            enabled: root.canSave && root.configLoaded
                     && !root.saveInProgress && !root.configLoading
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole

            ToolTip.visible: !enabled && (saveBtn.hovered || saveBtn.activeFocus)
            ToolTip.text: !root.configLoaded
                ? qsTr("Load config from the device first (Connect).")
                : qsTr("Name and valid host are required.")
            ToolTip.delay: 300

            onClicked: {
                root.forceActiveFocus();
                root.saveInProgress = true;
                root.configLoadError = "";
                DashboardController.saveLoggerFromForm(
                    root.mode === "add",
                    root.loggerId,
                    nameField.text,
                    hostField.text,
                    modbusPortSpin.value,
                    apiPortSpin.value,
                    apiTokenField.text,
                    modbusUnitIdSpin.value,
                    pollIntervalSpin.value,
                    timeoutSpin.value);
            }
        }
    }
}
