# 国际化（i18n）功能使用指南

## 概述

乐乐工具箱现在支持多语言国际化功能，目前支持中文（简体）和英文两种语言。

## 支持的语言

- **中文（简体）** (`zh_CN`) - 默认语言
- **English** (`en_US`) - 英文

## 如何切换语言

1. 在主窗口菜单栏中点击 **"语言(&L)"** 菜单
2. 选择您想要的语言：
   - 中文（简体）
   - English
3. 系统会提示您重启应用程序以使更改完全生效
4. 选择"立即重启"或"稍后重启"

## 语言设置持久化

- 语言设置会自动保存到用户配置文件中
- 下次启动应用程序时会自动加载上次选择的语言
- 如果是首次运行，系统会根据操作系统语言自动选择合适的语言

## 翻译文件格式

关于您询问的翻译文件格式问题：

### 为什么使用 `.ts` 而不是 `.xml`？

虽然Qt的翻译文件本质上是XML格式，但我们建议使用标准的 `.ts` 后缀，原因如下：

1. **Qt标准格式**：
   - `.ts` (Translation Source) - XML格式的源翻译文件
   - `.qm` (Qt Message) - 编译后的二进制格式

2. **工具链兼容性**：
   - Qt Linguist 专门支持 `.ts` 文件
   - `lupdate` 和 `lrelease` 工具针对 `.ts` 文件优化

3. **最佳实践**：
   - IDE 自动识别和语法高亮
   - 团队协作的一致性
   - 工具链的自动处理

## 开发者指南

### 添加新的翻译文本

在代码中使用 `tr()` 函数包装需要翻译的文本：

```cpp
// 正确的做法
setWindowTitle(tr("乐乐的工具箱"));
button->setText(tr("确定"));

// 错误的做法
setWindowTitle("乐乐的工具箱");  // 硬编码文本
```

### 更新翻译文件

1. **扫描源代码更新翻译文件**：
   ```bash
   # Windows PowerShell
   .\script\update_translations.ps1
   
   # 或者使用Qt工具
   lupdate . -ts translations/lele-tools_zh_CN.ts translations/lele-tools_en_US.ts
   ```

2. **编辑翻译文件**：
   - 使用 Qt Linguist 工具编辑 `.ts` 文件
   - 或者直接编辑XML格式的 `.ts` 文件

3. **生成二进制翻译文件**：
   ```bash
   # Windows PowerShell
   .\script\generate_translations.ps1
   
   # 或者使用Qt工具
   lrelease translations/lele-tools_zh_CN.ts
   lrelease translations/lele-tools_en_US.ts
   ```

### 文件结构

```
lele-tools/
├── translations/              # 翻译文件目录
│   ├── lele-tools_zh_CN.ts   # 中文翻译源文件
│   ├── lele-tools_en_US.ts   # 英文翻译源文件
│   ├── lele-tools_zh_CN.qm   # 中文编译后文件
│   └── lele-tools_en_US.qm   # 英文编译后文件
├── script/                    # 构建脚本
│   ├── update_translations.ps1    # 更新翻译文件
│   └── generate_translations.ps1  # 生成QM文件
└── my.qrc                     # 资源文件（包含翻译文件）
```

### CMake 配置

项目已配置支持Qt的翻译系统：

```cmake
# 翻译文件列表
set(TS_FILES 
    translations/lele-tools_zh_CN.ts
    translations/lele-tools_en_US.ts
)

# 自动生成翻译文件
qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
```

## 注意事项

1. **重启生效**：更改语言后需要重启应用程序才能完全生效
2. **模块支持**：目前主要在主窗口实现了国际化，各个工具模块正在逐步添加支持
3. **编码问题**：翻译文件使用UTF-8编码，确保正确显示各种字符
4. **占位符**：支持参数化翻译，如 `tr("语言已更改为 %1").arg(languageName)`

## 添加新语言

如需添加新语言支持：

1. 创建新的 `.ts` 文件，如 `lele-tools_ja_JP.ts`
2. 在 `CMakeLists.txt` 中添加到 `TS_FILES` 列表
3. 在 `my.qrc` 中添加对应的 `.qm` 文件
4. 在主窗口的语言菜单中添加新选项
5. 更新语言检测逻辑

## 故障排除

### 翻译不显示
- 检查 `.qm` 文件是否正确生成
- 确认资源文件 `my.qrc` 包含了翻译文件
- 验证翻译文件路径是否正确

### 编译错误
- 确保安装了Qt开发环境
- 检查 `lupdate` 和 `lrelease` 工具是否可用
- 验证CMake配置是否正确

### 字符显示问题
- 确保源文件使用UTF-8编码
- 检查翻译文件的编码格式
- 验证字体支持显示的字符集
