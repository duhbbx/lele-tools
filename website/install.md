# 下载与安装

## 推荐入口

直接从 [**GitHub Releases**](https://github.com/duhbbx/lele-tools/releases/latest) 下载最新版。

国内 CDN 加速下载：[`skylerx-build.cn-shanghai.taihangpfm.cn`](https://skylerx-build.cn-shanghai.taihangpfm.cn/lele-tools/latest/)（同步 GitHub Release，墙内速度更快）。

## 平台对照表

| 平台 | 文件 | 安装方式 |
|------|------|---------|
| **Windows 10/11** | `lele-tools-x.y.z-windows-x64.zip` | 解压即用，免安装。需要 [Visual C++ Redistributable 2022](https://aka.ms/vs/17/release/vc_redist.x64.exe) |
| **Windows 10/11** | `lele-tools-x.y.z-windows-x64.exe` | 安装向导式 |
| **macOS 11+** | `lele-tools-x.y.z-macos.dmg` | 双击挂载，拖入 Applications |
| **Ubuntu 22.04/24.04** | `lele-tools_x.y.z_ubuntu_amd64.deb` | `sudo dpkg -i xxx.deb` |
| **Debian 12+** | `lele-tools_x.y.z_debian_amd64.deb` | `sudo dpkg -i xxx.deb` |
| **Fedora 41+** | `lele-tools-x.y.z-fedora.x86_64.rpm` | `sudo dnf install xxx.rpm` |

## macOS 首次打开提示"无法打开"

因为没在苹果开发者后台公证，首次打开会拦截。打开终端运行一次：

```bash
xattr -dr com.apple.quarantine /Applications/lele-tools.app
```

然后正常双击打开即可。

或者：右键 → 打开 → 仍要打开。

## Linux 缺依赖

`.deb` / `.rpm` 包已声明依赖，正常包管理器装时会自动拉取。如果手动复制可执行文件运行，按需安装：

```bash
# Ubuntu / Debian
sudo apt install qt6-base qt6-tools libqt6sql6-sqlite libopencv-dev libssh-4

# Fedora
sudo dnf install qt6-qtbase qt6-qttools opencv libssh
```

## 从源码构建

完整步骤见仓库 [README](https://github.com/duhbbx/lele-tools#%E6%9E%84%E5%BB%BA)，简要：

```bash
git clone https://github.com/duhbbx/lele-tools.git
cd lele-tools

# macOS
brew install qt opencv libssh mariadb-connector-c postgresql jsoncpp vtk
PKG_CONFIG_PATH=/opt/homebrew/opt/mariadb-connector-c/lib/pkgconfig \
  cmake -S . -B build -G Ninja
cmake --build build --target lele-tools
open build/lele-tools.app

# Ubuntu / Debian
sudo apt install qt6-base-dev qt6-tools-dev cmake ninja-build \
  libopencv-dev libssh-dev libpq-dev libmariadb-dev libpcap-dev
cmake -S . -B build -G Ninja
cmake --build build --target lele-tools
./build/lele-tools
```

## 自动更新检查

App 启动时会自动检查 GitHub Release 是否有新版（每天一次，可在设置里关掉）。检查到新版会在状态栏提示，不强制弹窗。

## 上一版本回退

每个版本在 OSS CDN 上都按版本号归档：

```
https://skylerx-build.cn-shanghai.taihangpfm.cn/lele-tools/<版本号>/
```

历史版本永久保留，可以随时回退到任意旧版本。
