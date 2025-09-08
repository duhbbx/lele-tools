# 终端工具模块集成说明

## 问题描述
在工具列表中选择"终端工具"时，出现"对应的模块还未实现"的错误提示。

## 问题原因
终端工具模块已经正确创建和注册，但可能由于以下原因之一导致模块未被正确加载：
1. 项目需要重新编译以包含新的源文件
2. CMake缓存需要清理
3. 动态对象注册未生效

## 解决方案

### 方案1: 使用提供的重新构建脚本
```powershell
# 在项目根目录运行
.\script\rebuild-with-terminal.ps1
```

### 方案2: 手动重新构建
```powershell
# 1. 清理构建目录
Remove-Item -Recurse -Force cmake-build-debug-visual-studio

# 2. 重新配置CMake
mkdir cmake-build-debug-visual-studio
cd cmake-build-debug-visual-studio
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.9.1\msvc2022_64"

# 3. 重新编译
cmake --build . --config Debug --parallel
```

### 方案3: 在IDE中重新构建
1. 在CLion或Visual Studio中选择"Clean"
2. 重新配置CMake项目
3. 重新构建整个项目

## 验证步骤
构建完成后，执行以下步骤验证终端工具模块是否正确集成：

1. **启动应用程序**
2. **查找终端工具**: 在左侧工具列表中查找"终端工具"
3. **点击测试**: 点击"终端工具"，应该能正常打开而不会出现错误提示
4. **功能测试**: 在终端中输入命令测试功能是否正常

## 终端工具功能特性
成功集成后，终端工具将提供：
- ✅ Linux风格的命令行界面
- ✅ 命令历史记录（上下箭头浏览）
- ✅ 自动补全（Tab键）
- ✅ 内置命令（cd, pwd, clear, help等）
- ✅ 系统命令执行
- ✅ 彩色输出显示
- ✅ 快捷键支持（Ctrl+L清屏，Ctrl+C中断）

## 技术详情

### 文件结构
```
modules/terminal-tool/
├── terminaltool.h          # 头文件
├── terminaltool.cpp        # 实现文件
└── (README.md 已删除)
```

### 模块注册
```cpp
// 在terminaltool.cpp中
REGISTER_DYNAMICOBJECT(TerminalTool);
```

### 元数据配置
```cpp
// 在tool-list/module-meta-data.h中
{"xxx", "终端工具", "TerminalTool"},
```

### CMake配置
模块通过以下方式自动包含：
```cmake
file(GLOB_RECURSE SOURCES ./modules/*.cpp)
```

## 常见问题

### Q: 重新编译后仍然显示"模块还未实现"
**A**: 检查以下几点：
1. 确认构建成功没有错误
2. 确认使用的是新编译的可执行文件
3. 清理所有缓存文件重新构建

### Q: 编译时出现Qt Multimedia相关错误
**A**: 确保安装了完整的Qt 6.9.1，包括Multimedia模块

### Q: 终端工具界面显示异常
**A**: 检查Qt版本是否正确，终端工具需要Qt 6.0+

### Q: 命令执行没有响应
**A**: 检查QProcess权限和系统PATH环境变量

## 调试信息

如果问题仍然存在，可以添加以下调试代码到`main.cpp`中：
```cpp
#include "modules/terminal-tool/terminaltool.h"  // 临时添加

// 在main函数中添加
qDebug() << "Testing TerminalTool registration...";
DynamicObjectBase* test = DynamicObjectFactory::Instance()->CreateObject("TerminalTool");
if (test) {
    qDebug() << "TerminalTool registered successfully";
    delete test;
} else {
    qDebug() << "TerminalTool NOT registered";
}
```

## 联系支持
如果按照以上步骤仍无法解决问题，请提供：
1. 构建日志
2. 应用程序启动日志
3. 系统环境信息（Qt版本、编译器版本等）
