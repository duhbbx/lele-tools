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
        "基于内置的 pugixml 库解析 XML 文档树，通过 pugi::xml_document 加载并验证语法，\n再以自定义缩进参数输出格式化或压缩后的文本。",
        "支持大文件处理，自动检测并提示语法错误位置。",
        "Windows / macOS / Linux"
    };

    s_helpData["JSON Formatter"] = {
        "JSON 格式化",
        "对 JSON 数据进行格式化、压缩和校验。",
        "1. 在左侧输入框粘贴 JSON 文本\n2. 点击\"格式化\"按钮美化显示\n3. 点击\"压缩\"去除空白",
        "使用 Qt QJsonDocument::fromJson() 解析 JSON 文本并校验语法，\n通过 QJsonDocument::toJson(QJsonDocument::Indented) 输出格式化结果，\n压缩模式使用 QJsonDocument::Compact 格式输出。",
        "支持大文件处理，自动检测语法错误。",
        "Windows / macOS / Linux"
    };

    s_helpData["YAML Formatter"] = {
        "YAML 格式化",
        "对 YAML 数据进行格式化和语法校验，支持 YAML 与 JSON 互转。",
        "1. 在输入框中粘贴 YAML 文本\n2. 点击\"格式化\"按钮美化显示\n3. 可转换为 JSON 格式",
        "基于内置的 yaml-cpp 库，通过 YAML::Load() 解析文本并构建节点树，\n使用 YAML::Emitter 输出格式化结果；YAML 与 JSON 互转借助 QJsonDocument 桥接。",
        "注意缩进敏感，格式化后请检查层级关系是否正确。",
        "Windows / macOS / Linux"
    };

    s_helpData["Date Time Util"] = {
        "日期时间工具",
        "提供时间戳与日期格式之间的转换，支持多种日期格式。",
        "1. 输入时间戳或日期字符串\n2. 选择目标格式\n3. 点击\"转换\"获取结果",
        "使用 Qt QDateTime 进行时间戳与日期的互相转换，\n通过 QDateTime::fromSecsSinceEpoch() / fromMSecsSinceEpoch() 和 toString() 实现格式化输出。",
        "支持秒级和毫秒级时间戳，可查看当前时间戳。",
        "Windows / macOS / Linux"
    };

    s_helpData["Base64 Encode Decode"] = {
        "Base64 编解码",
        "对文本或文件进行 Base64 编码和解码操作。",
        "1. 输入要编码或解码的文本\n2. 点击\"编码\"或\"解码\"按钮\n3. 在输出区域查看结果",
        "使用 Qt QByteArray::toBase64() 进行编码，QByteArray::fromBase64() 进行解码，\n支持文本和二进制数据的 Base64 标准编解码。",
        "支持文本和文件的 Base64 处理，可处理图片等二进制数据。",
        "Windows / macOS / Linux"
    };

    s_helpData["Regex Content Generator"] = {
        "正则内容生成器",
        "根据正则表达式模式生成匹配的随机内容。",
        "1. 输入正则表达式\n2. 设置生成数量\n3. 点击\"生成\"获取匹配内容",
        "使用 Qt QRegularExpression（基于 PCRE2 引擎）解析正则表达式结构，\n结合 QRandomGenerator 随机生成符合模式的字符串。",
        "适用于测试数据生成，复杂正则可能生成较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["Random Password Generator"] = {
        "随机密码生成器",
        "生成安全的随机密码，支持自定义长度和字符类型。",
        "1. 设置密码长度\n2. 选择包含的字符类型（大小写字母、数字、特殊字符）\n3. 点击\"生成\"",
        "使用 Qt QRandomGenerator 生成密码学安全的随机数，\n从预定义的字符集（大小写字母、数字、特殊字符）中随机选取字符组合成密码。",
        "建议密码长度不少于 12 位，包含多种字符类型以提高安全性。",
        "Windows / macOS / Linux"
    };

    s_helpData["Telnet Tool"] = {
        "Telnet 工具",
        "通过 Telnet 协议连接远程服务器，测试端口连通性。",
        "1. 输入目标主机地址和端口号\n2. 点击\"连接\"建立连接\n3. 在终端中输入命令交互",
        "使用 Qt QTcpSocket 建立 TCP 连接，实现 Telnet 协议的数据收发，\n通过 readyRead 信号异步接收服务器响应并显示在终端界面。",
        "仅用于测试和调试，Telnet 协议不加密，请勿传输敏感信息。",
        "Windows / macOS / Linux"
    };

    s_helpData["Hosts Editor (Table)"] = {
        "Hosts 编辑器（表格）",
        "以表格形式查看和编辑系统 hosts 文件，支持启用/禁用条目。",
        "1. 查看当前 hosts 条目列表\n2. 双击单元格编辑 IP 或域名\n3. 通过复选框启用或禁用条目\n4. 点击\"保存\"写入文件",
        "通过直接文件 I/O 读写系统 hosts 文件，解析为表格数据模型；\n保存时通过 osascript（macOS）或 ShellExecuteEx（Windows）提升权限写入。",
        "修改 hosts 文件需要管理员权限，保存前请确认内容正确。",
        "Windows / macOS / Linux"
    };

    s_helpData["Hosts Editor (Text)"] = {
        "Hosts 编辑器（文本）",
        "以纯文本形式直接编辑系统 hosts 文件。",
        "1. 查看和编辑 hosts 文件内容\n2. 直接修改文本\n3. 点击\"保存\"写入文件",
        "通过直接文件 I/O 读取系统 hosts 文件内容到 QTextEdit 中编辑；\n保存时通过 osascript（macOS）或 ShellExecuteEx（Windows）提升权限写入。",
        "修改 hosts 文件需要管理员权限，注意格式为\"IP地址 域名\"。",
        "Windows / macOS / Linux"
    };

    s_helpData["Ping Tool"] = {
        "Ping 工具",
        "向目标主机发送 ICMP 请求，测试网络连通性和延迟。",
        "1. 输入目标主机地址或域名\n2. 点击\"Ping\"开始测试\n3. 查看延迟和丢包率",
        "通过 QProcess 调用系统自带的 ping 命令，\n解析命令输出获取延迟、TTL 和丢包率等信息并展示在界面上。",
        "部分网络环境可能屏蔽 ICMP 协议导致超时。",
        "Windows / macOS / Linux"
    };

    s_helpData["Network Scanner"] = {
        "网络扫描器",
        "扫描局域网内的活跃设备，发现网络中的主机和开放端口。",
        "1. 设置扫描的 IP 范围\n2. 点击\"开始扫描\"\n3. 查看扫描结果列表",
        "通过 QProcess 调用系统 arp 命令扫描局域网设备，\n解析 ARP 表获取活跃主机的 IP 和 MAC 地址信息。",
        "扫描大范围 IP 可能较慢，请在授权范围内使用。",
        "Windows / macOS / Linux"
    };

    s_helpData["Database Tool"] = {
        "数据库工具",
        "连接并管理 MySQL/SQLite 数据库，执行 SQL 查询和数据操作。",
        "1. 配置数据库连接信息\n2. 点击\"连接\"建立连接\n3. 在查询编辑器中输入 SQL 并执行",
        "使用 Qt QSqlDatabase + QSqlQuery 管理数据库连接和执行 SQL，\nSQLite 驱动内置支持，MySQL 通过原生驱动连接；查询结果通过 QSqlQueryModel 展示。",
        "MySQL 功能需要编译时启用 WITH_MYSQL 选项。SQLite 默认支持。",
        "Windows / macOS / Linux"
    };

    s_helpData["IP Lookup Tool"] = {
        "IP 查询工具",
        "查询 IP 地址的地理位置、运营商等归属信息。",
        "1. 输入要查询的 IP 地址\n2. 点击\"查询\"获取信息\n3. 查看地理位置和运营商详情",
        "使用 Qt QNetworkAccessManager 向 ip-api.com API 发送 HTTP 请求，\n解析返回的 JSON 数据获取 IP 地理位置、运营商等归属信息。",
        "查询结果来自在线数据库，需要网络连接。",
        "Windows / macOS / Linux"
    };

    s_helpData["PDF Merge"] = {
        "PDF 合并",
        "将多个 PDF 文件合并为一个文件。",
        "1. 添加需要合并的 PDF 文件\n2. 拖拽调整文件顺序\n3. 点击\"合并\"生成新文件",
        "使用 Qt QPdfWriter 创建输出 PDF，通过 QPainter 将每个源 PDF 的页面\n逐页绘制到新文件中，实现多文件合并。",
        "合并后的文件大小为各文件之和，注意磁盘空间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Regex Test Tool"] = {
        "正则测试工具",
        "测试正则表达式是否匹配目标文本，实时高亮显示匹配结果。",
        "1. 在上方输入正则表达式\n2. 在下方输入测试文本\n3. 实时查看匹配结果和分组",
        "使用 Qt QRegularExpression（基于 PCRE2 引擎）编译和执行正则匹配，\n通过 globalMatch() 获取所有匹配结果和捕获分组，实时高亮显示。",
        "支持常用正则语法，可查看捕获分组详情。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Compression"] = {
        "图片压缩",
        "压缩图片文件大小，支持调整质量和分辨率。",
        "1. 选择或拖入图片文件\n2. 设置压缩质量参数\n3. 点击\"压缩\"并保存结果",
        "使用 Qt QImage 加载图片，通过 QImageWriter 设置 quality 参数控制压缩质量，\n调整图片分辨率后以指定质量等级保存，实现文件体积压缩。",
        "支持 JPG、PNG 等常见格式，压缩率与画质需权衡。",
        "Windows / macOS / Linux"
    };

    s_helpData["Favicon Production"] = {
        "Favicon 生成",
        "将图片转换为网站图标（favicon.ico），支持多种尺寸。",
        "1. 选择源图片文件\n2. 选择需要生成的尺寸\n3. 点击\"生成\"导出 ico 文件",
        "使用 Qt QImage 加载源图片，通过 scaled() 缩放到多种尺寸（16/32/48/64/128/256），\n将各尺寸图像数据按 ICO 文件格式规范写入输出文件。",
        "建议使用正方形图片作为源图，以获得最佳效果。",
        "Windows / macOS / Linux"
    };

    s_helpData["Color Tools"] = {
        "颜色工具",
        "提供颜色选择、格式转换（HEX/RGB/HSL）和调色板功能。",
        "1. 通过拾色器选择颜色\n2. 查看不同格式的颜色值\n3. 可复制所需格式的颜色代码",
        "使用 Qt QColorDialog 提供拾色器界面，通过 QColor 类的\nname()、red()/green()/blue()、hslHue()/hslSaturation()/lightness() 等方法实现颜色格式互转。",
        "支持 HEX、RGB、HSL 等多种颜色格式互转。",
        "Windows / macOS / Linux"
    };

    s_helpData["Mobile Location"] = {
        "手机号归属地",
        "查询手机号码的归属地和运营商信息。",
        "1. 输入手机号码\n2. 点击\"查询\"\n3. 查看归属地和运营商信息",
        "基于离线号段数据库进行本地查询，通过手机号前缀匹配号段表，\n获取对应的省市和运营商信息，无需网络连接。",
        "仅支持中国大陆手机号查询，数据可能存在延迟。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTML Special Character Table"] = {
        "HTML 特殊字符表",
        "提供 HTML 特殊字符和实体编码的对照表，方便查找和复制。",
        "1. 浏览字符列表或搜索特定字符\n2. 点击字符即可复制对应的 HTML 实体编码",
        "基于静态查找表存储 HTML 实体名称、Unicode 码点和字符描述的对应关系，\n通过 QTableWidget 展示并支持关键词过滤搜索。",
        "包含常用 HTML 实体、数学符号、箭头等特殊字符。",
        "Windows / macOS / Linux"
    };

    s_helpData["Torrent File Analysis"] = {
        "种子文件分析",
        "解析 .torrent 文件，查看其中包含的文件列表和元数据信息。",
        "1. 选择或拖入 .torrent 文件\n2. 查看文件列表、大小和哈希信息",
        "实现自定义 Bencoding 解码器解析 .torrent 文件的二进制格式，\n提取 info 字典中的文件名、大小、分片哈希等元数据信息。",
        "仅分析种子文件结构，不提供下载功能。",
        "Windows / macOS / Linux"
    };

    s_helpData["Zip Code Query"] = {
        "邮编查询",
        "查询中国邮政编码对应的地区信息。",
        "1. 输入邮政编码或地区名称\n2. 点击\"查询\"获取结果",
        "基于离线邮编数据库进行本地查询，通过邮编或地区名称匹配对应记录，\n无需网络连接即可获取查询结果。",
        "数据基于中国邮政编码体系。",
        "Windows / macOS / Linux"
    };

    s_helpData["QR Code Generator"] = {
        "二维码生成器",
        "根据输入的文本或链接生成二维码图片。",
        "1. 输入文本或 URL\n2. 设置二维码尺寸和纠错级别\n3. 点击\"生成\"并保存图片",
        "使用 QR Code 编码算法生成二维码矩阵数据，\n通过 Qt QPainter 将矩阵逐像素绘制为图片，支持不同纠错级别。",
        "支持自定义尺寸，可导出为 PNG 图片文件。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Text Recognition"] = {
        "图片文字识别",
        "识别图片中的文字内容（OCR），提取文本信息。",
        "1. 选择或拖入包含文字的图片\n2. 点击\"识别\"开始处理\n3. 在结果区域查看和复制识别的文字",
        "使用 Qt QImage 加载图片，调用 OCR 引擎对图像进行文字识别，\n提取识别结果并展示在文本编辑区域中。",
        "识别准确率取决于图片清晰度，建议使用高清图片。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Hash Calculation"] = {
        "文件哈希计算",
        "计算文件的 MD5、SHA1、SHA256 等哈希值，用于校验文件完整性。",
        "1. 选择或拖入文件\n2. 选择哈希算法\n3. 查看计算结果",
        "使用 Qt QCryptographicHash 类支持 MD5、SHA-1、SHA-256、SHA-512 等算法，\n分块读取文件数据调用 addData() 增量计算，最终通过 result() 获取哈希值。",
        "支持 MD5、SHA1、SHA256 等多种算法，大文件计算可能需要较长时间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Barcode Generator"] = {
        "条形码生成器",
        "根据输入数据生成标准条形码图片。",
        "1. 输入条形码数据\n2. 选择条形码类型（Code128、EAN13 等）\n3. 点击\"生成\"并保存",
        "根据条形码编码规范（Code128、EAN13 等）将数据转换为条纹宽度序列，\n通过 Qt QPainter 绘制黑白条纹图案生成条形码图片。",
        "不同类型的条形码对输入数据格式有要求，请注意规范。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Format Conversion"] = {
        "图片格式转换",
        "在不同图片格式之间进行转换，如 PNG、JPG、BMP、WebP 等。",
        "1. 选择或拖入源图片\n2. 选择目标格式\n3. 点击\"转换\"并保存",
        "使用 Qt QImage 加载源图片（自动识别格式），\n通过 QImage::save() 指定目标格式名称参数输出为不同格式的图片文件。",
        "支持批量转换，转换过程中可能会有质量损失（如 PNG 转 JPG）。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTTP Status Code"] = {
        "HTTP 状态码",
        "查看 HTTP 状态码及其含义的参考手册。",
        "1. 浏览状态码列表\n2. 搜索特定状态码\n3. 查看详细说明",
        "基于静态查找表存储所有标准 HTTP 状态码及其含义描述，\n通过 QTableWidget 展示列表并支持关键词搜索过滤。",
        "涵盖 1xx~5xx 全部标准 HTTP 状态码。",
        "Windows / macOS / Linux"
    };

    s_helpData["Crontab Time Calculation"] = {
        "Crontab 时间计算",
        "解析 Crontab 表达式，计算并显示未来的执行时间。",
        "1. 输入 Crontab 表达式（如 */5 * * * *）\n2. 查看表达式含义说明\n3. 查看未来 N 次执行时间",
        "实现自定义 Cron 表达式解析器，将五段式表达式拆分为分/时/日/月/周字段，\n结合 QDateTime 从当前时间向后逐分钟推算匹配的执行时间点。",
        "支持标准五段式 Crontab 语法。",
        "Windows / macOS / Linux"
    };

    s_helpData["Text Encryption And Decryption"] = {
        "文本加解密",
        "对文本进行加密和解密操作，支持多种加密算法。",
        "1. 输入明文或密文\n2. 选择加密算法和密钥\n3. 点击\"加密\"或\"解密\"",
        "哈希功能使用 Qt QCryptographicHash 支持 MD5/SHA 系列算法，\n简单加密采用自定义 XOR/Caesar 移位算法实现对称加解密。",
        "请妥善保管密钥，丢失密钥将无法解密数据。",
        "Windows / macOS / Linux"
    };

    s_helpData["UUID Generator"] = {
        "UUID 生成器",
        "生成通用唯一标识符（UUID），支持多种版本。",
        "1. 选择 UUID 版本\n2. 设置生成数量\n3. 点击\"生成\"获取 UUID",
        "使用 Qt QUuid::createUuid() 生成符合 RFC 4122 标准的 UUID v4，\n基于系统随机数源生成 128 位全局唯一标识符。",
        "UUID v4 为随机生成，碰撞概率极低，适合大多数场景。",
        "Windows / macOS / Linux"
    };

    s_helpData["OpenCV Demo"] = {
        "OpenCV 演示",
        "展示 OpenCV 图像处理的基本功能和效果演示。",
        "1. 选择演示项目\n2. 加载图片或使用示例图片\n3. 查看处理效果",
        "使用 OpenCV 库（cv::Mat、cv::imread 等）进行图像处理，\n通过 cv::cvtColor、cv::GaussianBlur 等函数展示滤波、色彩转换等基础效果。",
        "需要编译时启用 WITH_OPENCV 选项。",
        "Windows / macOS / Linux"
    };

    s_helpData["OpenCV Image Processor"] = {
        "OpenCV 图像处理器",
        "使用 OpenCV 对图片进行高级处理，如滤波、边缘检测、变换等。",
        "1. 加载图片\n2. 选择处理算法和参数\n3. 预览效果并保存结果",
        "使用 OpenCV 库（cv::Mat、cv::imread）加载图像，\n调用 cv::Canny、cv::threshold、cv::warpAffine 等函数实现边缘检测、阈值分割、几何变换等高级处理。",
        "需要编译时启用 WITH_OPENCV 选项，处理大图可能较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["SSH Client"] = {
        "SSH 客户端",
        "通过 SSH 协议连接远程服务器，进行安全的远程终端操作。",
        "1. 输入服务器地址、端口、用户名和密码/密钥\n2. 点击\"连接\"建立 SSH 会话\n3. 在终端中输入命令操作",
        "基于 libssh 库（libssh/libssh.h）实现 SSH 协议，通过 ssh_session 建立加密连接，\nssh_channel 创建交互通道，支持 sftp_session 进行文件传输。",
        "需要编译时启用 WITH_LIBSSH 选项。支持密码和密钥认证。",
        "Windows / macOS / Linux"
    };

    s_helpData["FTP Client"] = {
        "FTP 客户端",
        "连接 FTP 服务器，进行文件的上传和下载管理。",
        "1. 输入 FTP 服务器地址和认证信息\n2. 点击\"连接\"登录服务器\n3. 浏览目录、上传或下载文件",
        "使用 Qt QTcpSocket 实现自定义 FTP 协议客户端，\n通过控制连接发送 FTP 命令，数据连接传输文件，支持主动和被动模式。",
        "支持主动和被动模式，大文件传输请确保网络稳定。",
        "Windows / macOS / Linux"
    };

    s_helpData["FTP Server"] = {
        "FTP 服务器",
        "在本机启动一个简易 FTP 服务器，方便局域网文件共享。",
        "1. 设置共享目录和端口号\n2. 配置访问权限\n3. 点击\"启动\"开始服务",
        "使用 Qt QTcpServer 监听端口接受连接，通过 QTcpSocket 实现自定义 FTP 协议服务端，\n处理 FTP 命令（USER/PASS/LIST/RETR/STOR 等）并管理数据传输。",
        "仅建议在受信任的局域网内使用，注意防火墙端口放行。",
        "Windows / macOS / Linux"
    };

    s_helpData["Camera Tool"] = {
        "摄像头工具",
        "调用本机摄像头进行预览和拍照。",
        "1. 选择摄像头设备\n2. 点击\"开启\"预览画面\n3. 点击\"拍照\"捕获当前画面",
        "使用 Qt Multimedia 模块，通过 QCamera 控制摄像头设备，\nQMediaCaptureSession 管理捕获会话，QImageCapture 实现拍照功能。",
        "需要编译时启用 WITH_QT_MULTIMEDIA 选项，需要摄像头设备。",
        "Windows / macOS / Linux"
    };

    s_helpData["Terminal Tool"] = {
        "终端工具",
        "内嵌终端模拟器，可在应用内直接执行系统命令。",
        "1. 在命令行中输入命令\n2. 按回车执行\n3. 查看输出结果",
        "通过 QProcess 启动系统 Shell 进程执行命令，\n读取标准输出和标准错误流并实时显示在终端界面中。",
        "命令执行权限与当前用户一致，请谨慎使用危险命令。",
        "Windows / macOS / Linux"
    };

    s_helpData["Traceroute Tool"] = {
        "路由追踪工具",
        "追踪数据包到目标主机的路由路径，显示每一跳的延迟。",
        "1. 输入目标主机地址或域名\n2. 点击\"开始追踪\"\n3. 查看路由路径和各节点延迟",
        "通过原始套接字（raw socket）发送 ICMP 报文并逐跳递增 TTL 值，\n接收超时响应获取每跳路由节点 IP 和延迟，可选使用 libpcap 抓包辅助分析。",
        "某些路由节点可能不响应，显示为超时属正常现象。",
        "Windows / macOS / Linux"
    };

    s_helpData["Route Test Tool"] = {
        "路由测试工具",
        "测试本机路由表和网络路由配置是否正常。",
        "1. 选择测试类型\n2. 输入目标地址\n3. 点击\"测试\"查看路由信息",
        "通过 QProcess 调用系统路由命令（route/netstat -r）获取路由表信息，\n解析输出结果展示网关、接口和路由规则等网络配置。",
        "可用于排查网络不通问题。",
        "Windows / macOS / Linux"
    };

    s_helpData["System Info Tool"] = {
        "系统信息工具",
        "显示当前系统的硬件和软件详细信息。",
        "1. 打开工具即可查看系统信息\n2. 包括 CPU、内存、操作系统、网络等信息",
        "使用 Qt QSysInfo 获取跨平台系统信息，\nWindows 上调用 Windows API，macOS 上调用 sysctl 获取 CPU、内存等硬件详情。",
        "部分硬件信息可能因系统权限限制而无法获取。",
        "Windows / macOS / Linux"
    };

    s_helpData["Rich Text Notepad"] = {
        "富文本记事本",
        "支持富文本编辑的记事本，可设置字体、颜色、插入图片等。",
        "1. 在编辑区域输入文本\n2. 使用工具栏设置格式\n3. 保存为文件",
        "基于 Qt QTextEdit 实现富文本编辑，通过 QTextCharFormat 和 QTextCursor\n设置字体、颜色、对齐等格式，支持 HTML 富文本的加载和保存。",
        "支持常见的富文本格式化操作，可导出为多种格式。",
        "Windows / macOS / Linux"
    };

    s_helpData["Media Manager"] = {
        "媒体管理器",
        "管理和浏览本地媒体文件，支持图片和视频预览。",
        "1. 选择媒体文件目录\n2. 浏览和筛选媒体文件\n3. 预览或打开文件",
        "使用 SQLite 数据库存储媒体文件元数据（路径、分类、标签），\n通过 Qt QMediaPlayer 实现视频播放预览，QImage/QPixmap 展示图片缩略图。",
        "需要编译时启用 WITH_QT_MULTIMEDIA 选项以支持视频预览。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Watermark"] = {
        "图片水印",
        "为图片添加文字或图片水印，保护图片版权。",
        "1. 选择或拖入图片\n2. 设置水印文字或水印图片\n3. 调整位置、透明度等参数\n4. 保存结果",
        "使用 Qt QPainter 在 QImage 上叠加绘制水印内容，\n通过 setOpacity() 控制透明度，drawText()/drawImage() 绘制文字或图片水印。",
        "支持批量添加水印，可自定义水印样式和位置。",
        "Windows / macOS / Linux"
    };

    s_helpData["WHOIS Tool"] = {
        "WHOIS 查询",
        "查询域名的 WHOIS 注册信息，包括注册商、到期时间等。",
        "1. 输入域名\n2. 点击\"查询\"\n3. 查看 WHOIS 详细信息",
        "使用 Qt QTcpSocket 连接 WHOIS 服务器（端口 43），\n发送域名查询请求并接收返回的注册信息文本。",
        "查询需要网络连接，部分域名信息可能受隐私保护。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Compare Tool"] = {
        "文件比较工具",
        "对比两个文件的差异，高亮显示不同之处。",
        "1. 选择两个要比较的文件\n2. 点击\"比较\"\n3. 查看差异高亮结果",
        "采用逐行对比的 diff 算法计算两个文件的差异，\n通过 QTextEdit 高亮显示新增、删除和修改的行。",
        "支持文本文件对比，大文件比较可能需要较长时间。",
        "Windows / macOS / Linux"
    };

    s_helpData["Blood Type Tool"] = {
        "血型工具",
        "查询父母血型组合下子女可能的血型及概率。",
        "1. 选择父亲血型\n2. 选择母亲血型\n3. 查看子女可能的血型和概率",
        "基于 ABO 血型遗传学的简单遗传算法，根据父母表现型推算可能的基因型组合，\n计算子女各血型的出现概率。",
        "基于 ABO 和 Rh 血型系统的遗传规律计算。",
        "Windows / macOS / Linux"
    };

    s_helpData["Port Scanner"] = {
        "端口扫描器",
        "扫描目标主机的端口开放状态。",
        "1. 输入目标主机地址\n2. 设置端口范围\n3. 点击\"扫描\"查看结果",
        "通过 QProcess 调用系统命令（Windows: netstat，macOS: lsof，Linux: netstat）\n获取端口占用信息，或使用 QTcpSocket 尝试连接目标端口检测开放状态。",
        "请在授权范围内使用，扫描大量端口可能较慢。",
        "Windows / macOS / Linux"
    };

    s_helpData["Key Remapper"] = {
        "按键映射",
        "重新映射键盘按键，将一个按键映射为另一个按键。",
        "1. 选择原始按键\n2. 设置映射目标按键\n3. 启用映射",
        "通过 Qt 全局键盘钩子拦截按键事件，在事件过滤器中将原始按键\n替换为映射目标按键后重新分发事件。",
        "映射仅在应用运行时生效，关闭后恢复原按键功能。",
        "Windows / macOS / Linux"
    };

    s_helpData["Chinese Copybook"] = {
        "中文字帖",
        "生成中文书法练习字帖，可自定义内容和样式。",
        "1. 输入要练习的文字\n2. 选择字体和格式\n3. 生成并打印字帖",
        "使用 Qt QPainter + QPrinter 生成 PDF 格式字帖，\n通过 QPainter 绘制田字格/米字格和汉字，支持自定义字体和版式。",
        "支持多种字体和田字格/米字格样式。",
        "Windows / macOS / Linux"
    };

    s_helpData["HTTP Client"] = {
        "HTTP 客户端",
        "发送 HTTP 请求，支持 GET、POST 等多种方法，方便接口调试。",
        "1. 输入请求 URL\n2. 选择请求方法，配置请求头和请求体\n3. 点击\"发送\"查看响应结果",
        "使用 Qt QNetworkAccessManager 发送 HTTP 请求，通过 QNetworkRequest 设置请求头，\nQNetworkReply 异步接收响应数据，支持 GET/POST/PUT/DELETE 等方法。",
        "支持自定义请求头、请求体和认证信息。",
        "Windows / macOS / Linux"
    };

    s_helpData["File Search"] = {
        "文件搜索",
        "在指定目录中快速搜索文件，支持文件名和内容匹配。",
        "1. 选择搜索目录\n2. 输入搜索关键词或通配符\n3. 点击\"搜索\"查看结果",
        "使用 Qt QDirIterator 进行递归目录遍历，\n通过文件名通配符匹配和文本内容搜索定位目标文件。",
        "支持递归搜索子目录，文件数量较多时请耐心等待。",
        "Windows / macOS / Linux"
    };

    s_helpData["Character Counter"] = {
        "字符统计",
        "统计文本的字符数、单词数、行数等信息。",
        "1. 在输入框中粘贴或输入文本\n2. 实时查看字符、单词、行数统计",
        "通过遍历 QString 逐字符统计，利用 QChar 的 isLetter()、isDigit()、isSpace() 等方法\n分类统计中英文字符、数字、空格和行数。",
        "支持中英文混合文本统计，包含字节数统计。",
        "Windows / macOS / Linux"
    };

    s_helpData["Images to PDF"] = {
        "图片转 PDF",
        "将多张图片合并为一个 PDF 文件。",
        "1. 添加图片文件\n2. 拖拽调整图片顺序\n3. 点击\"生成 PDF\"保存",
        "使用 Qt QPdfWriter 创建 PDF 文件，通过 QPainter 将每张图片绘制到独立页面，\n每张图片对应一页，自动适配页面尺寸。",
        "支持 JPG、PNG 等常见图片格式，注意图片数量对文件大小的影响。",
        "Windows / macOS / Linux"
    };

    s_helpData["Image Collection"] = {
        "图片收藏",
        "管理和收藏图片，方便分类浏览和查找。",
        "1. 添加图片到收藏\n2. 创建分类管理\n3. 浏览和搜索收藏的图片",
        "使用 SQLite 数据库存储图片元数据（路径、分类、标签等），\n通过文件拷贝方式保存图片，QImage/QPixmap 生成缩略图用于浏览展示。",
        "图片以引用方式管理，不会复制原文件。",
        "Windows / macOS / Linux"
    };

    s_helpData["Calculator"] = {
        "计算器",
        "提供基础和科学计算功能。",
        "1. 输入数学表达式\n2. 点击\"=\"或按回车计算结果",
        "实现自定义数学表达式解析器，支持运算符优先级和括号嵌套，\n通过递归下降或调度场算法解析并计算四则运算和数学函数表达式。",
        "支持四则运算、括号和常用数学函数。",
        "Windows / macOS / Linux"
    };

    s_helpData["Admin Rights Tool"] = {
        "管理员权限工具",
        "以管理员权限执行特定系统操作。",
        "1. 选择需要提权执行的操作\n2. 确认操作后以管理员身份执行",
        "Windows 上通过 ShellExecuteEx 以 runas 动词提升权限，\nmacOS 上通过 osascript 调用 AppleScript 请求管理员授权执行操作。",
        "操作需要管理员权限，请谨慎使用。",
        "Windows"
    };

    s_helpData["Screen Capture"] = {
        "截图工具",
        "截取屏幕画面，支持区域选择和全屏截图。",
        "1. 按快捷键或点击按钮启动截图\n2. 拖拽选择截图区域\n3. 保存或复制到剪贴板",
        "Windows 上使用 GDI API（BitBlt、GetDC）捕获屏幕像素数据，\n结合自定义截图库实现区域选择和全屏截取功能。",
        "支持快捷键触发，截图自动保存到剪贴板。",
        "Windows"
    };

    s_helpData["Windows Settings"] = {
        "Windows 设置",
        "快速访问 Windows 系统常用设置项。",
        "1. 浏览设置分类列表\n2. 点击对应项目快速打开系统设置",
        "通过 Windows Registry API 读取系统配置信息，\n使用 ShellExecuteEx 调用系统设置面板（ms-settings: URI）快速打开对应设置页面。",
        "仅在 Windows 系统上可用。",
        "Windows"
    };
}
