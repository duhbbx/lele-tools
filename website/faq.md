# 常见问题

## App 启动 / 兼容

### Windows 提示缺少 dll

通常缺 Microsoft Visual C++ 2022 Redistributable。装一次终身有效：[官方下载](https://aka.ms/vs/17/release/vc_redist.x64.exe)。

### macOS 提示"已损坏"或"无法打开"

App 没在苹果公证（公证一年要交 99 美元，开源项目不值得）。终端运行一次：

```bash
xattr -dr com.apple.quarantine /Applications/lele-tools.app
```

或者：右键 → 打开 → 仍要打开。一次性操作，下次直接双击就行。

### Linux 启动报错"找不到 libQt6XXX"

发行版的 Qt6 包版本太老。`.deb` / `.rpm` 包已经声明了最低版本依赖，正常 `apt install xxx.deb` 会自动拉。如果还报错：

```bash
# Ubuntu 22.04 默认 Qt6 太老，需要装 6.4+
sudo add-apt-repository ppa:beineri/opt-qt-6.4.0-jammy
sudo apt update
sudo apt install qt6-base qt6-tools
```

## 功能使用

### 数据库工具连接 MySQL 报错 mysql_native_password cannot be loaded

MySQL 9 客户端把 mysql_native_password 插件移除了，但很多老服务器还在用这个认证。两种修法：

**服务器侧**（推荐）：
```sql
ALTER USER 'user'@'%' IDENTIFIED WITH caching_sha2_password BY '密码';
```

**客户端侧**：lele-tools 编译时优先链 MariaDB Connector/C（同时支持新老认证）。如果你拿到的是没用 MariaDB Connector/C 链接的版本，重新编译或换发行版包。

### PDF 加水印后体积涨了好多

QPdfWriter 不支持原生 JPEG 嵌入，只能光栅化页面再 zlib 压缩。文本类 PDF 会膨胀。在"输出质量"下拉里选**极小 (110 DPI)** 或 **极致 (85 DPI)**，体积能压缩到原来的 1/4~1/2。

### 富文本笔记字号偶尔变小

历史 bug，1.2.30+ 版本已强制锁定 20pt 最小，且每次按键都重置 currentCharFormat。如果你用的是旧版本，更新到最新即可。

### OSS 图床上传报错 SignatureDoesNotMatch

99% 是 AccessKey Secret 复制错了（少字符 / 多空格 / 复制了引号）。回控制台重新复制粘贴。

### 工资计算器的 2026 节假日数据是哪来的

2024 / 2025 是国务院已发布的官方版（内置）。2026 起会自动从 [timor.tech](https://timor.tech) 拉最新版（开源公益服务）。也可以点"📥 从网络更新"手动拉取。

## 数据安全

### 我的数据存在哪

全部本地 SQLite，路径：

- Windows: `%LOCALAPPDATA%/duhbbx/lele-tools/`
- macOS: `~/Library/Application Support/duhbbx/lele-tools/`
- Linux: `~/.local/share/duhbbx/lele-tools/`

可以直接备份这个目录迁移到其他机器。

### 是否上传数据到服务器

**没有**。所有功能本地运行，App 内不收集任何用户数据。唯一会联网的是：

- 自动检查更新（拉 GitHub Releases JSON，每天一次，可在设置关闭）
- 公网 IP 查询、节假日数据、OSS 图床 — 这些是工具本身需要联网的功能，用户主动触发

### 隐私问题怎么报告

发现任何怀疑的网络请求 / 数据泄露，发 issue 或者邮件 duhbbx@gmail.com，24 小时内处理。

## 贡献 / 商务

### 想添加新工具 / 修 bug，怎么提 PR

1. Fork [仓库](https://github.com/duhbbx/lele-tools)
2. 看 [CLAUDE.md](https://github.com/duhbbx/lele-tools/blob/master/CLAUDE.md) 了解架构
3. 在 `modules/<新模块>/` 创建文件，参考其他模块
4. 在 `tool-list/module-meta.h` 注册
5. 提 PR

主仓库平均 24 小时内 review。

### 商务定制 / 软件外包

- 桌面 PC 应用（Qt / Electron）
- 小程序（微信 / 支付宝 / 抖音）
- App（iOS / Android）
- 企业管理系统

联系方式：

- 微信：**tuhoooo**
- 邮箱：**duhbbx@gmail.com**
- 官网：[www.skyler.uno](https://www.skyler.uno)

介绍项目有丰厚分佣。
