# SSH 客户端工具

这是一个基于 LibSSH 的 SSH 连接和 SFTP 文件管理工具，提供类似于 PuTTY/WinSCP 的功能。

## 功能特性

### 🔌 SSH 连接管理
- **连接配置**: 保存和管理多个 SSH 连接配置
- **认证方式**: 支持密码认证和私钥认证
- **连接测试**: 在保存前测试连接的有效性
- **连接历史**: 自动保存连接历史和配置

### 💻 SSH 终端
- **交互式终端**: 提供完整的 SSH 终端体验
- **命令历史**: 支持命令历史记录和导航（上/下箭头）
- **彩色输出**: 支持终端颜色和格式
- **会话管理**: 自动维护 SSH 会话状态

### 📁 SFTP 文件管理
- **双面板设计**: 左侧本地文件，右侧远程文件
- **文件操作**: 上传、下载、删除、新建目录
- **目录导航**: 双击进入目录，支持返回上级
- **文件信息**: 显示文件大小、修改时间、权限
- **批量操作**: 支持多文件选择和批量传输

## 使用方法

### 1. 创建 SSH 连接

1. 点击左侧面板的 **"新建"** 按钮
2. 填写连接信息：
   - **连接名称**: 给连接起一个易识别的名称
   - **主机名**: SSH 服务器的 IP 地址或域名
   - **端口**: SSH 服务端口（默认 22）
   - **用户名**: SSH 登录用户名
   - **认证方式**: 选择密码认证或私钥认证
   - **密码/私钥**: 根据认证方式填写相应信息
3. 点击 **"测试连接"** 验证配置
4. 点击 **"确定"** 保存连接

### 2. 建立 SSH 连接

- **方式一**: 双击左侧连接列表中的连接
- **方式二**: 选中连接后点击 **"连接"** 按钮
- **方式三**: 右键点击连接选择 **"连接"**

### 3. 使用 SSH 终端

连接成功后，可以在 **"SSH终端"** 标签页中：

- 输入命令并按回车执行
- 使用上/下箭头键浏览命令历史
- 查看彩色的命令输出
- 执行各种 Linux/Unix 命令

### 4. 使用 SFTP 文件管理

在 **"SFTP文件管理"** 标签页中：

#### 导航操作
- **双击目录**: 进入子目录
- **双击 ".."**: 返回上级目录
- **刷新**: 点击刷新按钮更新文件列表

#### 文件操作
- **上传文件**: 选择本地文件，点击 **"上传 →"**
- **下载文件**: 选择远程文件，点击 **"← 下载"**
- **删除文件**: 选择远程文件，点击 **"删除"**
- **新建目录**: 点击 **"新建目录"** 并输入目录名

## 界面布局

```
┌─────────────────────────────────────────────────────────────┐
│ 连接(C)  帮助(H)                                               │
├─────────────┬───────────────────────────────────────────────┤
│  SSH连接    │  ┌─ SSH终端 ──┬─ SFTP文件管理 ─┐                │
│             │  │            │                │                │
│ ┌─────────┐ │  │ 终端输出区  │  本地 │按钮│远程  │                │
│ │ 连接1   │ │  │            │      │    │      │                │
│ │ 连接2   │ │  │            │ 文件  │操作│文件   │                │
│ │ 连接3   │ │  │            │ 列表  │按钮│列表   │                │
│ └─────────┘ │  │            │      │    │      │                │
│             │  │ 命令输入框  │      │    │      │                │
│ [新建][编辑] │  └────────────┴──────┴────┴──────┘                │
│ [删除][连接] │                                                  │
├─────────────┴───────────────────────────────────────────────┤
│ 状态: 已连接到 user@example.com                               │
└─────────────────────────────────────────────────────────────┘
```

## 技术实现

### 依赖库
- **LibSSH**: SSH 协议实现和 SFTP 支持
- **Qt6**: GUI 框架和用户界面
- **C++20**: 现代 C++ 语言特性

### 核心组件

1. **SSHClient**: 主界面控制器
2. **ConnectionManager**: 连接配置管理
3. **SSHConnection**: SSH 连接封装
4. **SSHTerminal**: 交互式终端组件
5. **SFTPBrowser**: 文件管理界面
6. **SSHConnectionDialog**: SSH连接配置对话框

### 安全考虑

- 私钥文件支持密码保护
- 连接配置本地加密存储（计划中）
- SSL/TLS 传输加密
- 主机密钥验证（计划中）

## 编译要求

### 必需依赖
- **LibSSH 0.10.0+**: SSH 客户端库
- **Qt 6.5+**: GUI 框架
- **CMake 3.16+**: 构建系统
- **C++20 编译器**: GCC 10+/Clang 12+/MSVC 2019+

### 编译选项
```cmake
# 启用 LibSSH 支持
-DWITH_LIBSSH=ON

# 指定 LibSSH 安装路径（如果需要）
-DLibSSH_ROOT_DIR=/path/to/libssh
```

### Windows 下安装 LibSSH

#### 方法 1: 使用 vcpkg
```bash
vcpkg install libssh
```

#### 方法 2: 手动编译
1. 下载 LibSSH 源码
2. 使用 CMake 编译
3. 设置 `LibSSH_ROOT_DIR` 环境变量

#### 方法 3: 预编译包
从 https://www.libssh.org/ 下载预编译包

### Linux 下安装 LibSSH

```bash
# Ubuntu/Debian
sudo apt-get install libssh-dev

# CentOS/RHEL
sudo yum install libssh-devel

# Fedora
sudo dnf install libssh-devel

# Arch Linux
sudo pacman -S libssh
```

## 使用示例

### 连接到服务器
```cpp
SSHConnectionInfo info;
info.name = "我的服务器";
info.hostname = "example.com";
info.port = 22;
info.username = "user";
info.password = "password";

// 或使用私钥
info.usePrivateKey = true;
info.privateKeyPath = "/path/to/private.key";
```

### 执行命令
```cpp
connection->executeCommand("ls -la");
connection->executeCommand("cat /etc/hosts");
connection->executeCommand("ps aux | grep nginx");
```

### SFTP 操作
```cpp
// 上传文件
sftpBrowser->uploadFile("/local/path/file.txt", "/remote/path/file.txt");

// 下载文件
sftpBrowser->downloadFile("/remote/path/file.txt", "/local/path/file.txt");

// 创建目录
sftpBrowser->createRemoteDirectory("/remote/path", "new_directory");
```

## 扩展功能

该工具设计时考虑了扩展性，可以轻松添加：

- **端口转发**: SSH 隧道和端口转发
- **批量操作**: 多服务器批量执行命令
- **脚本支持**: 自动化脚本执行
- **会话录制**: 终端会话记录和回放
- **主题支持**: 自定义终端主题和配色

## 故障排除

### 常见问题

1. **连接失败**
   - 检查主机名、端口是否正确
   - 确认防火墙设置
   - 验证用户名和密码

2. **认证失败**
   - 检查用户名和密码
   - 验证私钥文件路径和格式
   - 确认私钥密码（如果有）

3. **LibSSH 编译错误**
   - 确认 LibSSH 已正确安装
   - 检查 CMake 配置
   - 设置正确的库路径

### 调试模式

编译时启用调试模式获取详细日志：
```cmake
-DCMAKE_BUILD_TYPE=Debug
```

## 许可证

本工具基于项目主许可证发布。LibSSH 库遵循 LGPL 许可证。