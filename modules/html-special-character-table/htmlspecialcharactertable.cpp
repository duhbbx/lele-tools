#include "htmlspecialcharactertable.h"

REGISTER_DYNAMICOBJECT(HtmlSpecialCharacterTable);

// CharacterCard 实现
CharacterCard::CharacterCard(const HtmlCharacter &character, QWidget *parent)
    : QFrame(parent)
    , m_character(character)
    , m_highlighted(false)
    , m_hovered(false)
{
    setFixedSize(HtmlSpecialCharacterTable::CARD_WIDTH, HtmlSpecialCharacterTable::CARD_HEIGHT);
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setLineWidth(1);
    setCursor(Qt::PointingHandCursor);
    
    setupUI();
}

void CharacterCard::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // 字符显示
    charLabel = new QLabel(m_character.character);
    charLabel->setAlignment(Qt::AlignCenter);
    charLabel->setStyleSheet("font-size: 24pt; font-weight: bold; color: #2c3e50;");
    charLabel->setMinimumHeight(40);
    mainLayout->addWidget(charLabel);
    
    // HTML实体
    entityLabel = new QLabel(m_character.entity);
    entityLabel->setAlignment(Qt::AlignCenter);
    entityLabel->setStyleSheet("font-family: 'Consolas', monospace; font-size: 10pt; color: #e74c3c; font-weight: bold;");
    entityLabel->setWordWrap(true);
    mainLayout->addWidget(entityLabel);
    
    // 描述
    descLabel = new QLabel(m_character.description);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("font-size: 9pt; color: #7f8c8d;");
    descLabel->setWordWrap(true);
    descLabel->setMaximumHeight(30);
    mainLayout->addWidget(descLabel);
    
    // 操作按钮
    buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(2);
    
    copyCharBtn = new QPushButton("字符");
    copyCharBtn->setMaximumHeight(20);
    copyCharBtn->setStyleSheet("QPushButton { font-size: 8pt; padding: 2px 4px; background-color: #3498db; color: white; border: none; border-radius: 2px; } QPushButton:hover { background-color: #2980b9; }");
    connect(copyCharBtn, &QPushButton::clicked, [this]() {
        emit copyRequested(m_character.character, "字符");
    });
    
    copyEntityBtn = new QPushButton("实体");
    copyEntityBtn->setMaximumHeight(20);
    copyEntityBtn->setStyleSheet("QPushButton { font-size: 8pt; padding: 2px 4px; background-color: #e74c3c; color: white; border: none; border-radius: 2px; } QPushButton:hover { background-color: #c0392b; }");
    connect(copyEntityBtn, &QPushButton::clicked, [this]() {
        emit copyRequested(m_character.entity, "HTML实体");
    });
    
    copyNumericBtn = new QPushButton("数字");
    copyNumericBtn->setMaximumHeight(20);
    copyNumericBtn->setStyleSheet("QPushButton { font-size: 8pt; padding: 2px 4px; background-color: #f39c12; color: white; border: none; border-radius: 2px; } QPushButton:hover { background-color: #e67e22; }");
    connect(copyNumericBtn, &QPushButton::clicked, [this]() {
        emit copyRequested(m_character.numeric, "数字实体");
    });
    
    buttonLayout->addWidget(copyCharBtn);
    buttonLayout->addWidget(copyEntityBtn);
    buttonLayout->addWidget(copyNumericBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    setStyleSheet(R"(
        CharacterCard {
            background-color: #ffffff;
            border: 1px solid #bdc3c7;
            border-radius: 8px;
        }
        CharacterCard:hover {
            border-color: #3498db;
            background-color: #ecf0f1;
        }
    )");
}

void CharacterCard::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void CharacterCard::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    if (m_highlighted || m_hovered) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QPen pen(m_highlighted ? QColor("#e74c3c") : QColor("#3498db"), 2);
        painter.setPen(pen);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    }
}

void CharacterCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit characterClicked(m_character);
    }
    QFrame::mousePressEvent(event);
}

void CharacterCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    
    // 显示工具提示
    QString tooltip = QString("字符: %1\nHTML实体: %2\n数字实体: %3\n十六进制: %4\nUnicode: U+%5\n描述: %6")
                     .arg(m_character.character)
                     .arg(m_character.entity)
                     .arg(m_character.numeric)
                     .arg(m_character.hex)
                     .arg(QString::number(m_character.unicode, 16).toUpper().rightJustified(4, '0'))
                     .arg(m_character.description);
    setToolTip(tooltip);
    
    QFrame::enterEvent(event);
}

void CharacterCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QFrame::leaveEvent(event);
}

// HtmlSpecialCharacterTable 主类实现
HtmlSpecialCharacterTable::HtmlSpecialCharacterTable() : QWidget(nullptr), DynamicObjectBase()
{
    m_currentSearchText = "";
    m_currentCategory = "全部";
    m_showFavoritesOnly = false;
    m_cardViewMode = true;
    
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300);
    
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(3000);
    
    setupUI();
    initializeCharacters();
    loadSettings();
    loadFavorites();
    
    // 连接信号槽
    connect(m_searchTimer, &QTimer::timeout, this, &HtmlSpecialCharacterTable::filterCharacters);
    connect(m_statusTimer, &QTimer::timeout, [this]() {
        statusLabel->setText("就绪");
    });
    
    connect(searchEdit, &QLineEdit::textChanged, this, &HtmlSpecialCharacterTable::onSearchTextChanged);
    connect(clearSearchBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onClearSearch);
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HtmlSpecialCharacterTable::onCategoryChanged);
    connect(favoritesOnlyCheck, &QCheckBox::toggled, this, &HtmlSpecialCharacterTable::onShowFavorites);
    connect(cardViewRadio, &QRadioButton::toggled, this, &HtmlSpecialCharacterTable::onViewModeChanged);
    connect(tableViewRadio, &QRadioButton::toggled, this, &HtmlSpecialCharacterTable::onViewModeChanged);
    
    connect(exportBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onExportList);
    connect(importBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onImportList);
    connect(showFavoritesBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onShowFavorites);
    connect(addToFavoritesBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onAddToFavorites);
    connect(removeFromFavoritesBtn, &QPushButton::clicked, this, &HtmlSpecialCharacterTable::onRemoveFromFavorites);
    
    // 初始显示
    filterCharacters();
}

HtmlSpecialCharacterTable::~HtmlSpecialCharacterTable()
{
    saveSettings();
    saveFavorites();
}

void HtmlSpecialCharacterTable::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    setupSearchArea();
    
    // 创建分割器
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    setupCharacterDisplay();
    setupDetailPanel();
    
    mainSplitter->addWidget(displayWidget);
    mainSplitter->addWidget(detailGroup);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({600, 300});
    
    mainLayout->addWidget(searchGroup);
    mainLayout->addWidget(mainSplitter);
    
    setupStatusArea();
    mainLayout->addLayout(buttonLayout);
    mainLayout->addLayout(statusLayout);
    
    // 应用样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 6px 12px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 11pt;
            font-weight: normal;
            min-width: 60px;
            background-color: #f8f9fa;
        }
        QPushButton:hover { 
            background-color: #e9ecef; 
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
        }
        QPushButton:disabled {
            background-color: #e9ecef;
            color: #6c757d;
            border-color: #dee2e6;
        }
        QGroupBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QLineEdit, QComboBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 6px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #80bdff;
            outline: 0;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
        QCheckBox, QRadioButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
        QTableWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            border: 2px solid #dee2e6;
            border-radius: 4px;
            font-size: 11pt;
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #dee2e6;
        }
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            padding: 8px;
            border: 1px solid #dee2e6;
            font-weight: bold;
        }
        QTextEdit {
            font-family: 'Consolas', 'Monaco', monospace;
            border: 2px solid #dee2e6;
            border-radius: 4px;
            font-size: 10pt;
            background-color: white;
        }
    )");
}

void HtmlSpecialCharacterTable::setupSearchArea()
{
    searchGroup = new QGroupBox("🔍 搜索和过滤");
    searchLayout = new QHBoxLayout(searchGroup);
    
    // 搜索框
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("搜索字符、实体或描述...");
    searchEdit->setMinimumWidth(200);
    searchLayout->addWidget(new QLabel("搜索:"));
    searchLayout->addWidget(searchEdit);
    
    clearSearchBtn = new QPushButton("🗑️ 清空");
    clearSearchBtn->setMaximumWidth(60);
    searchLayout->addWidget(clearSearchBtn);
    
    // 分类过滤
    searchLayout->addWidget(new QLabel("分类:"));
    categoryCombo = new QComboBox();
    categoryCombo->addItems({"全部", "基本符号", "标点符号", "数学符号", "货币符号", "箭头符号", "希腊字母", "特殊字符"});
    categoryCombo->setMinimumWidth(120);
    searchLayout->addWidget(categoryCombo);
    
    // 收藏过滤
    favoritesOnlyCheck = new QCheckBox("仅显示收藏");
    searchLayout->addWidget(favoritesOnlyCheck);
    
    searchLayout->addStretch();
    
    // 视图模式
    searchLayout->addWidget(new QLabel("视图:"));
    viewModeGroup = new QButtonGroup(this);
    cardViewRadio = new QRadioButton("卡片");
    tableViewRadio = new QRadioButton("表格");
    cardViewRadio->setChecked(true);
    viewModeGroup->addButton(cardViewRadio);
    viewModeGroup->addButton(tableViewRadio);
    searchLayout->addWidget(cardViewRadio);
    searchLayout->addWidget(tableViewRadio);
}

void HtmlSpecialCharacterTable::setupCharacterDisplay()
{
    displayWidget = new QWidget();
    QVBoxLayout *displayLayout = new QVBoxLayout(displayWidget);
    displayLayout->setContentsMargins(0, 0, 0, 0);
    
    // 卡片视图
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    cardContainer = new QWidget();
    cardLayout = new QGridLayout(cardContainer);
    cardLayout->setSpacing(10);
    cardLayout->setContentsMargins(10, 10, 10, 10);
    
    scrollArea->setWidget(cardContainer);
    
    // 表格视图
    characterTable = new QTableWidget();
    characterTable->setColumnCount(7);
    characterTable->setHorizontalHeaderLabels({"字符", "HTML实体", "数字实体", "十六进制", "Unicode", "描述", "分类"});
    characterTable->setAlternatingRowColors(true);
    characterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    characterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    characterTable->setSortingEnabled(true);
    characterTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    QHeaderView* header = characterTable->horizontalHeader();
    header->setStretchLastSection(true);
    characterTable->setColumnWidth(0, 60);   // 字符
    characterTable->setColumnWidth(1, 100);  // HTML实体
    characterTable->setColumnWidth(2, 80);   // 数字实体
    characterTable->setColumnWidth(3, 80);   // 十六进制
    characterTable->setColumnWidth(4, 80);   // Unicode
    characterTable->setColumnWidth(5, 200);  // 描述
    
    characterTable->setVisible(false);
    
    displayLayout->addWidget(scrollArea);
    displayLayout->addWidget(characterTable);
}

void HtmlSpecialCharacterTable::setupDetailPanel()
{
    detailGroup = new QGroupBox("📋 字符详情");
    detailLayout = new QVBoxLayout(detailGroup);
    
    // 字符显示
    detailCharLabel = new QLabel("选择一个字符");
    detailCharLabel->setAlignment(Qt::AlignCenter);
    detailCharLabel->setStyleSheet("font-size: 36pt; font-weight: bold; color: #2c3e50; min-height: 80px; background-color: #ecf0f1; border-radius: 8px; margin: 10px;");
    detailLayout->addWidget(detailCharLabel);
    
    // 详细信息
    QGridLayout *infoLayout = new QGridLayout();
    
    infoLayout->addWidget(new QLabel("HTML实体:"), 0, 0);
    detailEntityLabel = new QLabel("-");
    detailEntityLabel->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; color: #e74c3c;");
    infoLayout->addWidget(detailEntityLabel, 0, 1);
    
    infoLayout->addWidget(new QLabel("数字实体:"), 1, 0);
    detailNumericLabel = new QLabel("-");
    detailNumericLabel->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; color: #f39c12;");
    infoLayout->addWidget(detailNumericLabel, 1, 1);
    
    infoLayout->addWidget(new QLabel("十六进制:"), 2, 0);
    detailHexLabel = new QLabel("-");
    detailHexLabel->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; color: #9b59b6;");
    infoLayout->addWidget(detailHexLabel, 2, 1);
    
    infoLayout->addWidget(new QLabel("Unicode:"), 3, 0);
    detailUnicodeLabel = new QLabel("-");
    detailUnicodeLabel->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; color: #3498db;");
    infoLayout->addWidget(detailUnicodeLabel, 3, 1);
    
    infoLayout->addWidget(new QLabel("描述:"), 4, 0);
    detailDescLabel = new QLabel("-");
    detailDescLabel->setWordWrap(true);
    infoLayout->addWidget(detailDescLabel, 4, 1);
    
    infoLayout->addWidget(new QLabel("分类:"), 5, 0);
    detailCategoryLabel = new QLabel("-");
    detailCategoryLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    infoLayout->addWidget(detailCategoryLabel, 5, 1);
    
    detailLayout->addLayout(infoLayout);
    
    // 使用示例
    detailLayout->addWidget(new QLabel("使用示例:"));
    usageExample = new QTextEdit();
    usageExample->setMaximumHeight(80);
    usageExample->setReadOnly(true);
    detailLayout->addWidget(usageExample);
    
    // 操作按钮
    detailButtonLayout = new QHBoxLayout();
    
    copyCharDetailBtn = new QPushButton("📋 字符");
    copyCharDetailBtn->setStyleSheet("QPushButton { background-color: #3498db; color: white; } QPushButton:hover { background-color: #2980b9; }");
    detailButtonLayout->addWidget(copyCharDetailBtn);
    
    copyEntityDetailBtn = new QPushButton("📋 实体");
    copyEntityDetailBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; } QPushButton:hover { background-color: #c0392b; }");
    detailButtonLayout->addWidget(copyEntityDetailBtn);
    
    copyNumericDetailBtn = new QPushButton("📋 数字");
    copyNumericDetailBtn->setStyleSheet("QPushButton { background-color: #f39c12; color: white; } QPushButton:hover { background-color: #e67e22; }");
    detailButtonLayout->addWidget(copyNumericDetailBtn);
    
    copyHexDetailBtn = new QPushButton("📋 十六进制");
    copyHexDetailBtn->setStyleSheet("QPushButton { background-color: #9b59b6; color: white; } QPushButton:hover { background-color: #8e44ad; }");
    detailButtonLayout->addWidget(copyHexDetailBtn);
    
    detailLayout->addLayout(detailButtonLayout);
    
    // 收藏按钮
    QHBoxLayout *favoriteLayout = new QHBoxLayout();
    addToFavoritesBtn = new QPushButton("⭐ 添加到收藏");
    addToFavoritesBtn->setStyleSheet("QPushButton { background-color: #f1c40f; color: white; } QPushButton:hover { background-color: #f39c12; }");
    removeFromFavoritesBtn = new QPushButton("❌ 从收藏移除");
    removeFromFavoritesBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; } QPushButton:hover { background-color: #c0392b; }");
    
    favoriteLayout->addWidget(addToFavoritesBtn);
    favoriteLayout->addWidget(removeFromFavoritesBtn);
    detailLayout->addLayout(favoriteLayout);
    
    detailLayout->addStretch();
    
    // 连接详情面板按钮
    connect(copyCharDetailBtn, &QPushButton::clicked, [this]() {
        if (!m_currentCharacter.character.isEmpty()) {
            onCopyRequested(m_currentCharacter.character, "字符");
        }
    });
    connect(copyEntityDetailBtn, &QPushButton::clicked, [this]() {
        if (!m_currentCharacter.entity.isEmpty()) {
            onCopyRequested(m_currentCharacter.entity, "HTML实体");
        }
    });
    connect(copyNumericDetailBtn, &QPushButton::clicked, [this]() {
        if (!m_currentCharacter.numeric.isEmpty()) {
            onCopyRequested(m_currentCharacter.numeric, "数字实体");
        }
    });
    connect(copyHexDetailBtn, &QPushButton::clicked, [this]() {
        if (!m_currentCharacter.hex.isEmpty()) {
            onCopyRequested(m_currentCharacter.hex, "十六进制实体");
        }
    });
    
    clearDetailPanel();
}

void HtmlSpecialCharacterTable::setupStatusArea()
{
    // 操作按钮
    buttonLayout = new QHBoxLayout();
    
    exportBtn = new QPushButton("💾 导出列表");
    exportBtn->setStyleSheet("QPushButton { background-color: #27ae60; color: white; } QPushButton:hover { background-color: #229954; }");
    buttonLayout->addWidget(exportBtn);
    
    importBtn = new QPushButton("📁 导入列表");
    buttonLayout->addWidget(importBtn);
    
    showFavoritesBtn = new QPushButton("⭐ 显示收藏");
    showFavoritesBtn->setStyleSheet("QPushButton { background-color: #f1c40f; color: white; } QPushButton:hover { background-color: #f39c12; }");
    buttonLayout->addWidget(showFavoritesBtn);
    
    buttonLayout->addStretch();
    
    // 状态栏
    statusLayout = new QHBoxLayout();
    
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    
    countLabel = new QLabel("字符: 0");
    countLabel->setStyleSheet("color: #7f8c8d;");
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
    progressBar->setMaximumWidth(200);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(countLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
}

void HtmlSpecialCharacterTable::initializeCharacters()
{
    // 基本符号
    addCharacter("&", "&amp;", "&#38;", "&#x26;", "和号", "基本符号", 38);
    addCharacter("<", "&lt;", "&#60;", "&#x3C;", "小于号", "基本符号", 60);
    addCharacter(">", "&gt;", "&#62;", "&#x3E;", "大于号", "基本符号", 62);
    addCharacter("\"", "&quot;", "&#34;", "&#x22;", "双引号", "基本符号", 34);
    addCharacter("'", "&apos;", "&#39;", "&#x27;", "单引号", "基本符号", 39);
    addCharacter(" ", "&nbsp;", "&#160;", "&#xA0;", "不间断空格", "基本符号", 160);
    
    // 标点符号
    addCharacter("©", "&copy;", "&#169;", "&#xA9;", "版权符号", "标点符号", 169);
    addCharacter("®", "&reg;", "&#174;", "&#xAE;", "注册商标", "标点符号", 174);
    addCharacter("™", "&trade;", "&#8482;", "&#x2122;", "商标符号", "标点符号", 8482);
    addCharacter("§", "&sect;", "&#167;", "&#xA7;", "章节符号", "标点符号", 167);
    addCharacter("¶", "&para;", "&#182;", "&#xB6;", "段落符号", "标点符号", 182);
    addCharacter("†", "&dagger;", "&#8224;", "&#x2020;", "剑号", "标点符号", 8224);
    addCharacter("‡", "&Dagger;", "&#8225;", "&#x2021;", "双剑号", "标点符号", 8225);
    addCharacter("•", "&bull;", "&#8226;", "&#x2022;", "项目符号", "标点符号", 8226);
    addCharacter("…", "&hellip;", "&#8230;", "&#x2026;", "省略号", "标点符号", 8230);
    addCharacter("‰", "&permil;", "&#8240;", "&#x2030;", "千分号", "标点符号", 8240);
    addCharacter("′", "&prime;", "&#8242;", "&#x2032;", "分号(角度)", "标点符号", 8242);
    addCharacter("″", "&Prime;", "&#8243;", "&#x2033;", "秒号(角度)", "标点符号", 8243);
    
    // 数学符号
    addCharacter("±", "&plusmn;", "&#177;", "&#xB1;", "正负号", "数学符号", 177);
    addCharacter("×", "&times;", "&#215;", "&#xD7;", "乘号", "数学符号", 215);
    addCharacter("÷", "&divide;", "&#247;", "&#xF7;", "除号", "数学符号", 247);
    addCharacter("=", "=", "&#61;", "&#x3D;", "等号", "数学符号", 61);
    addCharacter("≠", "&ne;", "&#8800;", "&#x2260;", "不等号", "数学符号", 8800);
    addCharacter("≤", "&le;", "&#8804;", "&#x2264;", "小于等于", "数学符号", 8804);
    addCharacter("≥", "&ge;", "&#8805;", "&#x2265;", "大于等于", "数学符号", 8805);
    addCharacter("∞", "&infin;", "&#8734;", "&#x221E;", "无穷大", "数学符号", 8734);
    addCharacter("∑", "&sum;", "&#8721;", "&#x2211;", "求和符号", "数学符号", 8721);
    addCharacter("∏", "&prod;", "&#8719;", "&#x220F;", "乘积符号", "数学符号", 8719);
    addCharacter("∫", "&int;", "&#8747;", "&#x222B;", "积分符号", "数学符号", 8747);
    addCharacter("√", "&radic;", "&#8730;", "&#x221A;", "根号", "数学符号", 8730);
    addCharacter("∂", "&part;", "&#8706;", "&#x2202;", "偏微分", "数学符号", 8706);
    addCharacter("∆", "&Delta;", "&#8710;", "&#x2206;", "增量符号", "数学符号", 8710);
    addCharacter("∇", "&nabla;", "&#8711;", "&#x2207;", "梯度符号", "数学符号", 8711);
    
    // 货币符号
    addCharacter("¢", "&cent;", "&#162;", "&#xA2;", "美分", "货币符号", 162);
    addCharacter("£", "&pound;", "&#163;", "&#xA3;", "英镑", "货币符号", 163);
    addCharacter("¤", "&curren;", "&#164;", "&#xA4;", "通用货币符号", "货币符号", 164);
    addCharacter("¥", "&yen;", "&#165;", "&#xA5;", "日元/人民币", "货币符号", 165);
    addCharacter("€", "&euro;", "&#8364;", "&#x20AC;", "欧元", "货币符号", 8364);
    addCharacter("$", "$", "&#36;", "&#x24;", "美元", "货币符号", 36);
    
    // 箭头符号
    addCharacter("←", "&larr;", "&#8592;", "&#x2190;", "左箭头", "箭头符号", 8592);
    addCharacter("↑", "&uarr;", "&#8593;", "&#x2191;", "上箭头", "箭头符号", 8593);
    addCharacter("→", "&rarr;", "&#8594;", "&#x2192;", "右箭头", "箭头符号", 8594);
    addCharacter("↓", "&darr;", "&#8595;", "&#x2193;", "下箭头", "箭头符号", 8595);
    addCharacter("↔", "&harr;", "&#8596;", "&#x2194;", "左右箭头", "箭头符号", 8596);
    addCharacter("↕", "&varr;", "&#8597;", "&#x2195;", "上下箭头", "箭头符号", 8597);
    addCharacter("⇐", "&lArr;", "&#8656;", "&#x21D0;", "左双箭头", "箭头符号", 8656);
    addCharacter("⇑", "&uArr;", "&#8657;", "&#x21D1;", "上双箭头", "箭头符号", 8657);
    addCharacter("⇒", "&rArr;", "&#8658;", "&#x21D2;", "右双箭头", "箭头符号", 8658);
    addCharacter("⇓", "&dArr;", "&#8659;", "&#x21D3;", "下双箭头", "箭头符号", 8659);
    addCharacter("⇔", "&hArr;", "&#8660;", "&#x21D4;", "左右双箭头", "箭头符号", 8660);
    
    // 希腊字母
    addCharacter("α", "&alpha;", "&#945;", "&#x3B1;", "希腊小写字母阿尔法", "希腊字母", 945);
    addCharacter("β", "&beta;", "&#946;", "&#x3B2;", "希腊小写字母贝塔", "希腊字母", 946);
    addCharacter("γ", "&gamma;", "&#947;", "&#x3B3;", "希腊小写字母伽马", "希腊字母", 947);
    addCharacter("δ", "&delta;", "&#948;", "&#x3B4;", "希腊小写字母德尔塔", "希腊字母", 948);
    addCharacter("ε", "&epsilon;", "&#949;", "&#x3B5;", "希腊小写字母艾普西隆", "希腊字母", 949);
    addCharacter("ζ", "&zeta;", "&#950;", "&#x3B6;", "希腊小写字母泽塔", "希腊字母", 950);
    addCharacter("η", "&eta;", "&#951;", "&#x3B7;", "希腊小写字母埃塔", "希腊字母", 951);
    addCharacter("θ", "&theta;", "&#952;", "&#x3B8;", "希腊小写字母西塔", "希腊字母", 952);
    addCharacter("λ", "&lambda;", "&#955;", "&#x3BB;", "希腊小写字母兰布达", "希腊字母", 955);
    addCharacter("μ", "&mu;", "&#956;", "&#x3BC;", "希腊小写字母缪", "希腊字母", 956);
    addCharacter("π", "&pi;", "&#960;", "&#x3C0;", "希腊小写字母派", "希腊字母", 960);
    addCharacter("σ", "&sigma;", "&#963;", "&#x3C3;", "希腊小写字母西格马", "希腊字母", 963);
    addCharacter("τ", "&tau;", "&#964;", "&#x3C4;", "希腊小写字母陶", "希腊字母", 964);
    addCharacter("φ", "&phi;", "&#966;", "&#x3C6;", "希腊小写字母斐", "希腊字母", 966);
    addCharacter("ω", "&omega;", "&#969;", "&#x3C9;", "希腊小写字母欧米伽", "希腊字母", 969);
    
    // 希腊大写字母
    addCharacter("Α", "&Alpha;", "&#913;", "&#x391;", "希腊大写字母阿尔法", "希腊字母", 913);
    addCharacter("Β", "&Beta;", "&#914;", "&#x392;", "希腊大写字母贝塔", "希腊字母", 914);
    addCharacter("Γ", "&Gamma;", "&#915;", "&#x393;", "希腊大写字母伽马", "希腊字母", 915);
    addCharacter("Δ", "&Delta;", "&#916;", "&#x394;", "希腊大写字母德尔塔", "希腊字母", 916);
    addCharacter("Λ", "&Lambda;", "&#923;", "&#x39B;", "希腊大写字母兰布达", "希腊字母", 923);
    addCharacter("Π", "&Pi;", "&#928;", "&#x3A0;", "希腊大写字母派", "希腊字母", 928);
    addCharacter("Σ", "&Sigma;", "&#931;", "&#x3A3;", "希腊大写字母西格马", "希腊字母", 931);
    addCharacter("Φ", "&Phi;", "&#934;", "&#x3A6;", "希腊大写字母斐", "希腊字母", 934);
    addCharacter("Ω", "&Omega;", "&#937;", "&#x3A9;", "希腊大写字母欧米伽", "希腊字母", 937);
    
    // 特殊字符
    addCharacter("♠", "&spades;", "&#9824;", "&#x2660;", "黑桃", "特殊字符", 9824);
    addCharacter("♣", "&clubs;", "&#9827;", "&#x2663;", "梅花", "特殊字符", 9827);
    addCharacter("♥", "&hearts;", "&#9829;", "&#x2665;", "红心", "特殊字符", 9829);
    addCharacter("♦", "&diams;", "&#9830;", "&#x2666;", "方块", "特殊字符", 9830);
    addCharacter("★", "★", "&#9733;", "&#x2605;", "实心星星", "特殊字符", 9733);
    addCharacter("☆", "☆", "&#9734;", "&#x2606;", "空心星星", "特殊字符", 9734);
    addCharacter("♪", "♪", "&#9834;", "&#x266A;", "音符", "特殊字符", 9834);
    addCharacter("♫", "♫", "&#9835;", "&#x266B;", "双音符", "特殊字符", 9835);
    addCharacter("☀", "☀", "&#9728;", "&#x2600;", "太阳", "特殊字符", 9728);
    addCharacter("☁", "☁", "&#9729;", "&#x2601;", "云朵", "特殊字符", 9729);
    addCharacter("☂", "☂", "&#9730;", "&#x2602;", "雨伞", "特殊字符", 9730);
    addCharacter("☎", "☎", "&#9742;", "&#x260E;", "电话", "特殊字符", 9742);
    addCharacter("✓", "✓", "&#10003;", "&#x2713;", "对勾", "特殊字符", 10003);
    addCharacter("✗", "✗", "&#10007;", "&#x2717;", "叉号", "特殊字符", 10007);
    addCharacter("✉", "✉", "&#9993;", "&#x2709;", "信封", "特殊字符", 9993);
    addCharacter("✈", "✈", "&#9992;", "&#x2708;", "飞机", "特殊字符", 9992);
    
    statusLabel->setText(QString("已加载 %1 个HTML特殊字符").arg(m_allCharacters.size()));
}

void HtmlSpecialCharacterTable::addCharacter(const QString &ch, const QString &entity, const QString &numeric,
                                           const QString &hex, const QString &description, const QString &category, int unicode)
{
    HtmlCharacter character(ch, entity, numeric, hex, description, category, unicode);
    m_allCharacters.append(character);
}

// 槽函数实现
void HtmlSpecialCharacterTable::onSearchTextChanged()
{
    m_currentSearchText = searchEdit->text().trimmed();
    m_searchTimer->start();
}

void HtmlSpecialCharacterTable::onCategoryChanged()
{
    m_currentCategory = categoryCombo->currentText();
    filterCharacters();
}

void HtmlSpecialCharacterTable::onCharacterClicked(const HtmlCharacter &character)
{
    m_currentCharacter = character;
    updateDetailPanel(character);
}

void HtmlSpecialCharacterTable::onCopyRequested(const QString &text, const QString &type)
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    
    statusLabel->setText(QString("已复制%1: %2").arg(type, text));
    statusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    m_statusTimer->start();
}

void HtmlSpecialCharacterTable::onClearSearch()
{
    searchEdit->clear();
    m_currentSearchText = "";
    filterCharacters();
}

void HtmlSpecialCharacterTable::onExportList()
{
    QMessageBox::information(this, "导出功能", "导出功能待实现");
}

void HtmlSpecialCharacterTable::onImportList()
{
    QMessageBox::information(this, "导入功能", "导入功能待实现");
}

void HtmlSpecialCharacterTable::onShowFavorites()
{
    m_showFavoritesOnly = favoritesOnlyCheck->isChecked();
    filterCharacters();
}

void HtmlSpecialCharacterTable::onAddToFavorites()
{
    if (!m_currentCharacter.character.isEmpty()) {
        if (!m_favoriteCharacters.contains(m_currentCharacter)) {
            m_favoriteCharacters.append(m_currentCharacter);
            saveFavorites();
            statusLabel->setText("已添加到收藏");
            m_statusTimer->start();
            
            // 更新按钮状态
            addToFavoritesBtn->setEnabled(false);
            removeFromFavoritesBtn->setEnabled(true);
        }
    }
}

void HtmlSpecialCharacterTable::onRemoveFromFavorites()
{
    if (!m_currentCharacter.character.isEmpty()) {
        m_favoriteCharacters.removeAll(m_currentCharacter);
        saveFavorites();
        statusLabel->setText("已从收藏移除");
        m_statusTimer->start();
        
        // 更新按钮状态
        addToFavoritesBtn->setEnabled(true);
        removeFromFavoritesBtn->setEnabled(false);
        
        // 如果当前显示收藏，需要刷新
        if (m_showFavoritesOnly) {
            filterCharacters();
        }
    }
}

void HtmlSpecialCharacterTable::onViewModeChanged()
{
    m_cardViewMode = cardViewRadio->isChecked();
    
    if (m_cardViewMode) {
        characterTable->setVisible(false);
        scrollArea->setVisible(true);
    } else {
        scrollArea->setVisible(false);
        characterTable->setVisible(true);
    }
    
    updateCharacterDisplay();
}

void HtmlSpecialCharacterTable::filterCharacters()
{
    m_filteredCharacters.clear();
    
    for (const HtmlCharacter &character : m_allCharacters) {
        bool matches = true;
        
        // 分类过滤
        if (m_currentCategory != "全部" && character.category != m_currentCategory) {
            matches = false;
        }
        
        // 搜索过滤
        if (!m_currentSearchText.isEmpty()) {
            QString searchLower = m_currentSearchText.toLower();
            if (!character.character.toLower().contains(searchLower) &&
                !character.entity.toLower().contains(searchLower) &&
                !character.description.toLower().contains(searchLower) &&
                !character.numeric.toLower().contains(searchLower) &&
                !character.hex.toLower().contains(searchLower)) {
                matches = false;
            }
        }
        
        // 收藏过滤
        if (m_showFavoritesOnly && !m_favoriteCharacters.contains(character)) {
            matches = false;
        }
        
        if (matches) {
            m_filteredCharacters.append(character);
        }
    }
    
    updateCharacterDisplay();
    countLabel->setText(QString("字符: %1").arg(m_filteredCharacters.size()));
}

void HtmlSpecialCharacterTable::updateCharacterDisplay()
{
    if (m_cardViewMode) {
        // 清除现有卡片
        QLayoutItem *item;
        while ((item = cardLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        
        // 添加新卡片
        int row = 0, col = 0;
        for (const HtmlCharacter &character : m_filteredCharacters) {
            CharacterCard *card = new CharacterCard(character);
            connect(card, &CharacterCard::characterClicked, this, &HtmlSpecialCharacterTable::onCharacterClicked);
            connect(card, &CharacterCard::copyRequested, this, &HtmlSpecialCharacterTable::onCopyRequested);
            
            cardLayout->addWidget(card, row, col);
            
            col++;
            if (col >= CARDS_PER_ROW) {
                col = 0;
                row++;
            }
        }
        
        // 添加弹性空间
        cardLayout->setRowStretch(row + 1, 1);
        cardLayout->setColumnStretch(CARDS_PER_ROW, 1);
        
    } else {
        // 更新表格
        characterTable->setRowCount(m_filteredCharacters.size());
        
        for (int i = 0; i < m_filteredCharacters.size(); ++i) {
            const HtmlCharacter &character = m_filteredCharacters[i];
            
            characterTable->setItem(i, 0, new QTableWidgetItem(character.character));
            characterTable->setItem(i, 1, new QTableWidgetItem(character.entity));
            characterTable->setItem(i, 2, new QTableWidgetItem(character.numeric));
            characterTable->setItem(i, 3, new QTableWidgetItem(character.hex));
            characterTable->setItem(i, 4, new QTableWidgetItem(QString("U+%1").arg(QString::number(character.unicode, 16).toUpper().rightJustified(4, '0'))));
            characterTable->setItem(i, 5, new QTableWidgetItem(character.description));
            characterTable->setItem(i, 6, new QTableWidgetItem(character.category));
        }
        
        // 连接表格选择事件
        connect(characterTable, &QTableWidget::itemSelectionChanged, [this]() {
            int currentRow = characterTable->currentRow();
            if (currentRow >= 0 && currentRow < m_filteredCharacters.size()) {
                onCharacterClicked(m_filteredCharacters[currentRow]);
            }
        });
    }
}

void HtmlSpecialCharacterTable::updateDetailPanel(const HtmlCharacter &character)
{
    detailCharLabel->setText(character.character);
    detailEntityLabel->setText(character.entity);
    detailNumericLabel->setText(character.numeric);
    detailHexLabel->setText(character.hex);
    detailUnicodeLabel->setText(QString("U+%1").arg(QString::number(character.unicode, 16).toUpper().rightJustified(4, '0')));
    detailDescLabel->setText(character.description);
    detailCategoryLabel->setText(character.category);
    
    // 使用示例
    QString example = QString("HTML: %1\n数字: %2\n十六进制: %3")
                     .arg(character.entity)
                     .arg(character.numeric)
                     .arg(character.hex);
    usageExample->setText(example);
    
    // 更新收藏按钮状态
    bool isFavorite = m_favoriteCharacters.contains(character);
    addToFavoritesBtn->setEnabled(!isFavorite);
    removeFromFavoritesBtn->setEnabled(isFavorite);
    
    // 启用复制按钮
    copyCharDetailBtn->setEnabled(true);
    copyEntityDetailBtn->setEnabled(true);
    copyNumericDetailBtn->setEnabled(true);
    copyHexDetailBtn->setEnabled(true);
}

void HtmlSpecialCharacterTable::clearDetailPanel()
{
    detailCharLabel->setText("选择一个字符");
    detailEntityLabel->setText("-");
    detailNumericLabel->setText("-");
    detailHexLabel->setText("-");
    detailUnicodeLabel->setText("-");
    detailDescLabel->setText("-");
    detailCategoryLabel->setText("-");
    usageExample->clear();
    
    // 禁用按钮
    copyCharDetailBtn->setEnabled(false);
    copyEntityDetailBtn->setEnabled(false);
    copyNumericDetailBtn->setEnabled(false);
    copyHexDetailBtn->setEnabled(false);
    addToFavoritesBtn->setEnabled(false);
    removeFromFavoritesBtn->setEnabled(false);
}

void HtmlSpecialCharacterTable::loadSettings()
{
    // 简化实现，可以后续扩展
}

void HtmlSpecialCharacterTable::saveSettings()
{
    // 简化实现，可以后续扩展
}

void HtmlSpecialCharacterTable::loadFavorites()
{
    // 简化实现，可以后续扩展
}

void HtmlSpecialCharacterTable::saveFavorites()
{
    // 简化实现，可以后续扩展
}

#include "htmlspecialcharactertable.moc"
