# 更新翻译文件脚本
# 此脚本用于扫描源代码中的tr()函数并更新.ts文件

Write-Host "正在更新翻译文件..." -ForegroundColor Green

# 检查Qt工具是否可用
$lupdate = Get-Command lupdate -ErrorAction SilentlyContinue
if (-not $lupdate) {
    Write-Host "错误: 找不到lupdate工具。请确保Qt开发环境已正确安装并添加到PATH中。" -ForegroundColor Red
    exit 1
}

# 定义项目根目录
$projectRoot = Split-Path $PSScriptRoot -Parent
$translationsDir = Join-Path $projectRoot "translations"

Write-Host "项目根目录: $projectRoot" -ForegroundColor Yellow
Write-Host "翻译文件目录: $translationsDir" -ForegroundColor Yellow

# 确保翻译目录存在
if (-not (Test-Path $translationsDir)) {
    New-Item -ItemType Directory -Path $translationsDir -Force
    Write-Host "创建翻译目录: $translationsDir" -ForegroundColor Cyan
}

# 定义要扫描的源文件目录
$sourceDirs = @(
    "main-widget",
    "modules", 
    "common",
    "tool-list"
)

# 定义翻译文件
$tsFiles = @(
    "translations/lele-tools_zh_CN.ts",
    "translations/lele-tools_en_US.ts"
)

# 构建lupdate命令参数
$sourceArgs = @()
foreach ($dir in $sourceDirs) {
    $fullPath = Join-Path $projectRoot $dir
    if (Test-Path $fullPath) {
        $sourceArgs += $fullPath
        Write-Host "包含源目录: $dir" -ForegroundColor Cyan
    } else {
        Write-Host "跳过不存在的目录: $dir" -ForegroundColor Yellow
    }
}

# 添加主源文件
$mainCpp = Join-Path $projectRoot "main.cpp"
if (Test-Path $mainCpp) {
    $sourceArgs += $mainCpp
    Write-Host "包含主源文件: main.cpp" -ForegroundColor Cyan
}

# 执行lupdate命令更新每个翻译文件
foreach ($tsFile in $tsFiles) {
    $tsPath = Join-Path $projectRoot $tsFile
    Write-Host "`n正在更新: $tsFile" -ForegroundColor Green
    
    # 构建完整的lupdate命令
    $lupdateArgs = $sourceArgs + @("-ts", $tsPath)
    
    & lupdate @lupdateArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ 成功更新: $tsFile" -ForegroundColor Green
    } else {
        Write-Host "  ✗ 更新失败: $tsFile" -ForegroundColor Red
    }
}

Write-Host "`n翻译文件更新完成！" -ForegroundColor Green
Write-Host "提示: 请使用Qt Linguist编辑翻译文件，然后运行generate_translations.ps1生成.qm文件。" -ForegroundColor Cyan
