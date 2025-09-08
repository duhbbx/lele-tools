# 构建脚本 - 包含SQL支持
param(
    [string]$BuildType = "Debug",
    [string]$QtPath = "C:/Qt/6.9.1/msvc2022_64",
    [string]$VcpkgPath = "D:/projects/vcpkg"
)

Write-Host "=== Lele Tools 构建脚本 (包含SQL支持) ===" -ForegroundColor Green

# 检查Qt安装
$QtCMakePath = "$QtPath/lib/cmake"
if (-not (Test-Path $QtCMakePath)) {
    Write-Host "错误: Qt路径不存在: $QtCMakePath" -ForegroundColor Red
    Write-Host "请检查Qt安装路径" -ForegroundColor Yellow
    exit 1
}

# 检查vcpkg工具链
$VcpkgToolchain = "$VcpkgPath/scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $VcpkgToolchain)) {
    Write-Host "错误: vcpkg工具链文件不存在: $VcpkgToolchain" -ForegroundColor Red
    Write-Host "请检查vcpkg安装路径" -ForegroundColor Yellow
    exit 1
}

# 检查必要的Qt模块
Write-Host "检查Qt SQL模块..." -ForegroundColor Cyan
$QtSqlPath = "$QtPath/lib/Qt6Sql.lib"
if (-not (Test-Path $QtSqlPath)) {
    Write-Host "警告: Qt6 SQL模块可能未安装" -ForegroundColor Yellow
    Write-Host "如果编译失败，请安装Qt6 SQL模块" -ForegroundColor Yellow
}

# 清理之前的构建
$BuildDir = "cmake-build-debug-visual-studio"
if (Test-Path $BuildDir) {
    Write-Host "清理之前的构建目录..." -ForegroundColor Yellow
    Remove-Item $BuildDir -Recurse -Force
}

# 创建构建目录
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

# CMake配置
Write-Host "配置CMake..." -ForegroundColor Cyan
$CMakeArgs = @(
    ".",
    "-B", $BuildDir,
    "-DCMAKE_PREFIX_PATH=$QtCMakePath",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DWITH_REDIS=ON",
    "-DENABLE_REDIS=ON"
)

Write-Host "CMake命令: cmake $($CMakeArgs -join ' ')" -ForegroundColor Gray

$ConfigResult = & cmake @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake配置失败!" -ForegroundColor Red
    Write-Host "请检查以下依赖:" -ForegroundColor Yellow
    Write-Host "  1. Qt6 SQL模块是否已安装" -ForegroundColor Yellow
    Write-Host "  2. vcpkg是否正确安装redis-plus-plus" -ForegroundColor Yellow
    Write-Host "  3. Visual Studio 2022是否正确安装" -ForegroundColor Yellow
    exit 1
}

# 构建项目
Write-Host "开始构建..." -ForegroundColor Cyan
$BuildResult = & cmake --build $BuildDir --config $BuildType

if ($LASTEXITCODE -eq 0) {
    Write-Host "构建成功!" -ForegroundColor Green
    Write-Host "可执行文件位置: $BuildDir/lele-tools.exe" -ForegroundColor Cyan
} else {
    Write-Host "构建失败!" -ForegroundColor Red
    Write-Host "请检查编译错误信息" -ForegroundColor Yellow
}

Write-Host "构建完成" -ForegroundColor Green

