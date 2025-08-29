# 单独构建ScreenCapture库的脚本

param(
    [string]$BuildType = "Release",
    [string]$QtPath = "C:\Qt\6.9.1\msvc2022_64"
)

Write-Host "🔨 正在单独构建ScreenCapture库..." -ForegroundColor Green

$projectRoot = Split-Path $PSScriptRoot -Parent
$screenCaptureDir = Join-Path $projectRoot "common\thirdparty\screen-capture"
$buildDir = Join-Path $screenCaptureDir "build_standalone"

Write-Host "ScreenCapture目录: $screenCaptureDir" -ForegroundColor Yellow
Write-Host "构建目录: $buildDir" -ForegroundColor Yellow
Write-Host "Qt路径: $QtPath" -ForegroundColor Yellow

# 检查目录是否存在
if (-not (Test-Path $screenCaptureDir)) {
    Write-Host "❌ ScreenCapture目录不存在: $screenCaptureDir" -ForegroundColor Red
    exit 1
}

# 清理并创建构建目录
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

try {
    Push-Location $buildDir
    
    Write-Host "`n🔧 配置CMake..." -ForegroundColor Cyan
    
    # 使用完整路径调用cmake
    $cmakePath = Join-Path $QtPath "bin\cmake.exe"
    if (-not (Test-Path $cmakePath)) {
        # 如果Qt目录中没有cmake，尝试系统PATH
        $cmakePath = "cmake"
    }
    
    $cmakeArgs = @(
        "..",
        "-DCMAKE_PREFIX_PATH=$QtPath",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-G", "Visual Studio 17 2022",
        "-A", "x64"
    )
    
    & $cmakePath @cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ CMake配置失败" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "`n🔨 开始构建ScreenCapture库..." -ForegroundColor Cyan
    
    # 构建特定目标
    & $cmakePath --build . --target ScreenCaptureLib --config $BuildType
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ ScreenCapture库构建成功！" -ForegroundColor Green
        
        # 显示生成的文件
        $libFiles = Get-ChildItem -Path . -Name "*.lib" -Recurse
        if ($libFiles.Count -gt 0) {
            Write-Host "`n📁 生成的库文件:" -ForegroundColor Cyan
            foreach ($lib in $libFiles) {
                Write-Host "  • $lib" -ForegroundColor White
            }
        }
    } else {
        Write-Host "❌ ScreenCapture库构建失败" -ForegroundColor Red
        exit 1
    }
    
} finally {
    Pop-Location
}

Write-Host "`n🎉 ScreenCapture库单独构建完成！" -ForegroundColor Green



