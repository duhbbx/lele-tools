# Lele Tools - 乐乐的工具箱

一款集成了 **59+ 实用工具**的跨平台桌面应用，基于 Qt6/C++ 构建，支持 Windows、macOS、Linux（Ubuntu/Debian/Fedora）。

> 乐乐是我可可爱爱的儿子，这个工具箱以他命名。

## 下载

前往 [Releases](https://github.com/duhbbx/lele-tools/releases) 下载最新版本：

| 平台 | 格式 | 说明 |
|------|------|------|
| Windows | `.zip` | 解压即用 |
| macOS | `.dmg` | 拖入 Applications 安装 |
| Ubuntu | `.deb` | `sudo dpkg -i xxx.deb` |
| Debian | `.deb` | `sudo dpkg -i xxx.deb` |
| Fedora | `.rpm` | `sudo dnf install xxx.rpm` |

## 功能一览

涵盖编码转换、网络调试、文件处理、系统工具等多个类别：

**编码 & 格式化**：JSON 格式化、XML 格式化、YAML 格式化、Base64 编解码、进制转换、正则测试

**网络工具**：HTTP 客户端、FTP 客户端/服务器、SSH 客户端、Ping、端口扫描、路由追踪、Telnet、Whois 查询、IP 查询、网络扫描

**文件 & 文本**：文件搜索、文件比较、文件哈希计算、记事本、字符统计、文本加解密、PDF 合并、种子文件分析

**图像工具**：图片压缩、格式转换、添加水印、二维码/条形码生成、图标制作、OpenCV 图像处理

**系统工具**：Hosts 编辑器、系统信息、按键映射、终端、摄像头工具、管理员权限工具

**其他**：UUID 生成、随机密码生成、Cron 表达式计算、日期时间工具、邮编查询、手机归属地、血型工具、字帖生成 ...

## 构建

### 依赖

- CMake >= 3.10
- Qt 6（Widgets, Network, Concurrent, Sql, PrintSupport, LinguistTools）
- C++20 编译器

可选依赖（通过 CMake 开关控制）：

| 依赖 | 开关 | 说明 |
|------|------|------|
| OpenCV | `-DWITH_OPENCV=ON` | 图像处理模块 |
| Qt Multimedia | `-DWITH_QT_MULTIMEDIA=ON` | 摄像头工具 |
| MySQL Client | `-DWITH_MYSQL=ON` | 数据库工具 |
| libssh | `-DWITH_LIBSSH=ON` | SSH/SFTP 客户端 |
| libpcap | `-DWITH_PCAP=ON` | 增强网络抓包 |
| redis-plus-plus | `-DENABLE_REDIS=ON` | Redis 客户端 |

### macOS

```bash
brew install qt opencv libssh hiredis
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
ninja
open lele-tools.app
```

### Ubuntu / Debian

```bash
sudo apt install qt6-base-dev qt6-tools-dev cmake ninja-build \
  libopencv-dev libssh-dev libhiredis-dev libpcap-dev
mkdir build && cd build
cmake .. -G Ninja
ninja
./lele-tools
```

### Windows

使用 Visual Studio 2022 + Qt 6.9，参考 [CI 配置](.github/workflows/build-and-release.yml)。

## 国际化

```bash
lupdate -locations none ./ -ts translations/lele-tools_en_US.ts translations/lele-tools_zh_CN.ts
```

## 开源协议

[MIT License](LICENSE)

---

## 关于开发者

**武汉斯凯勒网络科技有限公司**

我们提供专业的软件定制开发服务：

- 小程序开发（微信 / 支付宝 / 抖音）
- App 开发（iOS / Android）
- 桌面 PC 应用开发（Windows / macOS / Linux）
- 企业定制化解决方案

### 联系我们

| 方式 | 联系方式 |
|------|---------|
| 官网 | [www.skyler.uno](https://www.skyler.uno) |
| 邮箱 | duhbbx@gmail.com |
| 微信 | tuhoooo |

### 扫码加微信

<img src="resources/wechat-qr.jpg" width="200" alt="微信二维码" />

欢迎扫码添加微信沟通需求，我们将竭诚为您服务！
