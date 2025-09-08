# 重新构建项目以包含终端工具模块
param(
    [string]$BuildType = "Debug",
    [string]$BuildDir = "cmake-build-debug-visual-studio"
)

Write-Host "=== 重新构建项目以包含终端工具模块 ===" -ForegroundColor Cyan
Write-Host "构建类型: $BuildType" -ForegroundColor Yellow
Write-Host "构建目录: $BuildDir" -ForegroundColor Yellow

# 获取项目根目录
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Write-Host "项目根目录: $ProjectRoot" -ForegroundColor Green

# 检查终端工具模块文件是否存在
$TerminalToolHeader = Join-Path $ProjectRoot "modules\terminal-tool\terminaltool.h"
$TerminalToolSource = Join-Path $ProjectRoot "modules\terminal-tool\terminaltool.cpp"

if (-not (Test-Path $TerminalToolHeader)) {
    Write-Host "错误: 终端工具头文件不存在: $TerminalToolHeader" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $TerminalToolSource)) {
    Write-Host "错误: 终端工具源文件不存在: $TerminalToolSource" -ForegroundColor Red
    exit 1
}

Write-Host "终端工具模块文件检查通过" -ForegroundColor Green

# 进入项目根目录
Set-Location $ProjectRoot

# 清理旧的构建目录
$FullBuildDir = Join-Path $ProjectRoot $BuildDir
if (Test-Path $FullBuildDir) {
    Write-Host "清理旧的构建目录: $FullBuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $FullBuildDir
}

# 创建新的构建目录
New-Item -ItemType Directory -Path $FullBuildDir | Out-Null
Set-Location $FullBuildDir

# 查找Qt安装路径
$QtPath = $env:Qt6_DIR
if (-not $QtPath) {
    $QtPath = "C:\Qt\6.9.1\msvc2022_64"
}

if (-not (Test-Path $QtPath)) {
    Write-Host "错误: Qt安装路径不存在: $QtPath" -ForegroundColor Red
    Write-Host "请设置正确的Qt路径或安装Qt 6.9.1" -ForegroundColor Red
    exit 1
}

Write-Host "Qt路径: $QtPath" -ForegroundColor Green

# 配置CMake
Write-Host "配置CMake..." -ForegroundColor Yellow
$CMakeArgs = @(
    $ProjectRoot,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_PREFIX_PATH=$QtPath"
)

# 检查是否有vcpkg
$VcpkgPath = "C:\vcpkg"
if (Test-Path $VcpkgPath) {
    $VcpkgToolchain = "$VcpkgPath\scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $VcpkgToolchain) {
        $CMakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain"
        $CMakeArgs += "-DENABLE_REDIS=ON"
        Write-Host "使用vcpkg工具链，启用Redis支持" -ForegroundColor Green
    }
} else {
    $CMakeArgs += "-DENABLE_REDIS=OFF"
    Write-Host "未找到vcpkg，禁用Redis支持" -ForegroundColor Yellow
}

Write-Host "运行CMake配置..." -ForegroundColor Yellow
& cmake @CMakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake配置失败!" -ForegroundColor Red
    exit 1
}

# 构建项目
Write-Host "构建项目..." -ForegroundColor Yellow
& cmake --build . --config $BuildType --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "项目构建失败!" -ForegroundColor Red
    exit 1
}

# 检查可执行文件
$ExeFile = Get-ChildItem -Path $BuildType -Filter "*.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "*lele*" } |
    Select-Object -First 1

if ($ExeFile) {
    Write-Host "构建成功!" -ForegroundColor Green
    Write-Host "可执行文件: $($ExeFile.FullName)" -ForegroundColor Cyan
    
    # 检查文件大小
    $FileSize = [math]::Round($ExeFile.Length / 1MB, 2)
    Write-Host "文件大小: $FileSize MB" -ForegroundColor Cyan
    
    Write-Host ""
    Write-Host "=== 构建完成 ===" -ForegroundColor Green
    Write-Host "现在可以运行应用程序测试终端工具模块" -ForegroundColor Green
    Write-Host "终端工具应该会出现在工具列表中" -ForegroundColor Green
} else {
    Write-Host "警告: 未找到可执行文件" -ForegroundColor Yellow
    Write-Host "请检查构建日志" -ForegroundColor Yellow
}

# 返回项目根目录
Set-Location $ProjectRoot
