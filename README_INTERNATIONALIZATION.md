# 工具列表国际化系统

## 概述

lele-tools 的工具列表现在完全支持国际化，可以根据用户的语言设置显示不同语言的工具名称。

## 已实现的功能

### ✅ 完成的改进

1. **模块元数据结构重构**
   - 将 `title` 字段改为 `titleKey`，存储翻译键而不是硬编码文本
   - 添加 `getLocalizedTitle()` 方法获取翻译后的标题
   - 支持动态语言切换

2. **工具列表显示优化**
   - `ToolList` 类现在使用 `tr()` 函数进行翻译
   - 工具名称根据当前语言环境自动显示
   - 保持按使用频率排序功能

3. **完整的翻译覆盖**
   - 所有 42+ 个工具模块都已添加翻译
   - 中文翻译保持原有名称
   - 英文翻译进行了优化，使其更符合英文习惯

## 技术实现

### 1. 模块元数据结构

```cpp
struct ModuleMeta {
    QString icon;
    QString titleKey;       // 翻译键值，而不是直接的标题
    QString className;

    // 获取翻译后的标题
    QString getLocalizedTitle() const {
        return QObject::tr(titleKey.toUtf8().constData());
    }
};
```

### 2. 模块数据文件

**之前（硬编码中文）：**
```cpp
{"xxx", "XML格式化", "XmlFormatter"},
{"xxx", "JSON格式化", "JsonFormatter"},
```

**现在（翻译键）：**
```cpp
{"xxx", "XML Formatter", "XmlFormatter"},
{"xxx", "JSON Formatter", "JsonFormatter"},
```

### 3. 工具列表显示

```cpp
// 在 toollist.cpp 中使用翻译
for (const auto& [icon, titleKey, className] : moduleMetaArray) {
    ToolItem item;
    item.icon = icon;
    item.title = tr(titleKey.toUtf8().constData()); // 使用翻译
    item.className = className;
    // ...
}
```

## 翻译内容

### 中文翻译 (zh_CN)
- 保持原有的中文工具名称
- 例如：`"XML Formatter" → "XML格式化"`

### 英文翻译 (en_US)
- 优化的英文表达
- 例如：
  - `"Base64 Encode Decode" → "Base64 Encoder/Decoder"`
  - `"Image Text Recognition" → "OCR Text Recognition"`
  - `"Rich Text Notepad" → "Rich Text Editor"`

## 完整工具列表翻译对照

| 英文键值 | 中文翻译 | 英文显示 |
|---------|----------|----------|
| XML Formatter | XML格式化 | XML Formatter |
| JSON Formatter | JSON格式化 | JSON Formatter |
| YAML Formatter | YAML格式化 | YAML Formatter |
| Date Time Util | 日期时间戳 | Date Time Util |
| Base64 Encode Decode | Base64编码解码 | Base64 Encoder/Decoder |
| Regex Content Generator | 正则表达式生成内容 | Regex Content Generator |
| Random Password Generator | 随机密码生成 | Random Password Generator |
| Telnet Tool | Telnet工具 | Telnet Tool |
| Windows Settings | Windows设置 | Windows Settings |
| Hosts Editor (Table) | Hosts编辑(表格) | Hosts Editor (Table) |
| Hosts Editor (Text) | Hosts编辑(文本) | Hosts Editor (Text) |
| Ping Tool | Ping工具 | Ping Tool |
| Network Scanner | 局域网扫描 | Network Scanner |
| Database Tool | 数据库连接工具 | Database Tool |
| IP Lookup Tool | IP地址查询 | IP Lookup Tool |
| PDF Merge | Pdf合并 | PDF Merge |
| Regex Test Tool | 正则测试工具 | Regex Test Tool |
| Image Compression | 图片压缩 | Image Compression |
| Favicon Production | favicon制作 | Favicon Generator |
| Color Tools | 颜色工具 | Color Tools |
| Mobile Location | 手机号归属地 | Mobile Number Location |
| HTML Special Character Table | Html特殊字符表 | HTML Special Characters |
| Torrent File Analysis | 种子文件分析 | Torrent File Analyzer |
| Zip Code Query | 邮编区号查询 | Zip Code Lookup |
| QR Code Generator | 二维码 | QR Code Generator |
| Image Text Recognition | 图片文字识别 | OCR Text Recognition |
| File Hash Calculation | 文件hash计算 | File Hash Calculator |
| Barcode Generator | 条形码生成 | Barcode Generator |
| Image Format Conversion | 图片格式转换 | Image Format Converter |
| HTTP Status Code | HTTP状态码 | HTTP Status Codes |
| Crontab Time Calculation | crontab时间计算 | Cron Expression Tool |
| Text Encryption And Decryption | 文字加密解密 | Text Encryption/Decryption |
| UUID Generator | UUID生成器 | UUID Generator |
| OpenCV Demo | OpenCV示例 | OpenCV Demo |
| OpenCV Image Processor | OpenCV图像处理 | OpenCV Image Processor |
| SSH Client | SSH客户端 | SSH Client |
| FTP Client | FTP客户端 | FTP Client |
| FTP Server | FTP服务器 | FTP Server |
| Camera Tool | 摄像头工具 | Camera Tool |
| Terminal Tool | 终端工具 | Terminal Tool |
| Traceroute Tool | 路由追踪 | Traceroute Tool |
| Route Test Tool | 回程路由测试 | Route Test Tool |
| System Info Tool | 系统基本信息 | System Information |
| Rich Text Notepad | 富文本记事本 | Rich Text Editor |
| Media Manager | 媒体文件管理 | Media Manager |
| Image Watermark | 图片加水印 | Image Watermark |
| WHOIS Tool | WHOIS查询 | WHOIS Lookup |
| File Compare Tool | 文件对比 | File Compare Tool |
| Blood Type Tool | 血型遗传规律 | Blood Type Genetics |
| Port Scanner | 端口占用查看 | Port Scanner |
| Key Remapper | 改键工具 | Key Remapper |
| Chinese Copybook | 汉字字帖生成器 | Chinese Character Practice |

## 特性

### 🌍 多语言支持
- **自动切换**：根据系统语言或用户设置自动显示对应语言
- **实时更新**：语言切换时工具列表立即更新
- **完整覆盖**：所有工具模块都支持国际化

### 🔄 向后兼容
- **现有功能保持**：工具使用跟踪、按频率排序等功能正常工作
- **数据兼容**：现有的使用记录和配置不受影响
- **类名不变**：模块类名保持不变，确保内部逻辑正常

### 📈 扩展性
- **新语言支持**：可以轻松添加更多语言翻译
- **新工具支持**：新增工具只需在翻译文件中添加对应条目
- **维护简单**：翻译内容集中管理，易于维护更新

## 使用效果

### 中文环境
用户看到的是熟悉的中文工具名称：
- XML格式化
- JSON格式化
- 日期时间戳
- Base64编码解码
- ...

### 英文环境
用户看到的是优化的英文工具名称：
- XML Formatter
- JSON Formatter
- Date Time Util
- Base64 Encoder/Decoder
- ...

## 开发者指南

### 添加新工具模块
1. 在 `module-meta-data.h` 中添加条目（使用英文键值）
2. 在翻译文件中添加对应的翻译条目
3. 无需修改其他代码

### 添加新语言
1. 创建新的 `.ts` 翻译文件
2. 在 CMakeLists.txt 中添加新语言
3. 翻译 QObject 上下文中的工具模块条目

这个国际化系统确保了 lele-tools 能够为全球用户提供本地化的使用体验！