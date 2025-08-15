function Controller() {
    // Default constructor
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var targetDir = installer.value("TargetDir");
    
    // Check if directory exists and contains our application
    if (installer.fileExists(targetDir)) {
        var exePath = targetDir + "/lele-tools.exe";
        var maintenancePath = targetDir + "/maintenancetool.exe";
        
        if (installer.fileExists(exePath) || installer.fileExists(maintenancePath)) {
            // Ask user what to do with existing installation
            var reply = QMessageBox.question("existing.installation", 
                "Existing Installation Found",
                "Lele Tools is already installed in:\n" + targetDir + 
                "\n\nDo you want to replace the existing installation?\n\n" +
                "Click Yes to replace or No to choose a different directory.",
                QMessageBox.Yes | QMessageBox.No);
            
            if (reply == QMessageBox.Yes) {
                // User confirmed overwrite
                installer.setValue("OverwriteExisting", "true");
            } else {
                // User wants different directory - return without advancing
                return;
            }
        }
    }
}