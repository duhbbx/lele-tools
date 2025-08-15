<#
    PowerShell Script: Build Qt Project and Package with IFW
    Usage:
        ./build_and_package.ps1            -> Release build
        ./build_and_package.ps1 Debug      -> Debug build
#>

param(
    [string]$BuildType = "Release"  # Default build type
)

# ==== 1. Save current directory ====
$OldDir = Get-Location

# ==== 2. Paths ====
$QtPath       = 'C:/Qt/6.9.1/msvc2022_64'
$NinjaPath    = 'C:/Qt/Tools/Ninja/ninja.exe'
$CMakePath    = 'C:/Qt/Tools/CMake_64/bin/cmake.exe'
$Windeployqt  = "$QtPath/bin/windeployqt.exe"
$VSDevBat     = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$BuildDir     = 'build1'
$IFWPath      = 'C:/Qt/Tools/QtInstallerFramework/4.10/bin'
$InstallerOut = 'LeleToolsInstaller.exe'

# ==== 3. Go to project root ====
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

# ==== 4. Remove old project_installer ====
$IFWProject = Join-Path (Get-Location) "project_installer"
if (Test-Path $IFWProject) {
    Write-Host "Removing old project_installer..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $IFWProject
}

# ==== 5. Create and go to build directory ====
if (-Not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}
Set-Location $BuildDir

# ==== 6. MSVC environment + CMake configure + Build ====
Write-Host "Configuring and building project..." -ForegroundColor Cyan
cmd /c "call `"$VSDevBat`" && `"$CMakePath`" .. -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PREFIX_PATH=`"$QtPath`" -DCMAKE_MAKE_PROGRAM=`"$NinjaPath`" && `"$CMakePath`" --build . --parallel"

# ==== 7. Find main Qt exe (exclude CMake test executables) ====
$ExeFile = Get-ChildItem -Recurse -Filter '*.exe' |
    Where-Object { $_.FullName -notmatch '\\CMakeFiles\\' -and $_.Name -ne 'moc.exe' -and $_.Name -ne 'rcc.exe' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-Not $ExeFile) {
    Write-Host "No Qt executable found. Please check your build output." -ForegroundColor Red
    Set-Location $OldDir
    exit 1
}

Write-Host "Build finished! Executable file:" $ExeFile.FullName -ForegroundColor Green

# ==== 8. Deploy Qt dependencies ====
Write-Host "Deploying Qt dependencies..." -ForegroundColor Cyan
& $Windeployqt --release $ExeFile.FullName
Write-Host "Qt dependencies deployed to:" $ExeFile.DirectoryName -ForegroundColor Green

# ==== 9. Prepare IFW installer structure ====
Write-Host "Preparing IFW installer structure..." -ForegroundColor Cyan

# Define folder structure
$ConfigDir   = Join-Path $IFWProject "config"
$PackagesDir = Join-Path $IFWProject "packages/com.leletools/data"
$MetaDir     = Join-Path $IFWProject "packages/com.leletools/meta"

# Create directories
foreach ($dir in @($ConfigDir, $PackagesDir, $MetaDir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Write-Host "Created directory:" $dir -ForegroundColor Green
}

# ==== 9.1 config.xml ====
$ConfigXml = @"
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>Lele Tools</Name>
    <Version>1.0.0</Version>
    <Title>Lele Tools Installer</Title>
    <Publisher>Your Name</Publisher>
    <StartMenuDir>LeleTools</StartMenuDir>
    <TargetDir>@HomeDir@/LeleTools</TargetDir>
</Installer>
"@
$ConfigXmlPath = Join-Path $ConfigDir "config.xml"
$ConfigXml | Out-File -Encoding UTF8 $ConfigXmlPath
Write-Host "config.xml created at:" $ConfigXmlPath -ForegroundColor Green

# ==== 9.2 package.xml ====
$PackageXml = @"
<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>Lele Tools</DisplayName>
    <Description>Lele Tools Application</Description>
    <Version>1.0.0</Version>
    <ReleaseDate>2025-08-15</ReleaseDate>
    <Default>true</Default>
    <Script>installscript.qs</Script>
</Package>
"@
$PackageXmlPath = Join-Path $MetaDir "package.xml"
$PackageXml | Out-File -Encoding UTF8 $PackageXmlPath
Write-Host "package.xml created at:" $PackageXmlPath -ForegroundColor Green

# ==== 9.3 installscript.qs ====
$InstallScript = @"
function Component() {}

Component.prototype.install = function() {
    console.log("Installing Lele Tools...");
}
"@
$InstallScriptPath = Join-Path $MetaDir "installscript.qs"
$InstallScript | Out-File -Encoding UTF8 $InstallScriptPath
Write-Host "installscript.qs created at:" $InstallScriptPath -ForegroundColor Green

# ==== 9.4 Copy exe + dependencies to packages/data ====
Write-Host "Copying exe and DLLs to packages/data..." -ForegroundColor Cyan
# 清空 packages/data 目录，避免重复嵌套
if (Test-Path $PackagesDir) {
    Remove-Item -Recurse -Force $PackagesDir
}
New-Item -ItemType Directory -Force -Path $PackagesDir | Out-Null

# 只复制主 exe 和 DLL 文件
$FilesToCopy = Get-ChildItem -Path $ExeFile.DirectoryName -File -Include *.exe, *.dll, *.pdb
foreach ($file in $FilesToCopy) {
    Copy-Item -Path $file.FullName -Destination $PackagesDir -Force
    Write-Host "Copied:" $file.Name -ForegroundColor Green
}

Write-Host "Copied exe and dependencies to:" $PackagesDir -ForegroundColor Green

# ==== 10. Build installer with IFW ====
$BinaryCreator = Join-Path $IFWPath "binarycreator.exe"
Write-Host "Creating installer with binarycreator.exe..." -ForegroundColor Cyan
Write-Host "BinaryCreator Path:" $BinaryCreator
Write-Host "Config XML:" $ConfigXmlPath
Write-Host "Packages Dir:" $IFWProject\packages
Write-Host "Installer Output:" $InstallerOut

if (-Not (Test-Path $BinaryCreator)) {
    Write-Host "binarycreator.exe not found! Check IFWPath." -ForegroundColor Red
} else {
    & $BinaryCreator --config $ConfigXmlPath --packages (Join-Path $IFWProject "packages") $InstallerOut
    Write-Host "Installer created successfully:" (Join-Path (Get-Location) $InstallerOut) -ForegroundColor Green
}

# ==== 11. Return to original directory ====
Set-Location $OldDir
Write-Host "Returned to original directory:" $OldDir -ForegroundColor Cyan
