/**
 * Control script for Central Logger installer.
 *
 * Problem solved: Qt IFW refuses to install into a directory that already
 * contains an existing installation (components.xml present). This script
 * detects that situation and runs the embedded uninstaller first, so the
 * user can upgrade without manually removing the old version.
 */

var gUninstallRan = false;

function Controller()
{
    // When the installer reaches the TargetDirectory page and the user
    // clicks Next, check whether we need to uninstall the previous version.
    installer.targetDirectorySelected.connect(onTargetDirectorySelected);
}

function onTargetDirectorySelected(path)
{
    var componentsXml = path + "/components.xml";
    if (!installer.fileExists(componentsXml)) {
        return; // Fresh install — nothing to do.
    }

    if (gUninstallRan) {
        return; // Already handled in this session.
    }

    var answer = QMessageBox.question(
        "uninstall.confirm",
        qsTr("Existing Installation Found"),
        qsTr("An existing installation of Central Logger was found in:\n\n%1\n\nIt must be removed before the new version can be installed. Do you want to uninstall it now and continue?").arg(path),
        QMessageBox.Yes | QMessageBox.No
    );

    if (answer !== QMessageBox.Yes) {
        // User declined — abort so they can clean up manually.
        installer.interrupt();
        return;
    }

    var maintenanceTool = path + "/maintenancetool.exe";
    if (!installer.fileExists(maintenanceTool)) {
        QMessageBox.warning(
            "uninstall.notfound",
            qsTr("Uninstaller Not Found"),
            qsTr("Could not find maintenancetool.exe in %1.\nPlease uninstall the old version manually from Windows Settings, then run this installer again.").arg(path)
        );
        installer.interrupt();
        return;
    }

    // Run the maintenance tool in unattended uninstall mode and wait for it.
    var exitCode = installer.execute(maintenanceTool, ["purge", "--confirm-command"]);
    if (exitCode[0] !== 0) {
        QMessageBox.critical(
            "uninstall.failed",
            qsTr("Uninstall Failed"),
            qsTr("The previous version could not be removed automatically (exit code %1).\nPlease uninstall Central Logger manually from Windows Settings, then run this installer again.").arg(exitCode[0])
        );
        installer.interrupt();
        return;
    }

    gUninstallRan = true;
}
