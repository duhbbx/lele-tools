#include "toollist.h"

#include "../main-widget/mainwindow.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSizePolicy>
#include <QMap>
#include <QDebug>

#include "module-meta.h"

ToolList::ToolList(MainWindow* mainWindow, QWidget* parent) : QWidget(parent) {
    this->mainWindow = mainWindow;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);


    // 搜索区域
    auto* searchWidget = new QWidget;
    searchWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* searchLayout = new QVBoxLayout(searchWidget);
    searchLayout->setContentsMargins(5, 0, 5, 0);
    searchLayout->setSpacing(5);

    searchLineEdit = new QLineEdit;
    searchLineEdit->setPlaceholderText(tr("搜索工具..."));
    searchLineEdit->setFixedHeight(30);
    searchLineEdit->setStyleSheet("border: 1px solid #ccc; border-radius: 0px; padding: 5px;");

    searchLayout->addWidget(searchLineEdit);
    // 搜索区域不拉伸，保持固定大小
    layout->addWidget(searchWidget, 0);

    // 工具列表
    listWidget = new QListWidget;
    listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    listWidget->setStyleSheet(
        "QListWidget {"
        "    border: none;"
        "    background-color: transparent;"
        "    outline: none;"
        "    font-size: 10pt;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    margin: 2px 5px;"
        "    border-radius: 0px;"
        "    border: none;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #e0e7ff;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3b82f6;"
        "    color: white;"
        "}"
        "QScrollBar:vertical {"
        "    background: #f0f0f0;"
        "    width: 8px;"
        "    border-radius: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #c0c0c0;"
        "    border-radius: 0px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {"
        "    background: none;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
    );

    // 按使用频率排序工具后添加到列表
    sortToolsByUsage();

    // 设置拉伸因子为1，让列表填满剩余空间
    layout->addWidget(listWidget, 1);

    connect(listWidget, &QListWidget::itemClicked, mainWindow, &MainWindow::itemClickedSlot);

    // 设置搜索功能
    setupSearchFunctionality();

    // 设置大小策略，确保组件能填满父容器
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->setLayout(layout);
}

void ToolList::setupSearchFunctionality() {
    // 连接搜索框的文本变化信号到过滤槽函数
    connect(searchLineEdit, &QLineEdit::textChanged, this, &ToolList::filterTools);
}

void ToolList::filterTools(const QString& text) const {
    // 如果搜索文本为空，显示所有项目
    if (text.isEmpty()) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setHidden(false);
        }
        return;
    }

    // 根据搜索文本过滤项目（不区分大小写）
    const QString searchText = text.toLower();
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        QString itemText = item->text().toLower();

        // 如果项目文本包含搜索文本，则显示；否则隐藏
        const bool shouldShow = itemText.contains(searchText);
        item->setHidden(!shouldShow);
    }
}

void ToolList::sortToolsByUsage() const {
    // 显式标记工具名称用于翻译提取（这些tr()调用确保lupdate能找到所有字符串）
    static QStringList translationKeys = {
        tr("XML Formatter"),
        tr("JSON Formatter"),
        tr("YAML Formatter"),
        tr("Date Time Util"),
        tr("Base64 Encode Decode"),
        tr("Regex Content Generator"),
        tr("Random Password Generator"),
        tr("Telnet Tool"),
        tr("Windows Settings"),
        tr("Hosts Editor (Table)"),
        tr("Hosts Editor (Text)"),
        tr("Ping Tool"),
        tr("Network Scanner"),
        tr("Database Tool"),
        tr("IP Lookup Tool"),
        tr("PDF Merge"),
        tr("Regex Test Tool"),
        tr("Image Compression"),
        tr("Favicon Production"),
        tr("Color Tools"),
        tr("Mobile Location"),
        tr("HTML Special Character Table"),
        tr("Torrent File Analysis"),
        tr("Zip Code Query"),
        tr("QR Code Generator"),
        tr("Image Text Recognition"),
        tr("File Hash Calculation"),
        tr("Barcode Generator"),
        tr("Image Format Conversion"),
        tr("HTTP Status Code"),
        tr("Crontab Time Calculation"),
        tr("Text Encryption And Decryption"),
        tr("UUID Generator"),
        tr("OpenCV Demo"),
        tr("OpenCV Image Processor"),
        tr("SSH Client"),
        tr("FTP Client"),
        tr("FTP Server"),
        tr("Camera Tool"),
        tr("Terminal Tool"),
        tr("Traceroute Tool"),
        tr("Route Test Tool"),
        tr("System Info Tool"),
        tr("Rich Text Notepad"),
        tr("Media Manager"),
        tr("Image Watermark"),
        tr("WHOIS Tool"),
        tr("File Compare Tool"),
        tr("Blood Type Tool"),
        tr("Port Scanner"),
        tr("Key Remapper"),
        tr("Chinese Copybook")
    };
    Q_UNUSED(translationKeys); // 避免编译器警告

    // 获取最近一周的使用统计
    QList<ToolUsageStats> usageStats = ToolUsageTracker::instance()->getWeeklyUsageStats();

    // 创建使用次数映射表
    QMap<QString, int> usageCountMap;
    for (const auto& stat : usageStats) {
        usageCountMap[stat.toolName] = stat.usageCount;
        qDebug() << "Tool usage:" << stat.toolName << "count:" << stat.usageCount;
    }

    // 创建工具项目列表，包含使用次数信息
    struct ToolItem {
        QString icon;
        QString title;
        QString className;
        int usageCount;

        bool operator<(const ToolItem& other) const {
            // 首先按使用次数降序排序
            if (usageCount != other.usageCount) {
                return usageCount > other.usageCount;
            }
            // 使用次数相同时按标题字母序排序
            return title < other.title;
        }
    };

    QList<ToolItem> toolItems;

    // 从原始数据创建工具项目列表
    for (const auto& [icon, titleKey, className] : moduleMetaArray) {
        ToolItem item;
        item.icon = icon;
        item.title = tr(titleKey.toUtf8().constData());
        item.className = className;
        item.usageCount = usageCountMap.value(className, 0); // 默认为0
        toolItems.append(item);
    }

    // 按使用频率排序
    std::sort(toolItems.begin(), toolItems.end());

    // 添加排序后的项目到列表
    for (const auto& toolItem : toolItems) {
        auto* item = new QListWidgetItem(QIcon(toolItem.icon), toolItem.title);
        item->setData(Qt::UserRole, QVariant(toolItem.className));
        item->setData(Qt::DisplayRole, QVariant(toolItem.title));

        // 如果有使用记录，在标题后显示使用次数
        if (toolItem.usageCount > 0) {
            QString titleWithCount = QString("%1 (%2次)").arg(toolItem.title).arg(toolItem.usageCount);
            item->setText(titleWithCount);
        }

        listWidget->addItem(item);
    }

    qDebug() << "Tools sorted by usage frequency. Total tools:" << toolItems.size();
}

ToolList::~ToolList() = default;
