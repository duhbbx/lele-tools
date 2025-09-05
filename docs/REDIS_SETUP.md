# Redis-Plus-Plus 安装指南

本项目使用 [redis-plus-plus](https://github.com/sewenew/redis-plus-plus) 作为Redis客户端库。

## Windows 安装 (推荐使用vcpkg)

### 1. 安装vcpkg (如果还没有)
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### 2. 安装redis-plus-plus
```bash
# 安装redis-plus-plus (会自动安装hiredis依赖)
.\vcpkg install redis-plus-plus:x64-windows

# 如果需要静态链接
.\vcpkg install redis-plus-plus:x64-windows-static
```

### 3. 配置CMake
在CMake配置时指定vcpkg工具链：
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ..
```

## Linux 安装

### Ubuntu/Debian
```bash
# 安装依赖
sudo apt-get update
sudo apt-get install libhiredis-dev

# 从源码编译redis-plus-plus
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```

### CentOS/RHEL
```bash
# 安装依赖
sudo yum install hiredis-devel

# 编译安装同上
```

## macOS 安装

### 使用Homebrew
```bash
# 安装hiredis
brew install hiredis

# 编译redis-plus-plus
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```

## 手动编译安装

### 1. 安装hiredis
```bash
git clone https://github.com/redis/hiredis.git
cd hiredis
make
sudo make install
```

### 2. 编译redis-plus-plus
```bash
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build
cd build

# 配置编译选项
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
      ..

make
sudo make install
```

## 验证安装

编译项目时，CMake会自动检测redis-plus-plus库：

```
-- Found redis-plus-plus: /usr/local/lib/libredis++.so
-- redis-plus-plus version: 1.3.5
-- redis-plus-plus include dirs: /usr/local/include;/usr/local/include
-- redis-plus-plus libraries: /usr/local/lib/libredis++.so;/usr/local/lib/libhiredis.so
```

如果看到以下警告，说明库未安装：
```
-- redis-plus-plus not found. Building without Redis support.
-- To install redis-plus-plus:
--   Windows: vcpkg install redis-plus-plus
--   Linux: sudo apt-get install libsw-redis++-dev
--   macOS: brew install redis-plus-plus
```

## 故障排除

### 1. 找不到hiredis
确保安装了hiredis开发库：
- Windows: `vcpkg install hiredis:x64-windows`
- Linux: `sudo apt-get install libhiredis-dev`
- macOS: `brew install hiredis`

### 2. CMake找不到库
设置环境变量或CMake变量：
```bash
export RedisPlus_ROOT_DIR=/path/to/redis-plus-plus
# 或者
cmake -DRedisPlus_ROOT_DIR=/path/to/redis-plus-plus ..
```

### 3. 链接错误
确保库文件在系统路径中，或设置LD_LIBRARY_PATH：
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

## 功能特性

使用redis-plus-plus后，支持的Redis功能包括：
- ✅ 连接池管理
- ✅ 自动重连
- ✅ 管道操作
- ✅ 事务支持
- ✅ 发布/订阅
- ✅ Lua脚本
- ✅ 集群支持
- ✅ SSL/TLS加密
- ✅ 所有Redis数据类型
- ✅ 异步操作支持
