/**
 * Control script for the Central Logger installer.
 *
 * Problem solved: Qt IFW refuses to install into a directory that already
 * contains an installation (it shows "The directory you selected already
 * exists and contains an installation"). This script detects a previous
 * version when the Target Directory page is shown and runs the embedded
 * maintenance tool in unattended "purge" mode first, so the user can
 * upgrade in place instead of being blocked.
 */

function Controller()
{
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    if (systemInfo.productType !== "windows") {
        return;
    }

    var targetDir = installer.value("TargetDir");
    if (!targetDir) {
        return;
    }

    var maintenanceTool = targetDir + "/"
        + installer.value("MaintenanceToolName") + ".exe";

    // Fresh install — nothing to remove.
    if (!installer.fileExists(maintenanceTool)) {
        return;
    }

    var answer = QMessageBox.question(
        "cl.upgrade.confirm",
        qsTr("Existing Installation Found"),
        qsTr("An existing installation of Central Logger was found in:\n\n%1\n\n"
             + "It must be removed before the new version can be installed. "
             + "Do you want to uninstall it now and continue?").arg(targetDir),
        QMessageBox.Yes | QMessageBox.No);

    // User declined: leave the old install untouched. IFW's own check will
    // then ask the user to pick a different directory or cancel.
    if (answer !== QMessageBox.Yes) {
        return;
    }

    // Run the maintenance tool unattended. Piping "yes" to stdin confirms
    // the purge command; execute() blocks until it finishes.
    var result = installer.execute(maintenanceTool, ["purge"], "yes");
    var exitCode = result[result.length - 1];

    if (exitCode !== 0 || installer.fileExists(maintenanceTool)) {
        QMessageBox.warning(
            "cl.upgrade.failed",
            qsTr("Uninstall Failed"),
            qsTr("The previous version could not be removed automatically.\n"
                 + "Please uninstall Central Logger manually (run "
                 + "maintenancetool.exe in %1 or use Windows Settings), "
                 + "then run this installer again.").arg(targetDir));
    }
};
