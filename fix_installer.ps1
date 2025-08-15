# Fix Installer Data Directory Script
Write-Host "=== 修复安装包数据目录 ===" -ForegroundColor Cyan

$BuildDir = "build1"
$DataDir = "$BuildDir\project_installer\packages\com.leletools\data"
$SourceDir = $BuildDir

# 确保数据目录存在
if (-Not (Test-Path $DataDir)) {
    New-Item -ItemType Directory -Force -Path $DataDir | Out-Null
    Write-Host "创建数据目录: $DataDir" -ForegroundColor Green
} else {
    # 清空现有内容
    Remove-Item -Path "$DataDir\*" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "清空数据目录: $DataDir" -ForegroundColor Yellow
}

Write-Host "开始复制文件..." -ForegroundColor Cyan

# 复制主程序
$ExeFiles = Get-ChildItem -Path $SourceDir -Filter "*.exe" | Where-Object { $_.Name -notlike "*Installer*" }
foreach ($exe in $ExeFiles) {
    Copy-Item -Path $exe.FullName -Destination $DataDir -Force
    Write-Host "✓ 复制: $($exe.Name)" -ForegroundColor Green
}

# 复制DLL文件
$DllFiles = Get-ChildItem -Path $SourceDir -Filter "*.dll"
foreach ($dll in $DllFiles) {
    Copy-Item -Path $dll.FullName -Destination $DataDir -Force
    Write-Host "✓ 复制: $($dll.Name)" -ForegroundColor Green
}

# 复制Qt插件目录
$QtDirs = @("platforms", "imageformats", "styles", "translations", "iconengines", "generic", "networkinformation", "tls")
foreach ($qtDir in $QtDirs) {
    $SourcePath = Join-Path $SourceDir $qtDir
    if (Test-Path $SourcePath) {
        $DestPath = Join-Path $DataDir $qtDir
        Copy-Item -Path $SourcePath -Destination $DestPath -Recurse -Force
        Write-Host "✓ 复制目录: $qtDir" -ForegroundColor Green
    }
}

# 验证复制结果
$CopiedFiles = Get-ChildItem -Path $DataDir -Recurse
Write-Host "复制完成！共 $($CopiedFiles.Count) 个文件/目录" -ForegroundColor Green

Write-Host "=== 修复完成 ===" -ForegroundColor Cyan