# FindPcap.cmake 使用说明

这个CMake模块用于在不同环境中查找和配置Npcap/libpcap库。

## 支持的环境

### Windows
- **Npcap SDK** - 首选，支持进程信息获取
- **Windows ICMP API** - 回退方案，基本功能

### Unix-like Systems
- **libpcap** - 标准pcap库

## 自动检测路径

### GitHub Actions CI 环境
- `D:/a/lele-tools/lele-tools/npcap-sdk`
- `${CMAKE_SOURCE_DIR}/npcap-sdk`
- `$ENV{GITHUB_WORKSPACE}/npcap-sdk`

### 本地开发环境
- `C:/npcap-sdk-1.15`
- `C:/npcap-sdk-1.14`
- `C:/npcap-sdk-1.13`
- `C:/npcap-sdk`

### 系统安装路径
- `C:/Program Files/Npcap/sdk`
- `C:/Program Files (x86)/Npcap/sdk`

### 项目相对路径
- `${CMAKE_SOURCE_DIR}/third_party/npcap-sdk`
- `${CMAKE_SOURCE_DIR}/deps/npcap-sdk`
- `${CMAKE_SOURCE_DIR}/external/npcap-sdk`

## 手动指定路径

### 1. CMake缓存变量
```bash
cmake -S . -B build -DNPCAP_SDK_ROOT="C:/path/to/npcap-sdk"
```

### 2. 环境变量
```bash
# 任选其一
set NPCAP_SDK_PATH=C:/path/to/npcap-sdk
set PCAP_ROOT=C:/path/to/npcap-sdk
set NPCAP_ROOT=C:/path/to/npcap-sdk
set NPCAP_DIR=C:/path/to/npcap-sdk
```

### 3. 直接设置库路径
```bash
cmake -S . -B build ^
  -DPCAP_INCLUDE_DIR="C:/npcap-sdk/Include" ^
  -DPCAP_LIBRARY="C:/npcap-sdk/Lib/x64/wpcap.lib" ^
  -DPACKET_LIBRARY="C:/npcap-sdk/Lib/x64/packet.lib"
```

## 调试模式

启用详细的搜索路径输出：
```bash
cmake -S . -B build -DCMAKE_FIND_DEBUG_MODE=ON
```

## 禁用Pcap支持

如果不需要pcap功能：
```bash
cmake -S . -B build -DWITH_PCAP=OFF
```

## 预定义宏

FindPcap会根据找到的库设置以下宏：

- `WITH_PCAP` - 找到了Npcap/libpcap
- `WITH_WINDOWS_ICMP` - 使用Windows ICMP API回退

## 示例：GitHub Actions配置

```yaml
- name: Download Npcap SDK
  run: |
    wget https://github.com/nmap/npcap/releases/download/v1.80/npcap-sdk-1.80.zip
    7z x npcap-sdk-1.80.zip -o.

- name: Configure with CMake
  run: cmake -S . -B build
  # FindPcap 会自动找到 ./npcap-sdk
```

## 故障排除

1. **库未找到**：启用调试模式查看搜索路径
2. **架构不匹配**：确保使用正确的x64/x86库
3. **CI环境**：确保npcap-sdk解压到正确位置
