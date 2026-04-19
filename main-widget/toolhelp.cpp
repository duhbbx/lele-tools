#include "toolhelp.h"

QHash<QString, ToolHelpInfo> ToolHelp::s_helpData;
bool ToolHelp::s_initialized = false;

const ToolHelpInfo* ToolHelp::get(const QString& toolTitle) {
    if (!s_initialized) {
        init();
    }
    auto it = s_helpData.find(toolTitle);
    if (it != s_helpData.end()) {
        return &it.value();
    }
    return nullptr;
}

void ToolHelp::init() {
    s_initialized = true;

    s_helpData["XML Formatter"] = {
        "XML 格式化",
        "对 XML 数据进行格式化、压缩和语法校验，方便阅读和编辑。",
        "1. 在输入框中粘贴 XML 文本\n2. 点击\"格式化\"按钮美化显示\n3. 点击\"压缩\"去除多余空白",
        "支持大文件处理，自动检测并提示语法错误位置。",
        "Windows / macOS / Linux"
    };

    s_helpData["JSON Formatter"] = {
        "JSON 格式化",
        "对 JSON 数据进行格式化、压缩和校验。",
        "1. 在左侧输入框粘贴 JSON 文本\n2. 点击\"格式化\"按钮美化显示\n3. 点击\"压缩\"去除空白",
        "支持大文件处理，自动检测语法错误。",
        "Windows / macOS / Linux"
    };

    s_helpData["YAML Formatter"] = {
        "YAML 格式化",
        "对 YAML 数据进行格式化和语法校验，支持 YAML 与 JSON 互转。",
        "1. 在输入框中粘贴 YAML 文本\n2. 点击\"格式化\"按钮美化显示\n3. 可转换为 JSON 格式",
        "注意缩进敏感，格式化后请检查层级关系是否正确。",
        "Windows / macOS / Linux"
    };

    s_helpData["Date Time Util"] = {
        "日期时间工具",
        "提供时间戳与日期格式之间的转换，支持多种日期格式。",
        "1. 输入时间戳或日期字符串\n2. 选择目标格式\n3. 点击\"转换\"获取结果",
        "支持秒级和毫秒级时间戳，可查看当前时间戳。",
        "Windows / macOS / Linux"
    };

    s_helpData["Base64 Encode Decode"] = {
        "Base64 编解码",
        "对文本或文件进行 Base64 编码和解码操作。",
        "1. 输入要编码或解码的文本\n2. 点击\"编码\"或\"解码\"按钮\n3. 在输出区域查看结果",
        "支持文本和文件的 Base64 处理，可处理图片等二进制数据。",
        "Windows / macOS / Linux"
    };

    s_helpData["Regex Content Generator"] = {
        "正则内容生成器",
        "根据正则表达式模式生成匹配的随机内容。",
        "1. 输入正则表达式\n2. 设置生成数量\n3. 点击\"生成\"获取匹配内容",
        "适用于测试数据生成，复杂正则可能生成较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["Random Password Generator"] = {
        "随机密码生成器",
        "生成安全的随机密码，支持自定义长度和字符类型。",
        "1. 设置密码长度\n2. 选择包含的字符类型（大小写字母、数字、特殊字符）\n3. 点击\"生成\"",
        "建议密码长度不少于 12 位，包含多种字符类型以提高安全性。",
        "Windows / macOS / Linux"
    };

    s_helpData["Telnet Tool"] = {
        "Telnet 工具",
        "通过 Telnet 协议连接远程服务器，测试端口连通性。",
        "1. 输入目标主机地址和端口号\n2. 点击\"连接\"建立连接\n3. 在终端中输入命令交互",
        "仅用于测试和调试，Telnet 协议不加密，请勿传输敏感信息。",
        "Windows / macOS / Linux"
    };

    s_helpData["Hosts Editor (Table)"] = {
        "Hosts 编辑器（表格）",
        "以表格形式查看和编辑系统 hosts 文件，支持启用/禁用条目。",
        "1. 查看当前 hosts 条目列表\n2. 双击单元格编辑 IP 或域名\n3. 通过复选框启用或禁用条目\n4. 点击\"保存\"写入文件",
        "修改 hosts 文件需要管理员权限，保存前请确认内容正确。",
        "Windows / macOS / Linux"
    };

    s_helpData["Hosts Editor (Text)"] = {
        "Hosts 编辑器（文本）",
        "以纯文本形式直接编辑系统 hosts 文件。",
        "1. 查看和编辑 hosts 文件内容\n2. 直接修改文本\n3. 点击\"保存\"写入文件",
        "修改 hosts 文件需要管理员权限，注意格式为\"IP地址 域名\"。",
        "Windows / macOS / Linux"
    };

    s_helpData["Ping Tool"] = {
        "Ping 工具",
        "向目标主机发送 ICMP 请求，测试网络连通性和延迟。",
        "1. 输入目标主机地址或域名\n2. 点击\"Ping\"开始测试\n3. 查看延迟和丢包率",
        "部分网络环境可能屏蔽 ICMP 协议导致超时。",
        "Windows / macOS / Linux"
    };

    s_helpData["Network Scanner"] = {
        "网络扫描器",
        "扫描局域网内的活跃设备，发现网络中的主机和开放端口。",
        "1. 设置扫描的 IP 范围\n2. 点击\"开始扫描\"\n3. 查看扫描结果列表",
        "扫描大范围 IP 可能较慢，请在授权范围内使用。",
        "Windows / macOS / Linux"
    };

    s_helpData["Database Tool"] = {
        "数据库工具",
        "连接并管理 MySQL/SQLite 数据库，执行 SQL 查询和数据操作。",
        "1. 配置数据库连接信息\n2. 点击\"连接\"建立连接\n3. 在查询编辑器中输入 SQL 并执行",
        "MySQL 功能需要编译时启用 WITH_MYSQL 选项。SQLite 默认支持。",
        "Windows / macOS / Linux"
    };

    s_helpData["IP Lookup Tool"] = {
        "IP 查询工具",
        "查询 IP 地址的地理位置、运营商等归属信息。",
        "1. 输入要查询的 IP 地址\n2. 点击\"查询\"获取信息\n3. 查看地理位置和运营商详情",
        "查询结果来自在线数据库，需要网络连接。",
        "Windows / macOS / Linux"
    };

    s_helpData["PDF Merge"] = {
        "PDF 合并",
        "将多个 PDF 文件合并为一个文件。",
        "1. 添加需要合并的 PDF 文件\n2. 拖拽调整文件顺序\n3. 点击\"合并\"生成新文件",
        "合并后的文件大小为各文件之和，注意磁盘空间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Regex Test Tool"] = {
        "正则测试工具",
        "测试正则表达式是否匹配目标文本，实时高亮显示匹配结果。",
        "1. 在上方输入正则表达式\n2. 在下方输入测试文本\n3. 实时查看匹配结果和分组",
        "支持常用正则语法，可查看捕获分组详情。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Compression"] = {
        "图片压缩",
        "压缩图片文件大小，支持调整质量和分辨率。",
        "1. 选择或拖入图片文件\n2. 设置压缩质量参数\n3. 点击\"压缩\"并保存结果",
        "支持 JPG、PNG 等常见格式，压缩率与画质需权衡。",
        "Windows / macOS / Linux"
    };

    s_helpData["Favicon Production"] = {
        "Favicon 生成",
        "将图片转换为网站图标（favicon.ico），支持多种尺寸。",
        "1. 选择源图片文件\n2. 选择需要生成的尺寸\n3. 点击\"生成\"导出 ico 文件",
        "建议使用正方形图片作为源图，以获得最佳效果。",
        "Windows / macOS / Linux"
    };

    s_helpData["Color Tools"] = {
        "颜色工具",
        "提供颜色选择、格式转换（HEX/RGB/HSL）和调色板功能。",
        "1. 通过拾色器选择颜色\n2. 查看不同格式的颜色值\n3. 可复制所需格式的颜色代码",
        "支持 HEX、RGB、HSL 等多种颜色格式互转。",
        "Windows / macOS / Linux"
    };

    s_helpData["Mobile Location"] = {
        "手机号归属地",
        "查询手机号码的归属地和运营商信息。",
        "1. 输入手机号码\n2. 点击\"查询\"\n3. 查看归属地和运营商信息",
        "仅支持中国大陆手机号查询，数据可能存在延迟。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTML Special Character Table"] = {
        "HTML 特殊字符表",
        "提供 HTML 特殊字符和实体编码的对照表，方便查找和复制。",
        "1. 浏览字符列表或搜索特定字符\n2. 点击字符即可复制对应的 HTML 实体编码",
        "包含常用 HTML 实体、数学符号、箭头等特殊字符。",
        "Windows / macOS / Linux"
    };

    s_helpData["Torrent File Analysis"] = {
        "种子文件分析",
        "解析 .torrent 文件，查看其中包含的文件列表和元数据信息。",
        "1. 选择或拖入 .torrent 文件\n2. 查看文件列表、大小和哈希信息",
        "仅分析种子文件结构，不提供下载功能。",
        "Windows / macOS / Linux"
    };

    s_helpData["Zip Code Query"] = {
        "邮编查询",
        "查询中国邮政编码对应的地区信息。",
        "1. 输入邮政编码或地区名称\n2. 点击\"查询\"获取结果",
        "数据基于中国邮政编码体系。",
        "Windows / macOS / Linux"
    };

    s_helpData["QR Code Generator"] = {
        "二维码生成器",
        "根据输入的文本或链接生成二维码图片。",
        "1. 输入文本或 URL\n2. 设置二维码尺寸和纠错级别\n3. 点击\"生成\"并保存图片",
        "支持自定义尺寸，可导出为 PNG 图片文件。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Text Recognition"] = {
        "图片文字识别",
        "识别图片中的文字内容（OCR），提取文本信息。",
        "1. 选择或拖入包含文字的图片\n2. 点击\"识别\"开始处理\n3. 在结果区域查看和复制识别的文字",
        "识别准确率取决于图片清晰度，建议使用高清图片。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Hash Calculation"] = {
        "文件哈希计算",
        "计算文件的 MD5、SHA1、SHA256 等哈希值，用于校验文件完整性。",
        "1. 选择或拖入文件\n2. 选择哈希算法\n3. 查看计算结果",
        "支持 MD5、SHA1、SHA256 等多种算法，大文件计算可能需要较长时间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Barcode Generator"] = {
        "条形码生成器",
        "根据输入数据生成标准条形码图片。",
        "1. 输入条形码数据\n2. 选择条形码类型（Code128、EAN13 等）\n3. 点击\"生成\"并保存",
        "不同类型的条形码对输入数据格式有要求，请注意规范。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Format Conversion"] = {
        "图片格式转换",
        "在不同图片格式之间进行转换，如 PNG、JPG、BMP、WebP 等。",
        "1. 选择或拖入源图片\n2. 选择目标格式\n3. 点击\"转换\"并保存",
        "支持批量转换，转换过程中可能会有质量损失（如 PNG 转 JPG）。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTTP Status Code"] = {
        "HTTP 状态码",
        "查看 HTTP 状态码及其含义的参考手册。",
        "1. 浏览状态码列表\n2. 搜索特定状态码\n3. 查看详细说明",
        "涵盖 1xx~5xx 全部标准 HTTP 状态码。",
        "Windows / macOS / Linux"
    };

    s_helpData["Crontab Time Calculation"] = {
        "Crontab 时间计算",
        "解析 Crontab 表达式，计算并显示未来的执行时间。",
        "1. 输入 Crontab 表达式（如 */5 * * * *）\n2. 查看表达式含义说明\n3. 查看未来 N 次执行时间",
        "支持标准五段式 Crontab 语法。",
        "Windows / macOS / Linux"
    };

    s_helpData["Text Encryption And Decryption"] = {
        "文本加解密",
        "对文本进行加密和解密操作，支持多种加密算法。",
        "1. 输入明文或密文\n2. 选择加密算法和密钥\n3. 点击\"加密\"或\"解密\"",
        "请妥善保管密钥，丢失密钥将无法解密数据。",
        "Windows / macOS / Linux"
    };

    s_helpData["UUID Generator"] = {
        "UUID 生成器",
        "生成通用唯一标识符（UUID），支持多种版本。",
        "1. 选择 UUID 版本\n2. 设置生成数量\n3. 点击\"生成\"获取 UUID",
        "UUID v4 为随机生成，碰撞概率极低，适合大多数场景。",
        "Windows / macOS / Linux"
    };

    s_helpData["OpenCV Demo"] = {
        "OpenCV 演示",
        "展示 OpenCV 图像处理的基本功能和效果演示。",
        "1. 选择演示项目\n2. 加载图片或使用示例图片\n3. 查看处理效果",
        "需要编译时启用 WITH_OPENCV 选项。",
        "Windows / macOS / Linux"
    };

    s_helpData["OpenCV Image Processor"] = {
        "OpenCV 图像处理器",
        "使用 OpenCV 对图片进行高级处理，如滤波、边缘检测、变换等。",
        "1. 加载图片\n2. 选择处理算法和参数\n3. 预览效果并保存结果",
        "需要编译时启用 WITH_OPENCV 选项，处理大图可能较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["SSH Client"] = {
        "SSH 客户端",
        "通过 SSH 协议连接远程服务器，进行安全的远程终端操作。",
        "1. 输入服务器地址、端口、用户名和密码/密钥\n2. 点击\"连接\"建立 SSH 会话\n3. 在终端中输入命令操作",
        "需要编译时启用 WITH_LIBSSH 选项。支持密码和密钥认证。",
        "Windows / macOS / Linux"
    };

    s_helpData["FTP Client"] = {
        "FTP 客户端",
        "连接 FTP 服务器，进行文件的上传和下载管理。",
        "1. 输入 FTP 服务器地址和认证信息\n2. 点击\"连接\"登录服务器\n3. 浏览目录、上传或下载文件",
        "支持主动和被动模式，大文件传输请确保网络稳定。",
        "Windows / macOS / Linux"
    };

    s_helpData["FTP Server"] = {
        "FTP 服务器",
        "在本机启动一个简易 FTP 服务器，方便局域网文件共享。",
        "1. 设置共享目录和端口号\n2. 配置访问权限\n3. 点击\"启动\"开始服务",
        "仅建议在受信任的局域网内使用，注意防火墙端口放行。",
        "Windows / macOS / Linux"
    };

    s_helpData["Camera Tool"] = {
        "摄像头工具",
        "调用本机摄像头进行预览和拍照。",
        "1. 选择摄像头设备\n2. 点击\"开启\"预览画面\n3. 点击\"拍照\"捕获当前画面",
        "需要编译时启用 WITH_QT_MULTIMEDIA 选项，需要摄像头设备。",
        "Windows / macOS / Linux"
    };

    s_helpData["Terminal Tool"] = {
        "终端工具",
        "内嵌终端模拟器，可在应用内直接执行系统命令。",
        "1. 在命令行中输入命令\n2. 按回车执行\n3. 查看输出结果",
        "命令执行权限与当前用户一致，请谨慎使用危险命令。",
        "Windows / macOS / Linux"
    };

    s_helpData["Traceroute Tool"] = {
        "路由追踪工具",
        "追踪数据包到目标主机的路由路径，显示每一跳的延迟。",
        "1. 输入目标主机地址或域名\n2. 点击\"开始追踪\"\n3. 查看路由路径和各节点延迟",
        "某些路由节点可能不响应，显示为超时属正常现象。",
        "Windows / macOS / Linux"
    };

    s_helpData["Route Test Tool"] = {
        "路由测试工具",
        "测试本机路由表和网络路由配置是否正常。",
        "1. 选择测试类型\n2. 输入目标地址\n3. 点击\"测试\"查看路由信息",
        "可用于排查网络不通问题。",
        "Windows / macOS / Linux"
    };

    s_helpData["System Info Tool"] = {
        "系统信息工具",
        "显示当前系统的硬件和软件详细信息。",
        "1. 打开工具即可查看系统信息\n2. 包括 CPU、内存、操作系统、网络等信息",
        "部分硬件信息可能因系统权限限制而无法获取。",
        "Windows / macOS / Linux"
    };

    s_helpData["Rich Text Notepad"] = {
        "富文本记事本",
        "支持富文本编辑的记事本，可设置字体、颜色、插入图片等。",
        "1. 在编辑区域输入文本\n2. 使用工具栏设置格式\n3. 保存为文件",
        "支持常见的富文本格式化操作，可导出为多种格式。",
        "Windows / macOS / Linux"
    };

    s_helpData["Media Manager"] = {
        "媒体管理器",
        "管理和浏览本地媒体文件，支持图片和视频预览。",
        "1. 选择媒体文件目录\n2. 浏览和筛选媒体文件\n3. 预览或打开文件",
        "需要编译时启用 WITH_QT_MULTIMEDIA 选项以支持视频预览。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Watermark"] = {
        "图片水印",
        "为图片添加文字或图片水印，保护图片版权。",
        "1. 选择或拖入图片\n2. 设置水印文字或水印图片\n3. 调整位置、透明度等参数\n4. 保存结果",
        "支持批量添加水印，可自定义水印样式和位置。",
        "Windows / macOS / Linux"
    };

    s_helpData["WHOIS Tool"] = {
        "WHOIS 查询",
        "查询域名的 WHOIS 注册信息，包括注册商、到期时间等。",
        "1. 输入域名\n2. 点击\"查询\"\n3. 查看 WHOIS 详细信息",
        "查询需要网络连接，部分域名信息可能受隐私保护。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Compare Tool"] = {
        "文件比较工具",
        "对比两个文件的差异，高亮显示不同之处。",
        "1. 选择两个要比较的文件\n2. 点击\"比较\"\n3. 查看差异高亮结果",
        "支持文本文件对比，大文件比较可能需要较长时间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Blood Type Tool"] = {
        "血型工具",
        "查询父母血型组合下子女可能的血型及概率。",
        "1. 选择父亲血型\n2. 选择母亲血型\n3. 查看子女可能的血型和概率",
        "基于 ABO 和 Rh 血型系统的遗传规律计算。",
        "Windows / macOS / Linux"
    };

    s_helpData["Port Scanner"] = {
        "端口扫描器",
        "扫描目标主机的端口开放状态。",
        "1. 输入目标主机地址\n2. 设置端口范围\n3. 点击\"扫描\"查看结果",
        "请在授权范围内使用，扫描大量端口可能较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["Key Remapper"] = {
        "按键映射",
        "重新映射键盘按键，将一个按键映射为另一个按键。",
        "1. 选择原始按键\n2. 设置映射目标按键\n3. 启用映射",
        "映射仅在应用运行时生效，关闭后恢复原按键功能。",
        "Windows / macOS / Linux"
    };

    s_helpData["Chinese Copybook"] = {
        "中文字帖",
        "生成中文书法练习字帖，可自定义内容和样式。",
        "1. 输入要练习的文字\n2. 选择字体和格式\n3. 生成并打印字帖",
        "支持多种字体和田字格/米字格样式。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTTP Client"] = {
        "HTTP 客户端",
        "发送 HTTP 请求，支持 GET、POST 等多种方法，方便接口调试。",
        "1. 输入请求 URL\n2. 选择请求方法，配置请求头和请求体\n3. 点击\"发送\"查看响应结果",
        "支持自定义请求头、请求体和认证信息。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Search"] = {
        "文件搜索",
        "在指定目录中快速搜索文件，支持文件名和内容匹配。",
        "1. 选择搜索目录\n2. 输入搜索关键词或通配符\n3. 点击\"搜索\"查看结果",
        "支持递归搜索子目录，文件数量较多时请耐心等待。",
        "Windows / macOS / Linux"
    };

    s_helpData["Character Counter"] = {
        "字符统计",
        "统计文本的字符数、单词数、行数等信息。",
        "1. 在输入框中粘贴或输入文本\n2. 实时查看字符、单词、行数统计",
        "支持中英文混合文本统计，包含字节数统计。",
        "Windows / macOS / Linux"
    };

    s_helpData["Images to PDF"] = {
        "图片转 PDF",
        "将多张图片合并为一个 PDF 文件。",
        "1. 添加图片文件\n2. 拖拽调整图片顺序\n3. 点击\"生成 PDF\"保存",
        "支持 JPG、PNG 等常见图片格式，注意图片数量对文件大小的影响。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Collection"] = {
        "图片收藏",
        "管理和收藏图片，方便分类浏览和查找。",
        "1. 添加图片到收藏\n2. 创建分类管理\n3. 浏览和搜索收藏的图片",
        "图片以引用方式管理，不会复制原文件。",
        "Windows / macOS / Linux"
    };

    s_helpData["Calculator"] = {
        "计算器",
        "提供基础和科学计算功能。",
        "1. 输入数学表达式\n2. 点击\"=\"或按回车计算结果",
        "支持四则运算、括号和常用数学函数。",
        "Windows / macOS / Linux"
    };

    s_helpData["Admin Rights Tool"] = {
        "管理员权限工具",
        "以管理员权限执行特定系统操作。",
        "1. 选择需要提权执行的操作\n2. 确认操作后以管理员身份执行",
        "操作需要管理员权限，请谨慎使用。",
        "Windows"
    };

    s_helpData["Screen Capture"] = {
        "截图工具",
        "截取屏幕画面，支持区域选择和全屏截图。",
        "1. 按快捷键或点击按钮启动截图\n2. 拖拽选择截图区域\n3. 保存或复制到剪贴板",
        "支持快捷键触发，截图自动保存到剪贴板。",
        "Windows"
    };

    s_helpData["Windows Settings"] = {
        "Windows 设置",
        "快速访问 Windows 系统常用设置项。",
        "1. 浏览设置分类列表\n2. 点击对应项目快速打开系统设置",
        "仅在 Windows 系统上可用。",
        "Windows"
    };
}
