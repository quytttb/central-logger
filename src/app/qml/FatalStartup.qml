import QtQuick
import QtQuick.Controls.Material

import CentralLogger.Components
import CentralLogger.Theme

// Shown when Database::open() fails before Main.qml loads.
ApplicationWindow {
    id: root

    required property string errorMessage
    required property string dbPath
    required property string backupPath
    required property string errorKind // "migrate_fail" | "newer_than_app"

    width: 480
    height: 360
    minimumWidth: 400
    minimumHeight: 280
    visible: true
    title: qsTr("Central Logger")

    Material.theme: Material.Dark
    Material.accent: AppTheme.accent
    Material.primary: AppTheme.primary
    color: AppColors.surface

    readonly property string dialogTitle:
        errorKind === "newer_than_app"
            ? qsTr("Database version incompatible")
            : qsTr("Cannot open database")

    readonly property string dialogMessage:
        errorKind === "newer_than_app"
            ? qsTr("This database was created by a newer version of Central Logger. "
                   + "Update the application or remove the database file and restart.")
            : qsTr("Central Logger could not upgrade or open the local database.")

    readonly property string dialogDetail: {
        let lines = []
        if (errorMessage.length > 0)
            lines.push(errorMessage)
        if (dbPath.length > 0)
            lines.push(qsTr("Database: %1").arg(dbPath))
        if (errorKind === "migrate_fail" && backupPath.length > 0)
            lines.push(qsTr("Backup (if migration started): %1").arg(backupPath))
        return lines.join("\n\n")
    }

    AlertDialog {
        id: fatalDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: root.dialogTitle
        message: root.dialogMessage
        detailText: root.dialogDetail
        semantic: "error"
        showCancel: false
        confirmText: qsTr("Close")
        onConfirmed: Qt.quit()
    }

    Component.onCompleted: fatalDialog.open()
}
