# One-click build, deploy and package script
param(
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Lele Tools One-Click Build Script ===" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor Yellow

# Configuration paths
$QtPath = 'C:/Qt/6.9.1/msvc2022_64'
$CMakePath = 'C:/Qt/Tools/CMake_64/bin/cmake.exe'
$VSDevBat = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$IFWPath = 'C:/Qt/Tools/QtInstallerFramework/4.10/bin'

# Project paths
$ScriptDir = $PSScriptRoot
$ProjectRoot = Split-Path $ScriptDir -Parent
$BuildDir = Join-Path $ProjectRoot "build1"

Write-Host "Project Root: $ProjectRoot" -ForegroundColor Cyan
Write-Host "Build Directory: $BuildDir" -ForegroundColor Cyan

# ==================== 第一步：构建项目 ====================
Write-Host "`nStep 1: Building project..." -ForegroundColor Green

# Clean old build directory
if (Test-Path $BuildDir) {
    Write-Host "Cleaning old build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
New-Item -ItemType Directory -Path $BuildDir | Out-Null
Set-Location $BuildDir

# Configure and build
Write-Host "Configuring project..." -ForegroundColor Yellow
$ConfigCommand = "call `"$VSDevBat`" && `"$CMakePath`" `"$ProjectRoot`" -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PREFIX_PATH=`"$QtPath`""
cmd /c $ConfigCommand

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR Project configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Compiling project..." -ForegroundColor Yellow
$BuildCommand = "call `"$VSDevBat`" && `"$CMakePath`" --build . --parallel"
cmd /c $BuildCommand

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR Project compilation failed!" -ForegroundColor Red
    exit 1
}

Write-Host "OK Project build successful!" -ForegroundColor Green

# ==================== 第二步：查找可执行文件 ====================
Write-Host "`nStep 2: Finding executable file..." -ForegroundColor Green

$ExeFile = Get-ChildItem -Path $BuildDir -Filter "*.exe" -Recurse | 
    Where-Object { $_.Name -like "*lele*" -or $_.Name -like "*tools*" } |
    Where-Object { $_.Name -notlike "*Installer*" } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-Not $ExeFile) {
    Write-Host "ERROR Executable file not found!" -ForegroundColor Red
    exit 1
}

Write-Host "Found executable file: $($ExeFile.FullName)" -ForegroundColor Green

# ==================== 第三步：部署Qt依赖 ====================
Write-Host "`nStep 3: Deploying Qt dependencies..." -ForegroundColor Green

$Windeployqt = "$QtPath/bin/windeployqt.exe"
if (-Not (Test-Path $Windeployqt)) {
    Write-Host "ERROR windeployqt.exe not found!" -ForegroundColor Red
    exit 1
}

Write-Host "Running windeployqt..." -ForegroundColor Yellow
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $ExeFile.FullName

if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING windeployqt execution has warnings, continuing..." -ForegroundColor Yellow
}

Write-Host "OK Qt dependencies deployed!" -ForegroundColor Green

# ==================== 第四步：创建安装包结构 ====================
Write-Host "`nStep 4: Creating installer structure..." -ForegroundColor Green

$IFWProject = Join-Path $ProjectRoot "project_installer"
$ConfigDir = Join-Path $IFWProject "config"
$PackagesDir = Join-Path $IFWProject "packages\com.leletools"
$MetaDir = Join-Path $PackagesDir "meta"
$DataDir = Join-Path $PackagesDir "data"

# 清理旧的安装包项目
if (Test-Path $IFWProject) {
    Remove-Item -Recurse -Force $IFWProject
}

# 创建目录结构
New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
New-Item -ItemType Directory -Force -Path $MetaDir | Out-Null
New-Item -ItemType Directory -Force -Path $DataDir | Out-Null

Write-Host "Installer directory structure created" -ForegroundColor Green

# ==================== 第五步：创建配置文件 ====================
Write-Host "`nStep 5: Creating configuration files..." -ForegroundColor Green

# Create config.xml
$ConfigXml = @'
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>Lele Tools</Name>
    <Version>1.0.0</Version>
    <Title>Lele Tools 安装程序</Title>
    <Publisher>Lele Tools</Publisher>
    <ProductUrl>https://github.com/lele-tools</ProductUrl>
    <TargetDir>@ApplicationsDir@/LeleTools</TargetDir>
    <StartMenuDir>LeleTools</StartMenuDir>
    <AllowNonAsciiCharacters>true</AllowNonAsciiCharacters>
    <AllowSpaceInPath>true</AllowSpaceInPath>
    <WizardStyle>Modern</WizardStyle>
</Installer>
'@

$ConfigXmlPath = Join-Path $ConfigDir "config.xml"
[System.IO.File]::WriteAllText($ConfigXmlPath, $ConfigXml, [System.Text.Encoding]::UTF8)

# Create package.xml
$PackageXml = @'
<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>Lele Tools</DisplayName>
    <Description>Lele Tools 实用工具集合</Description>
    <Version>1.0.0</Version>
    <ReleaseDate>2025-01-20</ReleaseDate>
    <Default>true</Default>
    <Essential>true</Essential>
    <Script>installscript.qs</Script>
</Package>
'@

$PackageXmlPath = Join-Path $MetaDir "package.xml"
[System.IO.File]::WriteAllText($PackageXmlPath, $PackageXml, [System.Text.Encoding]::UTF8)

# Create install script
$InstallScript = @'
function Component() {}

Component.prototype.createOperations = function() {
    component.createOperations();
    
    var targetDir = installer.value("TargetDir");
    var exePath = targetDir + "/lele-tools.exe";
    
    // 创建桌面快捷方式
    component.addOperation("CreateShortcut", 
                          exePath, 
                          "@DesktopDir@/Lele Tools.lnk",
                          "workingDirectory=" + targetDir,
                          "iconPath=" + exePath,
                          "description=Lele Tools 实用工具集合");
    
    // 创建开始菜单快捷方式
    component.addOperation("CreateShortcut", 
                          exePath, 
                          "@StartMenuDir@/Lele Tools.lnk",
                          "workingDirectory=" + targetDir,
                          "iconPath=" + exePath,
                          "description=Lele Tools 实用工具集合");
}
'@

$InstallScriptPath = Join-Path $MetaDir "installscript.qs"
[System.IO.File]::WriteAllText($InstallScriptPath, $InstallScript, [System.Text.Encoding]::UTF8)

Write-Host "OK Configuration files created!" -ForegroundColor Green

# ==================== 第六步：复制应用文件 ====================
Write-Host "`nStep 6: Copying application files..." -ForegroundColor Green

$SourceDir = $ExeFile.DirectoryName
Write-Host "Source directory: $SourceDir" -ForegroundColor Cyan
Write-Host "Target directory: $DataDir" -ForegroundColor Cyan

# Copy main program and DLL files
Write-Host "Copying executable files and DLLs..." -ForegroundColor Yellow
$FilesToCopy = Get-ChildItem -Path $SourceDir -File | 
    Where-Object { ($_.Extension -eq ".exe" -or $_.Extension -eq ".dll") -and 
                   $_.Name -notlike "*Installer*" -and 
                   $_.Name -notlike "*binarycreator*" }

$CopiedCount = 0
foreach ($file in $FilesToCopy) {
    try {
        Copy-Item -Path $file.FullName -Destination $DataDir -Force
        Write-Host "  OK $($file.Name)" -ForegroundColor Green
        $CopiedCount++
    }
    catch {
        Write-Host "  ERROR copying file: $($file.Name) - $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Copy Qt plugin directories
Write-Host "Copying Qt plugin directories..." -ForegroundColor Yellow
$QtDirs = @('platforms', 'imageformats', 'styles', 'translations', 'iconengines', 'generic', 'networkinformation', 'tls')
$DirsCopied = 0

foreach ($qtDir in $QtDirs) {
    $QtDirPath = Join-Path $SourceDir $qtDir
    if (Test-Path $QtDirPath) {
        try {
            $TargetPath = Join-Path $DataDir $qtDir
            Copy-Item -Path $QtDirPath -Destination $TargetPath -Recurse -Force
            $FileCount = (Get-ChildItem -Path $TargetPath -Recurse -File).Count
            Write-Host "  OK $qtDir/ ($FileCount files)" -ForegroundColor Green
            $DirsCopied++
        }
        catch {
            Write-Host "  ERROR copying directory: $qtDir - $($_.Exception.Message)" -ForegroundColor Red
        }
    } else {
        Write-Host "  WARNING directory not found: $qtDir" -ForegroundColor Yellow
    }
}

# Verify copy results
$TotalFiles = (Get-ChildItem -Path $DataDir -Recurse -File).Count
Write-Host "OK File copying completed!" -ForegroundColor Green
Write-Host "  - Copied $CopiedCount program files" -ForegroundColor Cyan
Write-Host "  - Copied $DirsCopied Qt plugin directories" -ForegroundColor Cyan
Write-Host "  - Total $TotalFiles files" -ForegroundColor Cyan

# ==================== 第七步：生成安装包 ====================
Write-Host "`nStep 7: Generating installer..." -ForegroundColor Green

$BinaryCreator = Join-Path $IFWPath "binarycreator.exe"
if (-Not (Test-Path $BinaryCreator)) {
    Write-Host "ERROR binarycreator.exe not found! Path: $BinaryCreator" -ForegroundColor Red
    exit 1
}

$InstallerName = "LeleToolsInstaller.exe"
$InstallerPath = Join-Path $ProjectRoot $InstallerName

# Delete old installer
if (Test-Path $InstallerPath) {
    Remove-Item $InstallerPath -Force
}

Write-Host "Creating installer..." -ForegroundColor Yellow
& $BinaryCreator --offline-only -c $ConfigXmlPath -p "$PackagesDir\.." $InstallerPath

if ($LASTEXITCODE -eq 0 -and (Test-Path $InstallerPath)) {
    $InstallerSize = [math]::Round((Get-Item $InstallerPath).Length / 1MB, 2)
    Write-Host "OK Installer created successfully!" -ForegroundColor Green
    Write-Host "  File: $InstallerPath" -ForegroundColor Cyan
Write-Host "  Size: $InstallerSize MB" -ForegroundColor Cyan
} else {
    Write-Host "ERROR Installer creation failed!" -ForegroundColor Red
    exit 1
}

# ==================== 完成 ====================
Write-Host "`nBUILD COMPLETED!" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan
Write-Host "OK Project compilation: Success" -ForegroundColor Green
Write-Host "OK Qt dependency deployment: Success" -ForegroundColor Green
Write-Host "OK File copying: $TotalFiles files" -ForegroundColor Green
Write-Host "OK Installer generation: $InstallerName ($InstallerSize MB)" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan

Set-Location $ProjectRoot