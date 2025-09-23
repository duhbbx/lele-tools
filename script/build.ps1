# One-click build, deploy and package script
param(
    [string]$BuildType = "Release",
    [string]$Version = "1.0.0",
    [switch]$IncrementVersion = $false
)

$ErrorActionPreference = "Stop"

# Save original directory
$OriginalDirectory = Get-Location

# Function to ensure we always return to original directory
function Cleanup-And-Exit {
    param([int]$ExitCode = 0)
    
    Write-Host "`nReturning to original directory: $OriginalDirectory" -ForegroundColor Yellow
    Set-Location $OriginalDirectory
    
    if ($ExitCode -ne 0) {
        Write-Host "Build failed with exit code: $ExitCode" -ForegroundColor Red
    }
    
    exit $ExitCode
}

# Set up error handling to always return to original directory
trap {
    Write-Host "`nUnexpected error occurred: $_" -ForegroundColor Red
    Cleanup-And-Exit -ExitCode 1
}

Write-Host "=== Lele Tools One-Click Build Script ===" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor Yellow

# Handle version increment
if ($IncrementVersion) {
    $versionParts = $Version.Split('.')
    if ($versionParts.Length -eq 3) {
        $build = [int]$versionParts[2] + 1
        $Version = "$($versionParts[0]).$($versionParts[1]).$build"
        Write-Host "Version incremented to: $Version" -ForegroundColor Green
    }
}
Write-Host "Version: $Version" -ForegroundColor Yellow

# Configuration paths
$QtPath = 'C:/Qt/6.9.1/msvc2022_64'
$CMakePath = 'C:/Qt/Tools/CMake_64/bin/cmake.exe'
$VSDevBat = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$IFWPath = 'C:/Qt/Tools/QtInstallerFramework/4.10/bin'
$OpenCVPath = 'C:/opencv/build'  # OpenCV installation path

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

# Check if OpenCV exists and add to CMAKE_PREFIX_PATH
$CMakePrefixPath = $QtPath
if (Test-Path $OpenCVPath) {
    $CMakePrefixPath = "$QtPath;$OpenCVPath"
    Write-Host "OpenCV found at: $OpenCVPath" -ForegroundColor Green
} else {
    Write-Host "OpenCV not found at: $OpenCVPath (skipping)" -ForegroundColor Yellow
}

# Configure with appropriate OpenCV setting
if (Test-Path $OpenCVPath) {
    $ConfigCommand = "call `"$VSDevBat`" && `"$CMakePath`" `"$ProjectRoot`" -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PREFIX_PATH=`"$CMakePrefixPath`" -DWITH_OPENCV=ON"
} else {
    $ConfigCommand = "call `"$VSDevBat`" && `"$CMakePath`" `"$ProjectRoot`" -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PREFIX_PATH=`"$QtPath`" -DWITH_OPENCV=OFF"
}

cmd /c $ConfigCommand

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR Project configuration failed!" -ForegroundColor Red
    Cleanup-And-Exit -ExitCode 1
}

Write-Host "Compiling project..." -ForegroundColor Yellow
$BuildCommand = "call `"$VSDevBat`" && `"$CMakePath`" --build . --parallel"
cmd /c $BuildCommand

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR Project compilation failed!" -ForegroundColor Red
    Cleanup-And-Exit -ExitCode 1
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
    Cleanup-And-Exit -ExitCode 1
}

Write-Host "Found executable file: $($ExeFile.FullName)" -ForegroundColor Green

# ==================== 第三步：部署Qt依赖 ====================
Write-Host "`nStep 3: Deploying Qt dependencies..." -ForegroundColor Green

$Windeployqt = "$QtPath/bin/windeployqt.exe"
if (-Not (Test-Path $Windeployqt)) {
    Write-Host "ERROR windeployqt.exe not found!" -ForegroundColor Red
    Cleanup-And-Exit -ExitCode 1
}

Write-Host "Running windeployqt..." -ForegroundColor Yellow
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $ExeFile.FullName

if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING windeployqt execution has warnings, continuing..." -ForegroundColor Yellow
}

Write-Host "OK Qt dependencies deployed!" -ForegroundColor Green

# Deploy OpenCV dependencies if available
if (Test-Path $OpenCVPath) {
    Write-Host "Deploying OpenCV dependencies..." -ForegroundColor Yellow

    $OpenCVBinPath = Join-Path $OpenCVPath "x64\vc16\bin"  # VS2019/2022 compatible
    if (-not (Test-Path $OpenCVBinPath)) {
        $OpenCVBinPath = Join-Path $OpenCVPath "x64\vc15\bin"  # VS2017 fallback
    }

    if (Test-Path $OpenCVBinPath) {
        # List all DLL files first for debugging
        $AllDlls = Get-ChildItem -Path $OpenCVBinPath -Filter "*.dll"
        Write-Host "Available OpenCV DLLs in $OpenCVBinPath :" -ForegroundColor Cyan
        foreach ($dll in $AllDlls) {
            Write-Host "  $($dll.Name)" -ForegroundColor Gray
        }

        $ExeDir = $ExeFile.DirectoryName
        $copiedCount = 0

        foreach ($dll in $AllDlls) {
            try {
                Copy-Item -Path $dll.FullName -Destination $ExeDir -Force
                Write-Host "  ✓ Copied OpenCV DLL: $($dll.Name)" -ForegroundColor Green
                $copiedCount++
            }
            catch {
                Write-Host "  ✗ Failed to copy $($dll.Name) - $($_.Exception.Message)" -ForegroundColor Red
            }
        }

        Write-Host "OK OpenCV dependencies deployed! ($copiedCount DLLs copied)" -ForegroundColor Green

        # Verify OpenCV DLLs were copied
        $CopiedOpenCVDlls = Get-ChildItem -Path $ExeDir -Filter "*opencv*.dll"
        Write-Host "OpenCV DLLs in executable directory: $($CopiedOpenCVDlls.Count)" -ForegroundColor Cyan
        foreach ($dll in $CopiedOpenCVDlls) {
            Write-Host "  ✓ $($dll.Name)" -ForegroundColor Green
        }
    } else {
        Write-Host "WARNING: OpenCV bin directory not found at $OpenCVBinPath" -ForegroundColor Yellow
        Write-Host "Attempting alternative OpenCV paths..." -ForegroundColor Yellow

        # Try alternative OpenCV paths
        $AlternativePaths = @(
            "$OpenCVPath\build\x64\vc16\bin",
            "$OpenCVPath\build\x64\vc15\bin",
            "$OpenCVPath\build\bin",
            "$OpenCVPath\bin"
        )

        $found = $false
        foreach ($altPath in $AlternativePaths) {
            if (Test-Path $altPath) {
                Write-Host "  Found alternative OpenCV path: $altPath" -ForegroundColor Yellow
                $OpenCVDlls = Get-ChildItem -Path $altPath -Filter "*opencv*.dll"
                $ExeDir = $ExeFile.DirectoryName

                foreach ($dll in $OpenCVDlls) {
                    try {
                        Copy-Item -Path $dll.FullName -Destination $ExeDir -Force
                        Write-Host "  ✓ Copied OpenCV DLL: $($dll.Name)" -ForegroundColor Green
                        $found = $true
                    } catch {
                        Write-Host "  ✗ Failed to copy OpenCV DLL: $($dll.Name)" -ForegroundColor Red
                    }
                }
                break
            }
        }

        if (-not $found) {
            Write-Host "  ⚠️  No OpenCV DLLs found in any location" -ForegroundColor Yellow
        }
    }
}

# ==================== 第四步：创建安装包结构 ====================
Write-Host "`nStep 4: Creating installer structure..." -ForegroundColor Green

$IFWProject = Join-Path $ProjectRoot "project_installer"
$ConfigDir = Join-Path $IFWProject "config"
$PackagesDir = Join-Path $IFWProject "packages\com.leletools"
$MetaDir = Join-Path $PackagesDir "meta"
$DataDir = Join-Path $PackagesDir "data"
$ConfigXmlPath = Join-Path $ConfigDir "config.xml"
$PackageXmlPath = Join-Path $MetaDir "package.xml"
$ControlScriptPath = Join-Path $ConfigDir "controlscript.qs"


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
    Cleanup-And-Exit -ExitCode 1
}

$InstallerName = "LeleToolsInstaller_v$($Version.Replace('.','_')).exe"
$InstallerPath = Join-Path $ProjectRoot $InstallerName

# Delete old installer
if (Test-Path $InstallerPath) {
    Remove-Item $InstallerPath -Force
}

Write-Host "Creating installer..." -ForegroundColor Yellow
$PackagesRootDir = Split-Path $PackagesDir -Parent
Write-Host "Config path: $ConfigXmlPath" -ForegroundColor Cyan
Write-Host "Packages path: $PackagesRootDir" -ForegroundColor Cyan
Write-Host "Output path: $InstallerPath" -ForegroundColor Cyan

& $BinaryCreator --offline-only -c $ConfigXmlPath -p $PackagesRootDir $InstallerPath

if ($LASTEXITCODE -eq 0 -and (Test-Path $InstallerPath)) {
    $InstallerSize = [math]::Round((Get-Item $InstallerPath).Length / 1MB, 2)
    Write-Host "OK Installer created successfully!" -ForegroundColor Green
    Write-Host "  File: $InstallerPath" -ForegroundColor Cyan
Write-Host "  Size: $InstallerSize MB" -ForegroundColor Cyan
} else {
    Write-Host "ERROR Installer creation failed!" -ForegroundColor Red
    Cleanup-And-Exit -ExitCode 1
}

# ==================== 完成 ====================
Write-Host "`nBUILD COMPLETED!" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan
Write-Host "OK Project compilation: Success" -ForegroundColor Green
Write-Host "OK Qt dependency deployment: Success" -ForegroundColor Green
Write-Host "OK File copying: $TotalFiles files" -ForegroundColor Green
Write-Host "OK Installer generation: $InstallerName ($InstallerSize MB)" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan

# Successfully completed, return to original directory
Cleanup-And-Exit -ExitCode 0