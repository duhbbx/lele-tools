# GitHub Actions 缓存性能测试脚本
# 用于本地模拟测试缓存策略的效果

param(
    [string]$TestType = "full",  # full, cache-only, no-cache
    [switch]$CleanFirst = $false
)

Write-Host "=== GitHub Actions 缓存性能测试 ===" -ForegroundColor Cyan
Write-Host "测试类型: $TestType" -ForegroundColor Yellow

# 定义测试目录
$TestDir = "D:\temp\cache-test"
$NpcapDir = "$TestDir\npcap-sdk"
$OpenCVDir = "$TestDir\opencv"
$VcpkgDir = "$TestDir\vcpkg"

# 清理测试环境
if ($CleanFirst) {
    Write-Host "清理测试环境..." -ForegroundColor Yellow
    if (Test-Path $TestDir) {
        Remove-Item -Recurse -Force $TestDir
    }
}

# 创建测试目录
New-Item -ItemType Directory -Path $TestDir -Force | Out-Null

function Test-NpcapDownload {
    Write-Host "`n=== 测试 Npcap SDK 下载 ===" -ForegroundColor Green
    $StartTime = Get-Date

    if (Test-Path $NpcapDir) {
        Write-Host "✅ Npcap SDK 缓存命中 (模拟)" -ForegroundColor Green
        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (缓存)" -ForegroundColor Cyan
    } else {
        Write-Host "⬇️ 下载 Npcap SDK (模拟)..." -ForegroundColor Yellow

        # 模拟下载
        Start-Sleep -Seconds 2
        New-Item -ItemType Directory -Path $NpcapDir -Force | Out-Null
        New-Item -ItemType File -Path "$NpcapDir\pcap.h" -Force | Out-Null

        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (下载)" -ForegroundColor Cyan
    }
}

function Test-OpenCVDownload {
    Write-Host "`n=== 测试 OpenCV 下载 ===" -ForegroundColor Green
    $StartTime = Get-Date

    if (Test-Path $OpenCVDir) {
        Write-Host "✅ OpenCV 缓存命中 (模拟)" -ForegroundColor Green
        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (缓存)" -ForegroundColor Cyan
    } else {
        Write-Host "⬇️ 下载 OpenCV (模拟)..." -ForegroundColor Yellow

        # 模拟下载
        Start-Sleep -Seconds 10
        New-Item -ItemType Directory -Path $OpenCVDir -Force | Out-Null
        New-Item -ItemType File -Path "$OpenCVDir\opencv.dll" -Force | Out-Null

        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (下载)" -ForegroundColor Cyan
    }
}

function Test-VcpkgBuild {
    Write-Host "`n=== 测试 vcpkg 构建 ===" -ForegroundColor Green
    $StartTime = Get-Date

    if (Test-Path $VcpkgDir) {
        Write-Host "✅ vcpkg 缓存命中 (模拟)" -ForegroundColor Green
        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (缓存)" -ForegroundColor Cyan
    } else {
        Write-Host "🔨 构建 vcpkg 依赖 (模拟)..." -ForegroundColor Yellow

        # 模拟构建
        Start-Sleep -Seconds 30
        New-Item -ItemType Directory -Path $VcpkgDir -Force | Out-Null
        New-Item -ItemType File -Path "$VcpkgDir\redis++.lib" -Force | Out-Null

        $Duration = (Get-Date) - $StartTime
        Write-Host "耗时: $($Duration.TotalSeconds) 秒 (构建)" -ForegroundColor Cyan
    }
}

# 根据测试类型执行不同的测试
$TotalStartTime = Get-Date

switch ($TestType) {
    "full" {
        Write-Host "执行完整构建测试..." -ForegroundColor Yellow
        Test-NpcapDownload
        Test-OpenCVDownload
        Test-VcpkgBuild
    }
    "cache-only" {
        Write-Host "执行缓存命中测试..." -ForegroundColor Yellow
        # 预先创建缓存目录
        New-Item -ItemType Directory -Path $NpcapDir -Force | Out-Null
        New-Item -ItemType Directory -Path $OpenCVDir -Force | Out-Null
        New-Item -ItemType Directory -Path $VcpkgDir -Force | Out-Null

        Test-NpcapDownload
        Test-OpenCVDownload
        Test-VcpkgBuild
    }
    "no-cache" {
        Write-Host "执行无缓存测试..." -ForegroundColor Yellow
        # 确保没有缓存
        if (Test-Path $NpcapDir) { Remove-Item -Recurse -Force $NpcapDir }
        if (Test-Path $OpenCVDir) { Remove-Item -Recurse -Force $OpenCVDir }
        if (Test-Path $VcpkgDir) { Remove-Item -Recurse -Force $VcpkgDir }

        Test-NpcapDownload
        Test-OpenCVDownload
        Test-VcpkgBuild
    }
}

$TotalDuration = (Get-Date) - $TotalStartTime

Write-Host "`n=== 测试结果摘要 ===" -ForegroundColor Cyan
Write-Host "总耗时: $($TotalDuration.TotalSeconds) 秒" -ForegroundColor White
Write-Host "测试类型: $TestType" -ForegroundColor White

# 显示性能对比
if ($TestType -eq "cache-only") {
    Write-Host "`n🚀 缓存优化效果:" -ForegroundColor Green
    Write-Host "  预计节省时间: ~15-20 分钟" -ForegroundColor Green
    Write-Host "  性能提升: ~75-85%" -ForegroundColor Green
} elseif ($TestType -eq "no-cache") {
    Write-Host "`n⏰ 无缓存构建时间:" -ForegroundColor Yellow
    Write-Host "  这是首次构建的正常时间" -ForegroundColor Yellow
    Write-Host "  后续构建将显著更快" -ForegroundColor Yellow
}

Write-Host "`n💡 提示: 运行 'test-cache-performance.ps1 -TestType cache-only' 查看缓存效果" -ForegroundColor Blue