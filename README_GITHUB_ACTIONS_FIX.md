# GitHub Actions 工作流修复

## 问题描述

GitHub Actions 构建失败，出现以下错误：

1. **ID冲突错误**：
   ```
   The identifier 'vcpkg-cache' may not be used more than once within the same scope.
   ```

2. **缓存路径错误**：
   ```
   Error: Path Validation Error: At least one directory or file path is required
   ```

## 根本原因分析

### 1. ID 重复使用
- `vcpkg-cache` 在同一个作业中被使用了两次（第822行和第1022行）
- GitHub Actions 要求每个步骤的 ID 在同一作业范围内必须唯一

### 2. 环境变量未定义
- 缓存步骤使用了 `${{ env.VCPKG_INSTALLED_DIR }}` 和 `${{ env.VCPKG_DOWNLOADS }}`
- 这些环境变量在后续步骤中才定义，导致缓存路径为空

## 修复方案

### ✅ 1. 修复重复ID问题

**修复前：**
```yaml
# LibSSH 缓存
- name: Cache vcpkg
  id: vcpkg-cache     # ❌ 重复

# Redis 缓存
- name: Cache vcpkg
  id: vcpkg-cache     # ❌ 重复
```

**修复后：**
```yaml
# LibSSH 缓存
- name: Cache vcpkg LibSSH
  id: vcpkg-cache     # ✅ 唯一

# Redis 缓存
- name: Cache vcpkg
  id: vcpkg-redis-cache  # ✅ 唯一
```

### ✅ 2. 修复缓存路径问题

**修复前：**
```yaml
path: |
  ${{ env.VCPKG_INSTALLED_DIR }}  # ❌ 未定义的环境变量
  ${{ env.VCPKG_DOWNLOADS }}      # ❌ 未定义的环境变量
```

**修复后：**
```yaml
path: |
  C:\vcpkg\installed\x64-windows  # ✅ 明确的硬编码路径
  C:\vcpkg\downloads               # ✅ 明确的硬编码路径
```

### ✅ 3. 更新相关引用

更新所有引用新 ID 的地方：

```yaml
# 步骤条件判断
if: steps.vcpkg-redis-cache.outputs.cache-hit != 'true'

# PowerShell 脚本中的引用
$vcpkgCacheHit = "${{ steps.vcpkg-redis-cache.outputs.cache-hit }}"
```

## 修复结果

### 🎯 解决的问题
- ✅ 消除了 ID 冲突错误
- ✅ 修复了缓存路径为空的问题
- ✅ 保持了两个不同 vcpkg 缓存的功能独立性

### 🔧 技术改进
- **明确的路径**：使用硬编码路径避免环境变量依赖
- **唯一标识**：为不同用途的缓存使用不同的 ID
- **向后兼容**：保持现有构建逻辑不变

### 📊 缓存策略
1. **LibSSH 缓存**（`vcpkg-cache`）
   - 缓存 LibSSH 相关的 vcpkg 包
   - 用于 SSH 客户端功能

2. **Redis 缓存**（`vcpkg-redis-cache`）
   - 缓存 Redis-plus-plus 相关的 vcpkg 包
   - 用于数据库连接功能

## 验证步骤

修复后的工作流应该能够：

1. ✅ 通过 GitHub Actions 语法验证
2. ✅ 正确缓存和恢复 vcpkg 包
3. ✅ 成功构建 Windows 版本
4. ✅ 避免重复下载依赖项

## 提交信息

```
fix: resolve vcpkg cache path issues in GitHub Actions workflow

- Fix empty cache path error by using hardcoded paths instead of undefined env vars
- Rename duplicate 'vcpkg-cache' ID to 'vcpkg-redis-cache' to avoid conflicts
- Update all references to use the correct cache IDs
```

这个修复确保了 GitHub Actions 工作流能够稳定可靠地构建项目！