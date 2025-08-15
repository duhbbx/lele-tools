var targetDirectoryPage = null;

function Component() 
{
    // 璇锋眰绠＄悊鍛樻潈闄愶紝鏂逛究瑕嗙洊绯荤粺鐩綍
    installer.gainAdminRights();
    component.loaded.connect(this, this.installerLoaded);
}

// 鍒涘缓瀹夎鎿嶄綔
Component.prototype.createOperations = function() 
{
    // 璋冪敤榛樿鎿嶄綔锛堝鍒舵枃浠讹級
    component.createOperations();

    var targetDir = installer.value("TargetDir");
    var exePath = targetDir + "/lele-tools.exe";

    // 鍒涘缓妗岄潰蹇嵎鏂瑰紡
    component.addOperation("CreateShortcut",
                           exePath,
                           "@DesktopDir@/Lele Tools.lnk",
                           "workingDirectory=" + targetDir,
                           "iconPath=" + exePath,
                           "description=Lele Tools Utility Collection");

    // 鍒涘缓寮€濮嬭彍鍗曞揩鎹锋柟寮?
    component.addOperation("CreateShortcut",
                           exePath,
                           "@StartMenuDir@/Lele Tools.lnk",
                           "workingDirectory=" + targetDir,
                           "iconPath=" + exePath,
                           "description=Lele Tools Utility Collection");
}

// 瀹夎鍣ㄥ姞杞藉畬鎴愭椂
Component.prototype.installerLoaded = function()
{
    // 闅愯棌榛樿鐩綍椤碉紝浣跨敤鑷畾涔?UI
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
    installer.addWizardPage(component, "TargetWidget", QInstaller.TargetDirectory);

    // 鑾峰彇鑷畾涔夌洰褰曢〉鎺т欢
    targetDirectoryPage = gui.pageWidgetByObjectName("DynamicTargetWidget");
    targetDirectoryPage.windowTitle = "Choose Installation Directory";
    targetDirectoryPage.description.setText("Please select where Lele Tools will be installed:");
    targetDirectoryPage.targetDirectory.textChanged.connect(this, this.targetDirectoryChanged);
    targetDirectoryPage.targetDirectory.setText(installer.value("TargetDir"));
    targetDirectoryPage.targetChooser.released.connect(this, this.targetChooserClicked);

    // 褰撹繘鍏ョ粍浠堕€夋嫨椤垫椂锛屽皾璇曡嚜鍔ㄥ嵏杞芥棫鐗堟湰
    gui.pageById(QInstaller.ComponentSelection).entered.connect(this, this.componentSelectionPageEntered);
}

// 鐩綍閫夋嫨鎸夐挳鐐瑰嚮浜嬩欢
Component.prototype.targetChooserClicked = function()
{
    var dir = QFileDialog.getExistingDirectory("", targetDirectoryPage.targetDirectory.text);
    if (dir) {
        targetDirectoryPage.targetDirectory.setText(dir);
    }
}

// 鐩綍鏂囨湰鍙樺寲浜嬩欢
Component.prototype.targetDirectoryChanged = function()
{
    var dir = targetDirectoryPage.targetDirectory.text;
    if (installer.fileExists(dir) && installer.fileExists(dir + "/maintenancetool.exe")) {
        targetDirectoryPage.warning.setText("<p style=\"color: red\">Existing installation detected and will be overwritten.</p>");
    }
    else if (installer.fileExists(dir)) {
        targetDirectoryPage.warning.setText("<p style=\"color: red\">Installing in existing directory. It may be overwritten.</p>");
    }
    else {
        targetDirectoryPage.warning.setText("");
    }
    installer.setValue("TargetDir", dir);
}

// 杩涘叆缁勪欢閫夋嫨椤垫椂鑷姩鍗歌浇鏃х増鏈?
Component.prototype.componentSelectionPageEntered = function()
{
    var dir = installer.value("TargetDir");
    if (installer.fileExists(dir) && installer.fileExists(dir + "/maintenancetool.exe")) {
        // 璋冪敤缁存姢宸ュ叿锛屼紶鍏ヨ嚜鍔ㄥ嵏杞借剼鏈?
        installer.execute(dir + "/maintenancetool.exe", "--script=" + dir + "/scripts/auto_uninstall.qs");
    }
}

// 榛樿缁勪欢琛屼负
Component.prototype.isDefault = function() { return true; }
Component.prototype.componentChangeRequested = function() { return true; }
