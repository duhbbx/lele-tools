# 🌐 一键式国际化脚本
# 🎯 这个脚本会自动处理所有国际化相关的工作

param(
    [string]$Action = "update"
)

Write-Host "🚀 乐乐工具箱 - 一键式国际化工具" -ForegroundColor Green
Write-Host "=" * 50 -ForegroundColor Gray

switch ($Action.ToLower()) {
    "update" {
        Write-Host "📝 正在扫描源码并更新翻译文件..." -ForegroundColor Cyan
        & "$PSScriptRoot\update_translations.ps1"
        
        Write-Host "`n✅ 翻译文件已更新！" -ForegroundColor Green
        Write-Host "💡 接下来您可以：" -ForegroundColor Yellow
        Write-Host "   1. 使用Qt Linguist编辑翻译：" -ForegroundColor White
        Write-Host "      C:\Qt\6.9.1\msvc2022_64\bin\linguist.exe translations\lele-tools_zh_CN.ts" -ForegroundColor Gray
        Write-Host "   2. 或直接编辑 translations\lele-tools_*.ts 文件" -ForegroundColor White
        Write-Host "   3. 然后运行：.\script\easy_i18n.ps1 build" -ForegroundColor White
    }
    
    "build" {
        Write-Host "🔨 正在编译翻译文件..." -ForegroundColor Cyan
        & "$PSScriptRoot\generate_translations.ps1"
        
        Write-Host "`n✅ 翻译文件编译完成！" -ForegroundColor Green
        Write-Host "🎉 现在可以构建项目了！" -ForegroundColor Yellow
    }
    
    "all" {
        Write-Host "🔄 执行完整的国际化流程..." -ForegroundColor Cyan
        
        Write-Host "`n📝 步骤1：更新翻译文件" -ForegroundColor Yellow
        & "$PSScriptRoot\update_translations.ps1"
        
        Write-Host "`n🔨 步骤2：编译翻译文件" -ForegroundColor Yellow  
        & "$PSScriptRoot\generate_translations.ps1"
        
        Write-Host "`n🎉 完成！所有国际化文件已准备就绪！" -ForegroundColor Green
    }
    
    "help" {
        Write-Host "📖 使用说明：" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "  .\script\easy_i18n.ps1 update  - 扫描源码，更新翻译文件" -ForegroundColor White
        Write-Host "  .\script\easy_i18n.ps1 build   - 编译翻译文件为.qm格式" -ForegroundColor White  
        Write-Host "  .\script\easy_i18n.ps1 all     - 执行完整流程（更新+编译）" -ForegroundColor White
        Write-Host "  .\script\easy_i18n.ps1 help    - 显示此帮助信息" -ForegroundColor White
        Write-Host ""
        Write-Host "💡 日常开发流程：" -ForegroundColor Cyan
        Write-Host "  1. 在源码中添加 tr(\"新文本\")" -ForegroundColor Gray
        Write-Host "  2. 运行：.\script\easy_i18n.ps1 update" -ForegroundColor Gray
        Write-Host "  3. 编辑翻译文件（使用Qt Linguist或直接编辑.ts文件）" -ForegroundColor Gray
        Write-Host "  4. 运行：.\script\easy_i18n.ps1 build" -ForegroundColor Gray
        Write-Host "  5. 构建项目" -ForegroundColor Gray
    }
    
    default {
        Write-Host "❌ 未知参数：$Action" -ForegroundColor Red
        Write-Host "💡 运行 .\script\easy_i18n.ps1 help 查看使用说明" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "🎯 记住：您只需要在源码中写 tr(\"文本\")，所有转义都会自动处理！" -ForegroundColor Green
