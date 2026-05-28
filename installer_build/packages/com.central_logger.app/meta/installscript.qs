function Component()
{
    // default constructor
}

Component.prototype.createOperations = function()
{
    // call default implementation
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Create shortcut in the Start Menu
        component.addOperation("CreateShortcut", "@TargetDir@/central_logger.exe", "@StartMenuDir@/Central Logger.lnk",
                               "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/central_logger.exe");

        // Create shortcut on the Desktop
        component.addOperation("CreateShortcut", "@TargetDir@/central_logger.exe", "@DesktopDir@/Central Logger.lnk",
                               "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/central_logger.exe");
    }
}
