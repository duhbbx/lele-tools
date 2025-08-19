# 生成翻译文件脚本
# 此脚本用于将.ts文件编译为.qm文件

Write-Host "正在生成翻译文件..." -ForegroundColor Green

# 检查Qt工具是否可用
$qtPath = "C:\Qt\6.9.1\msvc2022_64\bin"
$lrelease = Join-Path $qtPath "lrelease.exe"

if (-not (Test-Path $lrelease)) {
    # 尝试从PATH中查找
    $lrelease = Get-Command lrelease -ErrorAction SilentlyContinue
    if (-not $lrelease) {
        Write-Host "错误: 找不到lrelease工具。请确保Qt开发环境已正确安装。" -ForegroundColor Red
        Write-Host "尝试查找的路径: $qtPath" -ForegroundColor Yellow
        exit 1
    }
    $lrelease = $lrelease.Source
}

# 定义源目录和目标目录
$sourceDir = Join-Path $PSScriptRoot "..\translations"
$tsFiles = @("lele-tools_zh_CN.ts", "lele-tools_en_US.ts")

Write-Host "源目录: $sourceDir" -ForegroundColor Yellow

# 检查源目录是否存在
if (-not (Test-Path $sourceDir)) {
    Write-Host "错误: 翻译文件目录不存在: $sourceDir" -ForegroundColor Red
    exit 1
}

# 处理每个.ts文件
foreach ($tsFile in $tsFiles) {
    $tsPath = Join-Path $sourceDir $tsFile
    $qmFile = $tsFile -replace "\.ts$", ".qm"
    $qmPath = Join-Path $sourceDir $qmFile
    
    if (Test-Path $tsPath) {
        Write-Host "正在处理: $tsFile" -ForegroundColor Cyan
        
        # 使用lrelease编译.ts文件为.qm文件
        & lrelease $tsPath -qm $qmPath
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✓ 成功生成: $qmFile" -ForegroundColor Green
        } else {
            Write-Host "  ✗ 生成失败: $qmFile" -ForegroundColor Red
        }
    } else {
        Write-Host "  ⚠ 文件不存在: $tsFile" -ForegroundColor Yellow
    }
}

Write-Host "`nTranslation files generated successfully!" -ForegroundColor Green
Write-Host "Tip: To update translation files, use lupdate tool to scan tr() functions in source code." -ForegroundColor Cyan
