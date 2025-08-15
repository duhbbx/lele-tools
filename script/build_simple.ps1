# Simple Qt Installer Framework Script
param(
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

# Paths
$QtPath = 'C:/Qt/6.9.1/msvc2022_64'
$CMakePath = 'C:/Qt/Tools/CMake_64/bin/cmake.exe'
$VSDevBat = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$BuildDir = 'build1'
$IFWPath = 'C:/Qt/Tools/QtInstallerFramework/4.10/bin'

Write-Host "Building Lele Tools Installer..." -ForegroundColor Cyan

# Save current directory
$OldDir = Get-Location
$ProjectRoot = Get-Location

# Clean old installer project
$IFWProject = Join-Path $ProjectRoot "project_installer"
if (Test-Path $IFWProject) {
    Remove-Item -Recurse -Force $IFWProject
}

# Build the project
$BuildPath = Join-Path $ProjectRoot $BuildDir
if (-Not (Test-Path $BuildPath)) {
    New-Item -ItemType Directory -Path $BuildPath | Out-Null
}
Set-Location $BuildPath

Write-Host "Building project..." -ForegroundColor Yellow
cmd /c "call `"$VSDevBat`" && `"$CMakePath`" .. -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PREFIX_PATH=`"$QtPath`" && `"$CMakePath`" --build . --parallel"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Set-Location $OldDir
    exit 1
}

# Find executable
$ExeFile = Get-ChildItem -Recurse -Filter '*.exe' | 
    Where-Object { $_.Name -like '*lele*' -or $_.Name -like '*tools*' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-Not $ExeFile) {
    Write-Host "Executable not found!" -ForegroundColor Red
    Set-Location $OldDir
    exit 1
}

Write-Host "Found executable: $($ExeFile.FullName)" -ForegroundColor Green

# Deploy Qt
$Windeployqt = "$QtPath/bin/windeployqt.exe"
& $Windeployqt --release $ExeFile.FullName

# Create installer structure
$ConfigDir = Join-Path $IFWProject "config"
$PackagesDir = Join-Path $IFWProject "packages\com.leletools\data"
$MetaDir = Join-Path $IFWProject "packages\com.leletools\meta"

New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
New-Item -ItemType Directory -Force -Path $PackagesDir | Out-Null
New-Item -ItemType Directory -Force -Path $MetaDir | Out-Null

# Create config.xml
$ConfigXml = @'
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>Lele Tools</Name>
    <Version>1.0.0</Version>
    <Title>Lele Tools Installer</Title>
    <Publisher>Lele Tools</Publisher>
    <TargetDir>@ApplicationsDir@/LeleTools</TargetDir>
    <StartMenuDir>LeleTools</StartMenuDir>
</Installer>
'@

$ConfigXmlPath = Join-Path $ConfigDir "config.xml"
[System.IO.File]::WriteAllText($ConfigXmlPath, $ConfigXml, [System.Text.Encoding]::UTF8)

# Create package.xml
$PackageXml = @'
<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>Lele Tools</DisplayName>
    <Description>Lele Tools Application</Description>
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
    
    component.addOperation("CreateShortcut", 
                          exePath, 
                          "@DesktopDir@/Lele Tools.lnk",
                          "workingDirectory=" + targetDir);
}
'@

$InstallScriptPath = Join-Path $MetaDir "installscript.qs"
[System.IO.File]::WriteAllText($InstallScriptPath, $InstallScript, [System.Text.Encoding]::UTF8)

# Copy files
Write-Host "Copying files..." -ForegroundColor Yellow
$SourceDir = $ExeFile.DirectoryName

# Copy exe and dlls
Get-ChildItem -Path $SourceDir -File -Include *.exe, *.dll | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $PackagesDir -Force
    Write-Host "Copied: $($_.Name)" -ForegroundColor Green
}

# Copy Qt directories
$QtDirs = @('platforms', 'imageformats', 'styles', 'translations')
foreach ($qtDir in $QtDirs) {
    $qtDirPath = Join-Path $SourceDir $qtDir
    if (Test-Path $qtDirPath) {
        $targetPath = Join-Path $PackagesDir $qtDir
        Copy-Item -Path $qtDirPath -Destination $targetPath -Recurse -Force
        Write-Host "Copied directory: $qtDir" -ForegroundColor Green
    }
}

# Create installer
$BinaryCreator = Join-Path $IFWPath "binarycreator.exe"
$InstallerOut = "LeleToolsInstaller.exe"

if (Test-Path $BinaryCreator) {
    Write-Host "Creating installer..." -ForegroundColor Yellow
    $PackagesPath = Join-Path $IFWProject "packages"
    
    & $BinaryCreator --offline-only -c $ConfigXmlPath -p $PackagesPath $InstallerOut
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Installer created successfully!" -ForegroundColor Green
        Write-Host "Output: $(Join-Path $BuildPath $InstallerOut)" -ForegroundColor Green
    } else {
        Write-Host "Installer creation failed!" -ForegroundColor Red
    }
} else {
    Write-Host "binarycreator.exe not found at: $BinaryCreator" -ForegroundColor Red
}

Set-Location $OldDir
Write-Host "Done!" -ForegroundColor Cyan