# 跨平台兼容性问题记录

本文档记录了项目从 Windows 单平台移植到 macOS（及未来 Linux）过程中发现并修复的兼容性问题，供后续开发参考。

---

## 1. `_stdcall` 调用约定（Windows 专有）

**文件：** `common/dynamicobjectbase.h`

**问题：** `_stdcall` 是 MSVC 专有的函数调用约定关键字，macOS 的 Clang 编译器无法识别，导致宏 `REGISTER_DYNAMICOBJECT` 和 `DynamicObjectFactory` 中的函数指针类型定义全部报错。这个问题会导致几乎所有模块的编译失败，因为每个模块都通过该宏进行注册。

**修复方式：** 定义跨平台宏 `DYNAMIC_CALL`，在 Windows 上展开为 `__stdcall`，其他平台为空：

```cpp
#ifdef _WIN32
#define DYNAMIC_CALL __stdcall
#else
#define DYNAMIC_CALL
#endif
```

**影响范围：** 全局，所有使用 `REGISTER_DYNAMICOBJECT` 宏的模块。

---

## 2. Redis 依赖缺少条件编译保护

**文件：**
- `common/connx/RedisConnection.h`
- `common/connx/RedisConnection.cpp`
- `common/connx/Connection.cpp`

**问题：** Redis 功能依赖 `redis-plus-plus` 和 `hiredis` 第三方库。头文件中 `#include <sw/redis++/redis++.h>` 已有 `#ifdef WITH_REDIS_PLUS_PLUS` 保护，但以下内容遗漏了保护：

- `RedisConnection.h` 中的成员变量 `m_redis`、`m_connOpts`、`m_poolOpts`（使用了 `sw::redis` 命名空间类型）
- `RedisConnection.h` 中的 `parseRedisReply(redisReply* reply)` 方法声明（`redisReply` 是 hiredis 类型）
- `RedisConnection.cpp` 整个文件（所有实现都依赖 redis-plus-plus）
- `Connection.cpp` 中无条件 `#include "RedisConnection.h"` 和 `new RedisConnection(config)` 调用

**修复方式：** 将 `RedisConnection.h` 中整个类定义、`RedisConnection.cpp` 整个文件、以及 `Connection.cpp` 中的引用和实例化，全部包裹在 `#ifdef WITH_REDIS_PLUS_PLUS` 中。

**经验：** 可选依赖的条件编译保护必须贯穿头文件声明、源文件实现、以及所有引用处，三者缺一不可。

---

## 3. Windows API 头文件缺少平台保护

**文件：**
- `common/admin-utils/adminprivilegehelper.h`（`#include <Windows.h>`）
- `common/admin-utils/adminprivilegehelper.cpp`（`#include <shellapi.h>`、`#include <shlobj.h>`）
- `modules/windows-settings/windowssettings.cpp`（`#include <Windows.h>`）
- `modules/traceroute-tool/traceroutetool.cpp`（`cleanupWindows()` 方法使用 `closesocket`、`WSACleanup`）

**问题：** 这些文件直接包含 Windows SDK 头文件或调用 Windows API 函数，但没有用 `#ifdef Q_OS_WIN` 保护，在 macOS/Linux 上编译时报 `fatal error: 'Windows.h' file not found` 或 `use of undeclared identifier`。

**修复方式：**
- 纯 Windows 功能的整个文件用 `#ifdef Q_OS_WIN ... #endif` 包裹
- 混合平台的文件中，将 Windows 头文件包含和 API 调用用 `#ifdef Q_OS_WIN` 保护
- 头文件中使用 Windows 特有类型（如 `HKEY`、`DWORD`）的声明也需要保护

**经验：** 头文件中的 Windows 类型会通过 Qt 的 AUTOMOC 机制被 moc 编译器处理，即使对应的 `.cpp` 加了平台保护，头文件没保护仍会导致 moc 编译失败。

---

## 4. 缺少 POSIX 头文件 `<unistd.h>`

**文件：**
- `modules/hosts-editor-text/hostseditortext.cpp`
- `modules/hosts-editor-table/hostseditortable.cpp`

**问题：** 这两个文件在 `#else`（非 Windows）分支中调用了 `getuid()` 函数来检查是否以 root 权限运行，但没有包含声明该函数的头文件 `<unistd.h>`。在某些编译器/平台上可能隐式声明通过，但 macOS Clang 严格模式下会报 `use of undeclared identifier 'getuid'`。

**修复方式：** 在 `#ifdef Q_OS_WIN` 的 `#else` 分支中添加 `#include <unistd.h>`：

```cpp
#ifdef Q_OS_WIN
#include <windows.h>
// ... other Windows headers
#else
#include <unistd.h>
#endif
```

---

## 5. `<sys/sysinfo.h>` 仅限 Linux

**文件：**
- `modules/system-info/systeminfo.h`
- `modules/system-info/systeminfo.cpp`

**问题：** `<sys/sysinfo.h>` 是 Linux 特有的头文件（提供 `struct sysinfo` 和 `sysinfo()` 函数），macOS 没有这个头文件。原始代码的 `#else` 分支（非 Windows）假定了 Linux 环境。

同时，`getLinuxSystemInfo()` 和 `getLinuxDiskInfo()` 两个方法只有声明没有非 Windows 平台的实现，导致链接时 `Undefined symbols` 错误。

**修复方式：**

头文件中区分三个平台：

```cpp
#ifdef Q_OS_WIN
    // Windows headers
#elif defined(Q_OS_MACOS)
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    // ... other macOS headers
#else
    #include <sys/sysinfo.h>
    // ... Linux headers
#endif
```

源文件中为非 Windows 平台提供实现，使用 Qt 跨平台 API：
- `QSysInfo` 获取操作系统信息
- `QStorageInfo` 获取磁盘信息

**经验：** `#else` 分支不应假设特定平台。如果 Windows 和 Linux 使用完全不同的系统 API，需要考虑 macOS 作为第三种情况，或使用 Qt 跨平台 API 作为通用实现。

---

## 6. Qt 信号槽声明与实现的平台保护不一致

**文件：**
- `modules/port-scanner/portscanner.h`
- `modules/port-scanner/portscanner.cpp`

**问题：** `showContextMenu`、`onKillProcessRequested`、`onShowProcessPathRequested` 三个 slot 方法在头文件中无条件声明（`private slots:` 区域），但在 `.cpp` 中被 `#ifdef Q_OS_WIN` 包裹。

Qt 的 AUTOMOC 会为头文件中声明的所有 slot 生成 moc 代码并引用这些函数。即使 `.cpp` 中的实现被条件编译跳过，moc 生成的代码仍然会引用这些符号，导致链接时 `Undefined symbols` 错误。

**修复方式：** 头文件中的 slot 声明和 `.cpp` 中的 `connect()` 调用都需要加上 `#ifdef Q_OS_WIN` 保护：

```cpp
// .h
private slots:
#ifdef Q_OS_WIN
    void showContextMenu(const QPoint& pos);
    void onKillProcessRequested();
    void onShowProcessPathRequested();
#endif

// .cpp
#ifdef Q_OS_WIN
    connect(portTable, &QTableWidget::customContextMenuRequested,
            this, &PortScanner::showContextMenu);
#endif
```

**经验：** 在 Qt 项目中，slot/signal 的条件编译必须在头文件声明和源文件实现两侧同步，否则 AUTOMOC 生成的代码会产生链接错误。

---

## 7. 源文件编码导致字符串截断

**文件：** `modules/port-scanner/portscanner.cpp:244`

**问题：** 中文字符串字面量被截断，`"扫描已取消?` 缺少闭合引号，导致编译器报 `expected expression` 错误。这类问题通常由文件编码不一致（如部分内容以 GBK 保存但编译器按 UTF-8 解析）或编辑器错误导致。

**修复方式：** 补全字符串为 `"扫描已取消"`。

**经验：** 项目应统一使用 UTF-8 编码。可在 `.editorconfig` 或 IDE 配置中强制指定，并在 CI 中检查。

---

## 跨平台开发规范建议

1. **新增 Windows API 调用时**，始终用 `#ifdef Q_OS_WIN` 包裹，包括头文件 include、函数声明、实现、以及 `connect()` 调用
2. **可选依赖**（Redis、OpenCV 等）的条件编译保护必须覆盖：头文件类型声明、成员变量、源文件实现、以及外部引用处
3. **避免在 `#else` 中假设 Linux**，macOS 与 Linux 的系统 API 差异很大（如 `sysinfo` vs `sysctl`），优先使用 Qt 跨平台 API
4. **Qt AUTOMOC 的特殊性**：slot/signal 的声明如果有平台条件编译，头文件和源文件必须同步
5. **文件编码统一 UTF-8**，避免中文字符在跨平台编译时出现截断或乱码
