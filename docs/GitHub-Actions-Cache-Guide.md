# GitHub Actions 缓存优化指南

这个文档说明了我们如何优化GitHub Actions构建流程的缓存策略，从而显著减少构建时间。

## 🚀 缓存优化效果

### 时间节省估算
- **首次构建**: ~25-30分钟
- **缓存命中后**: ~5-8分钟
- **节省时间**: ~15-25分钟 (60-80%提升)

### 具体缓存项目

| 组件 | 原时间 | 缓存后 | 节省 | 缓存大小 |
|------|--------|--------|------|----------|
| **Npcap SDK** | ~30秒 | ~5秒 | 25秒 | ~1MB |
| **OpenCV** | ~2-5分钟 | ~10秒 | 2-5分钟 | ~150MB |
| **vcpkg依赖** | ~10-15分钟 | ~30秒 | 10-15分钟 | ~200MB |
| **Qt框架** | ~3-5分钟 | ~30秒 | 3-5分钟 | ~500MB |

## 🔧 缓存策略详解

### 1. Npcap SDK 缓存
```yaml
- name: Cache Npcap SDK
  uses: actions/cache@v4
  with:
    path: ${{ github.workspace }}/npcap-sdk
    key: ${{ runner.os }}-npcap-sdk-1.15-v1
    restore-keys: |
      ${{ runner.os }}-npcap-sdk-1.15
      ${{ runner.os }}-npcap-sdk
```

**特点**:
- 版本锁定缓存 (1.15)
- 首次缓存后永久有效
- 自动验证关键文件存在

### 2. OpenCV 缓存
```yaml
- name: Cache OpenCV
  uses: actions/cache@v4
  with:
    path: C:\opencv
    key: ${{ runner.os }}-opencv-${{ env.OPENCV_VERSION }}-v1
```

**特点**:
- 基于版本号的缓存键
- 包含完整的OpenCV安装
- 自动配置环境变量

### 3. vcpkg 依赖缓存
```yaml
- name: Cache vcpkg
  uses: actions/cache@v4
  with:
    path: |
      C:\vcpkg
      !C:\vcpkg\.git
      !C:\vcpkg\buildtrees
      !C:\vcpkg\downloads
    key: ${{ runner.os }}-vcpkg-redis-mysql-v2-${{ hashFiles('**/vcpkg.json', '**/CMakeLists.txt') }}
```

**特点**:
- 排除临时文件减少缓存大小
- 基于文件哈希的智能失效
- 支持增量更新

### 4. Qt 框架缓存
```yaml
- name: Setup Qt
  uses: jurplel/install-qt-action@v3
  with:
    cache: true  # 内置缓存支持
```

**特点**:
- Action内置缓存机制
- 自动版本管理
- 跨平台兼容

## 📊 缓存键策略

### 缓存键设计原则
1. **OS标识**: `${{ runner.os }}` - 区分不同操作系统
2. **版本标识**: `${{ env.VERSION }}` - 版本变更时失效
3. **内容哈希**: `${{ hashFiles(...) }}` - 依赖变更时失效
4. **手动版本**: `-v1`, `-v2` - 强制失效控制

### 缓存失效策略
- **版本更新**: 主要依赖版本变更时自动失效
- **配置变更**: CMakeLists.txt等配置文件变更时失效
- **手动失效**: 通过修改版本号强制失效

## 🛠️ 缓存管理

### 查看缓存状态
每次构建会输出详细的缓存状态报告：
```
=== 缓存状态报告 ===
✅ Npcap SDK: 缓存命中
✅ OpenCV: 缓存命中
✅ vcpkg: 缓存命中

=== 预估下载时间节省 ===
Npcap SDK: 节省 ~30 秒
OpenCV: 节省 ~2-5 分钟
vcpkg依赖: 节省 ~10-15 分钟
```

### 缓存清理
GitHub Actions会自动管理缓存：
- 7天未访问的缓存会被删除
- 总缓存大小限制: 10GB (免费账户)
- 可通过GitHub界面手动清理

### 强制重建缓存
如需强制重建某个缓存：

1. **Npcap SDK**: 修改 `npcap-sdk-1.15-v1` 为 `npcap-sdk-1.15-v2`
2. **OpenCV**: 修改 `opencv-4.10.0-v1` 为 `opencv-4.10.0-v2`
3. **vcpkg**: 修改 `vcpkg-redis-mysql-v2` 为 `vcpkg-redis-mysql-v3`

## 🔍 故障排除

### 缓存未命中
1. 检查缓存键是否正确
2. 确认版本号是否匹配
3. 查看依赖文件是否变更

### 缓存损坏
1. 增加缓存版本号强制重建
2. 添加验证步骤检查缓存内容
3. 使用 `restore-keys` 提供备选方案

### 构建仍然缓慢
1. 检查缓存命中情况
2. 识别未缓存的耗时步骤
3. 考虑添加新的缓存项

## 📈 性能监控

### 关键指标
- 缓存命中率
- 总构建时间
- 各步骤耗时
- 缓存大小

### 优化建议
1. 定期更新缓存策略
2. 监控缓存效率
3. 平衡缓存大小和速度
4. 使用合适的缓存键粒度

---

> 💡 **提示**: 这个缓存策略可以减少60-80%的构建时间，特别是在频繁构建时效果明显！