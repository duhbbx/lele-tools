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

# 显示数据目录内容
Write-Host "数据目录内容:" -ForegroundColor Yellow
Get-ChildItem -Path $DataDir | ForEach-Object {
    if ($_.PSIsContainer) {
        Write-Host "  📁 $($_.Name)/" -ForegroundColor Cyan
    } else {
        $sizeKB = [math]::Round($_.Length / 1KB, 1)
        Write-Host "  📄 $($_.Name) ($sizeKB KB)" -ForegroundColor Gray
    }
}

# 重新生成安装包
$IFWPath = "C:\Qt\Tools\QtInstallerFramework\4.10\bin"
$BinaryCreator = Join-Path $IFWPath "binarycreator.exe"

if (Test-Path $BinaryCreator) {
    Write-Host "重新生成安装包..." -ForegroundColor Cyan
    
    $ConfigPath = "$BuildDir\project_installer\config\config.xml"
    $PackagesPath = "$BuildDir\project_installer\packages"
    $InstallerOut = "$BuildDir\LeleToolsInstaller_Fixed.exe"
    
    try {
        & $BinaryCreator --offline-only -c $ConfigPath -p $PackagesPath $InstallerOut
        
        if ($LASTEXITCODE -eq 0 -and (Test-Path $InstallerOut)) {
            $InstallerSize = [math]::Round((Get-Item $InstallerOut).Length / 1MB, 2)
            Write-Host "✅ 安装包重新生成成功！" -ForegroundColor Green
            Write-Host "📦 文件: $InstallerOut" -ForegroundColor Green
            Write-Host "📏 大小: $InstallerSize MB" -ForegroundColor Green
            
            # 复制到项目根目录
            Copy-Item -Path $InstallerOut -Destination "LeleToolsInstaller_Fixed.exe" -Force
            Write-Host "✅ 已复制到项目根目录: LeleToolsInstaller_Fixed.exe" -ForegroundColor Green
        } else {
            Write-Host "❌ 安装包生成失败！" -ForegroundColor Red
        }
    }
    catch {
        Write-Host "❌ 生成安装包时出错: $($_.Exception.Message)" -ForegroundColor Red
    }
} else {
    Write-Host "⚠️  未找到 binarycreator.exe，跳过重新生成" -ForegroundColor Yellow
    Write-Host "💡 数据目录已修复，可以手动运行 binarycreator" -ForegroundColor Yellow
}

Write-Host "=== 修复完成 ===" -ForegroundColor Cyan