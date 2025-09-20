# GitHub Actions 缓存故障排除手册

## 🚨 常见缓存问题诊断

这个文档提供了详细的缓存问题诊断和解决方案，帮助快速定位和修复缓存相关问题。

## 🔍 问题诊断流程图

```
缓存问题诊断
├── 缓存从未命中
│   ├── 检查缓存键语法
│   ├── 验证路径配置
│   └── 确认权限设置
├── 缓存偶尔命中
│   ├── 分析缓存键变化
│   ├── 检查文件变更模式
│   └── 优化restore-keys
├── 缓存命中但效果差
│   ├── 验证缓存内容完整性
│   ├── 检查文件权限
│   └── 分析恢复时间
└── 缓存大小异常
    ├── 检查包含的文件类型
    ├── 优化排除规则
    └── 分析存储效率
```

## 📋 问题分类和解决方案

### 1. 缓存键相关问题

#### 问题1.1: 缓存从未命中

**症状**:
```yaml
# 每次构建都显示
⬇️ 缓存未命中，重新下载/构建
```

**排查方法**:
```yaml
- name: Debug Cache Key
  run: |
    echo "当前缓存键: ${{ env.CACHE_KEY }}"
    echo "操作系统: ${{ runner.os }}"
    echo "文件哈希: ${{ hashFiles('**/package.json') }}"

    # 列出可能影响缓存键的文件
    find . -name "package.json" -type f
```

**常见原因和解决方案**:

| 原因 | 症状 | 解决方案 |
|------|------|----------|
| **缓存键包含易变值** | 每次都不同 | 移除时间戳、run_number等 |
| **文件路径错误** | hashFiles返回空 | 检查glob模式和文件存在性 |
| **分支隔离** | PR无法访问main缓存 | 调整restore-keys策略 |

```yaml
# ❌ 问题代码
key: cache-${{ github.run_number }}-${{ github.run_attempt }}

# ✅ 修正代码
key: ${{ runner.os }}-deps-${{ hashFiles('**/package-lock.json') }}-v1
restore-keys: |
  ${{ runner.os }}-deps-
```

#### 问题1.2: 缓存键冲突

**症状**:
```yaml
# 不同内容使用了相同的缓存键
WARNING: Cache key collision detected
```

**解决方案**:
```yaml
# 添加更多区分信息
key: ${{ runner.os }}-${{ matrix.node-version }}-deps-${{ hashFiles('package*.json') }}
```

### 2. 路径配置问题

#### 问题2.1: 路径不存在

**症状**:
```bash
Warning: Path does not exist: /path/to/cache
```

**排查脚本**:
```yaml
- name: Debug Cache Paths
  run: |
    echo "=== 缓存路径检查 ==="

    # 检查各个缓存路径
    paths=(
      "C:\vcpkg"
      "C:\opencv"
      "${{ github.workspace }}\npcap-sdk"
    )

    for path in "${paths[@]}"; do
      if [ -d "$path" ]; then
        echo "✅ 路径存在: $path"
        echo "   大小: $(du -sh "$path" 2>/dev/null || echo "无法计算")"
      else
        echo "❌ 路径不存在: $path"
      fi
    done
```

**解决方案**:
```yaml
# 创建缓存目录的条件检查
- name: Ensure Cache Directories
  run: |
    $paths = @("C:\vcpkg", "C:\opencv")
    foreach ($path in $paths) {
      if (!(Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force
        Write-Host "创建目录: $path"
      }
    }
```

#### 问题2.2: 权限问题

**症状**:
```bash
Error: Permission denied while accessing cache
```

**解决方案**:
```yaml
- name: Fix Cache Permissions
  run: |
    # Windows
    icacls "C:\cache-dir" /grant Everyone:F /T

    # Linux
    sudo chown -R $USER:$USER ~/.cache
    chmod -R 755 ~/.cache
```

### 3. 缓存内容问题

#### 问题3.1: 缓存损坏

**检测方法**:
```yaml
- name: Validate Cache Integrity
  run: |
    # 检查关键文件
    $criticalFiles = @(
      "C:\opencv\build\include\opencv2\opencv.hpp",
      "C:\vcpkg\installed\x64-windows\include\hiredis\hiredis.h"
    )

    $corrupted = $false
    foreach ($file in $criticalFiles) {
      if (!(Test-Path $file)) {
        Write-Host "❌ 关键文件缺失: $file" -ForegroundColor Red
        $corrupted = $true
      }
    }

    if ($corrupted) {
      Write-Host "🔧 检测到缓存损坏，将强制重建" -ForegroundColor Yellow
      # 设置标志位强制重建
      echo "FORCE_REBUILD=true" >> $env:GITHUB_ENV
    }
```

**修复策略**:
```yaml
# 方法1: 增加版本号强制重建
key: ${{ runner.os }}-opencv-${{ env.OPENCV_VERSION }}-v2  # v1 → v2

# 方法2: 条件清理
- name: Clean Corrupted Cache
  if: env.FORCE_REBUILD == 'true'
  run: |
    Remove-Item -Recurse -Force "C:\opencv" -ErrorAction SilentlyContinue
```

#### 问题3.2: 缓存不完整

**症状**:
```bash
# 缓存恢复了但缺少关键文件
FileNotFoundError: required file not found in cache
```

**检测脚本**:
```yaml
- name: Check Cache Completeness
  run: |
    # 定义期望的文件结构
    $expectedStructure = @{
      "C:\opencv" = @("build\include", "build\x64\vc16\lib", "build\x64\vc16\bin")
      "C:\vcpkg\installed\x64-windows" = @("include", "lib", "bin")
    }

    foreach ($baseDir in $expectedStructure.Keys) {
      if (Test-Path $baseDir) {
        foreach ($subDir in $expectedStructure[$baseDir]) {
          $fullPath = Join-Path $baseDir $subDir
          if (!(Test-Path $fullPath)) {
            Write-Host "⚠️ 缺少目录: $fullPath" -ForegroundColor Yellow
          }
        }
      }
    }
```

### 4. 性能问题

#### 问题4.1: 缓存恢复过慢

**排查方法**:
```yaml
- name: Measure Cache Restore Time
  run: |
    $startTime = Get-Date

    # 执行缓存恢复后的操作
    # ... 缓存相关步骤 ...

    $endTime = Get-Date
    $duration = ($endTime - $startTime).TotalSeconds

    Write-Host "缓存恢复耗时: $duration 秒"

    # 性能警告
    if ($duration -gt 60) {
      Write-Host "⚠️ 缓存恢复耗时过长，建议优化" -ForegroundColor Yellow
    }
```

**优化策略**:
```yaml
# 1. 减少缓存大小
path: |
  important-files/
  !**/*.log
  !**/tmp/
  !**/.cache/

# 2. 分层缓存
- name: Cache Base Dependencies
  uses: actions/cache@v4
  with:
    path: base-deps/
    key: base-${{ hashFiles('base-requirements.txt') }}

- name: Cache Application Cache
  uses: actions/cache@v4
  with:
    path: app-cache/
    key: app-${{ hashFiles('app-requirements.txt') }}
```

#### 问题4.2: 缓存大小过大

**分析工具**:
```yaml
- name: Analyze Cache Size
  run: |
    echo "=== 缓存大小分析 ==="

    # 分析各目录大小
    $cacheDirs = @("C:\vcpkg", "C:\opencv", "$env:GITHUB_WORKSPACE\npcap-sdk")

    foreach ($dir in $cacheDirs) {
      if (Test-Path $dir) {
        $size = (Get-ChildItem $dir -Recurse | Measure-Object -Property Length -Sum).Sum
        $sizeMB = [math]::Round($size / 1MB, 2)
        Write-Host "$dir : $sizeMB MB"

        # 分析最大的子目录
        Get-ChildItem $dir -Directory | ForEach-Object {
          $subSize = (Get-ChildItem $_.FullName -Recurse -ErrorAction SilentlyContinue |
                     Measure-Object -Property Length -Sum).Sum
          if ($subSize -gt 10MB) {
            $subSizeMB = [math]::Round($subSize / 1MB, 2)
            Write-Host "  └── $($_.Name): $subSizeMB MB"
          }
        }
      }
    }
```

**优化方案**:
```yaml
# 优化vcpkg缓存
path: |
  C:\vcpkg
  !C:\vcpkg\.git          # 排除git历史 (~200MB)
  !C:\vcpkg\buildtrees    # 排除构建临时文件 (~500MB)
  !C:\vcpkg\downloads     # 排除下载缓存 (~300MB)
  !C:\vcpkg\packages      # 排除中间包文件 (~200MB)

# 只保留安装的库
  C:\vcpkg\installed     # 只缓存最终安装的库 (~100MB)
```

### 5. 环境特定问题

#### 问题5.1: Windows vs Linux路径差异

**症状**:
```bash
# Windows上正常，Linux上失败
Error: path C:\cache not found on Linux
```

**解决方案**:
```yaml
# 使用条件路径
- name: Cache Dependencies
  uses: actions/cache@v4
  with:
    path: |
      ${{ runner.os == 'Windows' && 'C:\vcpkg' || '~/.vcpkg' }}
      ${{ runner.os == 'Windows' && 'C:\opencv' || '/usr/local/opencv' }}
    key: ${{ runner.os }}-deps-${{ hashFiles('**/CMakeLists.txt') }}
```

#### 问题5.2: 分支隔离问题

**症状**:
```bash
# PR中无法访问main分支的缓存
Cache not found for key: main-branch-cache
```

**解决策略**:
```yaml
# 设置合适的restore-keys
restore-keys: |
  ${{ runner.os }}-deps-${{ github.base_ref || 'main' }}-
  ${{ runner.os }}-deps-main-
  ${{ runner.os }}-deps-
```

## 🛠️ 调试工具和脚本

### 缓存调试脚本

```powershell
# debug-cache.ps1
param(
    [string]$Component = "all"  # all, npcap, opencv, vcpkg
)

function Test-CacheComponent {
    param($Name, $Path, $KeyFiles)

    Write-Host "`n=== 检查 $Name 缓存 ===" -ForegroundColor Cyan

    if (Test-Path $Path) {
        Write-Host "✅ 路径存在: $Path" -ForegroundColor Green

        # 检查关键文件
        foreach ($file in $KeyFiles) {
            $fullPath = Join-Path $Path $file
            if (Test-Path $fullPath) {
                Write-Host "✅ 关键文件存在: $file" -ForegroundColor Green
            } else {
                Write-Host "❌ 关键文件缺失: $file" -ForegroundColor Red
            }
        }

        # 计算大小
        $size = (Get-ChildItem $Path -Recurse -ErrorAction SilentlyContinue |
                Measure-Object -Property Length -Sum).Sum
        $sizeMB = [math]::Round($size / 1MB, 2)
        Write-Host "📊 缓存大小: $sizeMB MB" -ForegroundColor Yellow

    } else {
        Write-Host "❌ 路径不存在: $Path" -ForegroundColor Red
    }
}

# 检查各组件
if ($Component -eq "all" -or $Component -eq "npcap") {
    Test-CacheComponent "Npcap SDK" "$env:GITHUB_WORKSPACE\npcap-sdk" @("Include\pcap.h", "Lib\x64\wpcap.lib")
}

if ($Component -eq "all" -or $Component -eq "opencv") {
    Test-CacheComponent "OpenCV" "C:\opencv" @("build\include\opencv2\opencv.hpp", "build\x64\vc16\lib\opencv_world4100.lib")
}

if ($Component -eq "all" -or $Component -eq "vcpkg") {
    Test-CacheComponent "vcpkg" "C:\vcpkg" @("installed\x64-windows\include\hiredis\hiredis.h", "vcpkg.exe")
}
```

### 缓存清理脚本

```powershell
# clean-cache.ps1
param(
    [switch]$DryRun = $false,
    [string[]]$Components = @("temp", "logs", "downloads")
)

Write-Host "=== 缓存清理工具 ===" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host "🔍 模拟运行模式 (不会实际删除文件)" -ForegroundColor Yellow
}

$cleanupRules = @{
    "temp" = @{
        "paths" = @("C:\vcpkg\buildtrees", "C:\vcpkg\packages")
        "description" = "临时构建文件"
    }
    "logs" = @{
        "paths" = @("**\*.log", "**\logs\*")
        "description" = "日志文件"
    }
    "downloads" = @{
        "paths" = @("C:\vcpkg\downloads")
        "description" = "下载缓存"
    }
}

foreach ($component in $Components) {
    if ($cleanupRules.ContainsKey($component)) {
        $rule = $cleanupRules[$component]
        Write-Host "`n🧹 清理 $($rule.description)..." -ForegroundColor Green

        foreach ($path in $rule.paths) {
            if (Test-Path $path) {
                $sizeBefore = (Get-ChildItem $path -Recurse -ErrorAction SilentlyContinue |
                              Measure-Object -Property Length -Sum).Sum
                $sizeMB = [math]::Round($sizeBefore / 1MB, 2)

                Write-Host "  📁 $path ($sizeMB MB)" -ForegroundColor Gray

                if (!$DryRun) {
                    Remove-Item -Path $path -Recurse -Force -ErrorAction SilentlyContinue
                    Write-Host "  ✅ 已清理" -ForegroundColor Green
                }
            }
        }
    }
}
```

## 📈 缓存监控和报警

### 性能监控脚本

```yaml
- name: Cache Performance Monitor
  run: |
    # 创建性能报告
    $report = @{
      timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
      cache_hits = @{
        npcap = "${{ steps.npcap-cache.outputs.cache-hit }}"
        opencv = "${{ steps.opencv-cache.outputs.cache-hit }}"
        vcpkg = "${{ steps.vcpkg-cache.outputs.cache-hit }}"
      }
      performance = @{
        total_time = "${{ env.TOTAL_BUILD_TIME }}"
        cache_restore_time = "${{ env.CACHE_RESTORE_TIME }}"
      }
    }

    # 转换为JSON并输出
    $report | ConvertTo-Json -Depth 3 | Out-File "cache-performance.json"

    # 计算缓存效率
    $hitCount = ($report.cache_hits.Values | Where-Object { $_ -eq "true" }).Count
    $totalCount = $report.cache_hits.Count
    $hitRate = [math]::Round(($hitCount / $totalCount) * 100, 2)

    Write-Host "📊 缓存命中率: $hitRate%" -ForegroundColor Cyan

    # 性能警告
    if ($hitRate -lt 80) {
      Write-Host "⚠️ 缓存命中率过低，建议检查缓存策略" -ForegroundColor Yellow
    }
```

---

> 💡 **故障排除建议**: 遇到缓存问题时，建议按照诊断流程逐步排查，使用提供的调试脚本快速定位问题。大多数缓存问题都可以通过调整缓存键、优化路径配置或增加验证机制来解决。