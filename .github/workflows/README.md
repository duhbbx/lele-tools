# GitHub Actions 工作流说明

本项目包含两个GitHub Actions工作流，用于自动化构建和发布Lele Tools。

## 🚀 工作流概览

### 1. `build-and-release.yml` - 主要构建和发布流程

**触发条件:**
- 推送到 `main` 或 `master` 分支
- 创建以 `v` 开头的标签 (如 `v1.0.0`)
- 手动触发 (workflow_dispatch)
- Pull Request 到主分支

**功能:**
- ✅ 自动构建Windows版本
- ✅ 部署Qt依赖
- ✅ 创建安装包
- ✅ 上传构建产物
- ✅ 自动发布Release (仅限标签推送)

### 2. `manual-build.yml` - 手动构建流程

**触发条件:**
- 仅手动触发

**功能:**
- ✅ 可配置版本号和构建类型
- ✅ 选择是否创建安装包
- ✅ 选择是否上传产物
- ✅ 快速测试构建

## 📋 使用方法

### 自动发布新版本

1. **创建版本标签:**
   ```bash
   git tag v1.2.0
   git push origin v1.2.0
   ```

2. **工作流自动执行:**
   - 构建项目
   - 创建安装包
   - 发布GitHub Release

### 手动构建测试

1. **进入GitHub仓库的Actions页面**

2. **选择 "Manual Build" 工作流**

3. **点击 "Run workflow" 并配置参数:**
   - **Version**: 版本号 (如 `1.0.0`)
   - **Build Type**: `Release` 或 `Debug`
   - **Create installer**: 是否创建安装包
   - **Upload artifacts**: 是否上传构建产物

4. **点击 "Run workflow" 开始构建**

## ⚙️ 配置说明

### 环境变量

```yaml
env:
  QT_VERSION: '6.9.1'          # Qt版本
  CMAKE_VERSION: '3.28'        # CMake版本
  MSVC_VERSION: '2022'         # Visual Studio版本
```

### 构建产物

**自动上传的文件:**
- `lele-tools-build-{version}` - 编译后的可执行文件和DLL
- `lele-tools-installer-{version}` - Windows安装包
- `LeleToolsInstaller_v{version}.exe` - 最终安装包

### Release自动化

当推送标签时，工作流会自动创建GitHub Release，包含:

- 📦 Windows安装包
- 📝 版本说明
- 🔧 安装和系统要求说明
- 📋 构建信息

## 🔧 本地开发

如果需要在本地复制GitHub Actions的构建过程:

```powershell
# 1. 确保安装了必要工具
# - Qt 6.9.1 with MSVC 2022
# - CMake 3.28+
# - Visual Studio 2022
# - Qt Installer Framework

# 2. 运行构建脚本
.\script\build.ps1 -Version "1.0.0" -BuildType "Release"
```

## 🐛 故障排除

### 常见问题

1. **Qt路径问题**
   - 确保Qt安装在标准路径
   - 检查Qt版本是否匹配

2. **CMake配置失败**
   - 检查CMakeLists.txt语法
   - 确保所有依赖都已安装

3. **安装包创建失败**
   - 检查Qt Installer Framework是否正确安装
   - 验证配置文件XML语法

### 调试方法

1. **查看构建日志:**
   - 进入Actions页面
   - 点击失败的工作流
   - 展开各个步骤查看详细日志

2. **本地测试:**
   - 在本地运行相同的构建命令
   - 使用相同的Qt和工具版本

3. **手动构建:**
   - 使用Manual Build工作流进行测试
   - 逐步启用不同选项排查问题

## 📚 相关文档

- [Qt Installer Framework 文档](https://doc.qt.io/qtinstallerframework/)
- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [CMake 文档](https://cmake.org/documentation/)

## 🤝 贡献

如果需要修改工作流配置:

1. 编辑 `.github/workflows/` 目录下的YAML文件
2. 测试修改后的工作流
3. 提交Pull Request并说明修改原因



