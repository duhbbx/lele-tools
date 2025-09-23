#include "chinesecopybook.h"

REGISTER_DYNAMICOBJECT(ChineseCopybook);

#include <QApplication>
#include <QMessageBox>
#include <QSplitter>
#include <QTextDocument>
#include <QPrintPreviewDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDebug>
#include <QRegularExpression>
#include <QPainterPath>
#include <QPageSize>
#include <QPageLayout>

ChineseCopybook::ChineseCopybook(QWidget* parent)
    : QWidget(parent), DynamicObjectBase()
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_textEdit(nullptr)
    , m_customColor(Qt::blue)
    , m_currentPageIndex(0)
{
    setWindowTitle(tr("汉字字帖生成器"));
    setMinimumSize(1000, 700);

    setupUI();
    updateConfig();
}

ChineseCopybook::~ChineseCopybook()
{
}

void ChineseCopybook::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    // 创建主分割器
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(mainSplitter);

    // 左侧控制面板
    QWidget* leftPanel = new QWidget();
    leftPanel->setMaximumWidth(350);
    leftPanel->setMinimumWidth(300);

    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(10);

    setupTabWidget();
    setupContentArea();
    setupOptionsArea();
    setupControlArea();

    leftLayout->addWidget(m_tabWidget);
    leftLayout->addWidget(m_textEdit);

    QVBoxLayout* optionsLayout = setupOptionsArea();
    QWidget* optionsWidget = new QWidget();
    optionsWidget->setLayout(optionsLayout);
    leftLayout->addWidget(optionsWidget);

    QHBoxLayout* controlLayout = setupControlArea();
    QWidget* controlWidget = new QWidget();
    controlWidget->setLayout(controlLayout);
    leftLayout->addWidget(controlWidget);

    leftLayout->addStretch();

    // 右侧预览面板
    setupPreviewArea();

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(m_previewArea);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
}

void ChineseCopybook::setupTabWidget()
{
    m_tabWidget = new QTabWidget();
    m_tabWidget->setFixedHeight(50);

    // 添加标签页（仅用于显示，实际切换会改变配置）
    m_tabWidget->addTab(new QWidget(), tr("描字帖"));
    m_tabWidget->addTab(new QWidget(), tr("笔顺贴"));
    m_tabWidget->addTab(new QWidget(), tr("控笔贴"));
    m_tabWidget->addTab(new QWidget(), tr("字帖模板"));

    connect(m_tabWidget, QOverload<int>::of(&QTabWidget::currentChanged),
            this, &ChineseCopybook::onTabChanged);
}

void ChineseCopybook::setupContentArea()
{
    m_textEdit = new QTextEdit();
    m_textEdit->setMaximumHeight(120);
    m_textEdit->setPlaceholderText(tr("请输入要生成字帖的汉字内容..."));
    m_textEdit->setPlainText(tr("汉字练习"));

    connect(m_textEdit, &QTextEdit::textChanged,
            this, &ChineseCopybook::onTextChanged);
}

QVBoxLayout* ChineseCopybook::setupOptionsArea()
{
    QVBoxLayout* optionsLayout = new QVBoxLayout();

    // 格子类型选择
    m_gridGroup = new QGroupBox(tr("格子类型"));
    QGridLayout* gridLayout = new QGridLayout(m_gridGroup);

    m_gridButtonGroup = new QButtonGroup(this);
    m_noneGridRadio = new QRadioButton(tr("无格子"));
    m_squareGridRadio = new QRadioButton(tr("方格"));
    m_tianZiGridRadio = new QRadioButton(tr("田字格"));
    m_miZiGridRadio = new QRadioButton(tr("米字格"));
    m_miZiHuiGongGridRadio = new QRadioButton(tr("米字回宫格"));
    m_huiGongGridRadio = new QRadioButton(tr("回宫格"));

    m_tianZiGridRadio->setChecked(true);  // 默认选择田字格

    m_gridButtonGroup->addButton(m_noneGridRadio, static_cast<int>(GridType::None));
    m_gridButtonGroup->addButton(m_squareGridRadio, static_cast<int>(GridType::Square));
    m_gridButtonGroup->addButton(m_tianZiGridRadio, static_cast<int>(GridType::TianZi));
    m_gridButtonGroup->addButton(m_miZiGridRadio, static_cast<int>(GridType::MiZi));
    m_gridButtonGroup->addButton(m_miZiHuiGongGridRadio, static_cast<int>(GridType::MiZiHuiGong));
    m_gridButtonGroup->addButton(m_huiGongGridRadio, static_cast<int>(GridType::HuiGong));

    gridLayout->addWidget(m_noneGridRadio, 0, 0);
    gridLayout->addWidget(m_squareGridRadio, 0, 1);
    gridLayout->addWidget(m_tianZiGridRadio, 1, 0);
    gridLayout->addWidget(m_miZiGridRadio, 1, 1);
    gridLayout->addWidget(m_miZiHuiGongGridRadio, 2, 0);
    gridLayout->addWidget(m_huiGongGridRadio, 2, 1);

    connect(m_gridButtonGroup, &QButtonGroup::idClicked,
            this, &ChineseCopybook::onGridTypeChanged);

    // 格子颜色选择
    m_colorGroup = new QGroupBox(tr("格子颜色"));
    QHBoxLayout* colorLayout = new QHBoxLayout(m_colorGroup);

    m_colorButtonGroup = new QButtonGroup(this);
    m_greenColorRadio = new QRadioButton(tr("绿色"));
    m_blackColorRadio = new QRadioButton(tr("黑色"));
    m_redColorRadio = new QRadioButton(tr("红色"));
    m_customColorButton = new QPushButton(tr("自定义"));

    m_greenColorRadio->setChecked(true);  // 默认绿色

    m_colorButtonGroup->addButton(m_greenColorRadio, 0);
    m_colorButtonGroup->addButton(m_blackColorRadio, 1);
    m_colorButtonGroup->addButton(m_redColorRadio, 2);

    colorLayout->addWidget(m_greenColorRadio);
    colorLayout->addWidget(m_blackColorRadio);
    colorLayout->addWidget(m_redColorRadio);
    colorLayout->addWidget(m_customColorButton);

    connect(m_colorButtonGroup, &QButtonGroup::idClicked,
            this, &ChineseCopybook::onGridColorChanged);
    connect(m_customColorButton, &QPushButton::clicked,
            this, &ChineseCopybook::onCustomColorChanged);

    // 其他选项
    m_optionsGroup = new QGroupBox(tr("其他选项"));
    QVBoxLayout* optLayout = new QVBoxLayout(m_optionsGroup);

    m_showPinyinRowCheck = new QCheckBox(tr("显示拼音栏"));
    m_fillPinyinCheck = new QCheckBox(tr("填充拼音"));
    m_showChineseRowCheck = new QCheckBox(tr("显示中文栏"));
    m_fillChineseCheck = new QCheckBox(tr("填充中文"));

    m_showPinyinRowCheck->setChecked(true);
    m_showChineseRowCheck->setChecked(true);

    optLayout->addWidget(m_showPinyinRowCheck);
    optLayout->addWidget(m_fillPinyinCheck);
    optLayout->addWidget(m_showChineseRowCheck);
    optLayout->addWidget(m_fillChineseCheck);

    connect(m_showPinyinRowCheck, &QCheckBox::toggled, this, &ChineseCopybook::onOptionsChanged);
    connect(m_fillPinyinCheck, &QCheckBox::toggled, this, &ChineseCopybook::onOptionsChanged);
    connect(m_showChineseRowCheck, &QCheckBox::toggled, this, &ChineseCopybook::onOptionsChanged);
    connect(m_fillChineseCheck, &QCheckBox::toggled, this, &ChineseCopybook::onOptionsChanged);

    // 大小设置
    m_sizeGroup = new QGroupBox(tr("大小设置"));
    QGridLayout* sizeLayout = new QGridLayout(m_sizeGroup);

    m_fontSizeLabel = new QLabel(tr("字体大小:"));
    m_fontSizeSlider = new QSlider(Qt::Horizontal);
    m_fontSizeSlider->setRange(20, 100);
    m_fontSizeSlider->setValue(48);
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(20, 100);
    m_fontSizeSpinBox->setValue(48);
    m_fontSizeSpinBox->setSuffix("px");

    m_gridSizeLabel = new QLabel(tr("格子大小:"));
    m_gridSizeSlider = new QSlider(Qt::Horizontal);
    m_gridSizeSlider->setRange(50, 150);
    m_gridSizeSlider->setValue(80);
    m_gridSizeSpinBox = new QSpinBox();
    m_gridSizeSpinBox->setRange(50, 150);
    m_gridSizeSpinBox->setValue(80);
    m_gridSizeSpinBox->setSuffix("px");

    sizeLayout->addWidget(m_fontSizeLabel, 0, 0);
    sizeLayout->addWidget(m_fontSizeSlider, 0, 1);
    sizeLayout->addWidget(m_fontSizeSpinBox, 0, 2);
    sizeLayout->addWidget(m_gridSizeLabel, 1, 0);
    sizeLayout->addWidget(m_gridSizeSlider, 1, 1);
    sizeLayout->addWidget(m_gridSizeSpinBox, 1, 2);

    connect(m_fontSizeSlider, &QSlider::valueChanged, m_fontSizeSpinBox, &QSpinBox::setValue);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_fontSizeSlider, &QSlider::setValue);
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, &ChineseCopybook::onFontSizeChanged);

    connect(m_gridSizeSlider, &QSlider::valueChanged, m_gridSizeSpinBox, &QSpinBox::setValue);
    connect(m_gridSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_gridSizeSlider, &QSlider::setValue);
    connect(m_gridSizeSlider, &QSlider::valueChanged, this, &ChineseCopybook::onGridSizeChanged);

    optionsLayout->addWidget(m_gridGroup);
    optionsLayout->addWidget(m_colorGroup);
    optionsLayout->addWidget(m_optionsGroup);
    optionsLayout->addWidget(m_sizeGroup);

    return optionsLayout;
}

QHBoxLayout* ChineseCopybook::setupControlArea()
{
    QHBoxLayout* controlLayout = new QHBoxLayout();

    m_generateButton = new QPushButton(tr("生成"));
    m_previewButton = new QPushButton(tr("预览"));
    m_printButton = new QPushButton(tr("打印"));

    m_generateButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
    );

    m_previewButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #2196F3;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1976D2;"
        "}"
    );

    m_printButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF9800;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #F57C00;"
        "}"
    );

    connect(m_generateButton, &QPushButton::clicked, this, &ChineseCopybook::onGenerate);
    connect(m_previewButton, &QPushButton::clicked, this, &ChineseCopybook::onPreview);
    connect(m_printButton, &QPushButton::clicked, this, &ChineseCopybook::onPrint);

    controlLayout->addWidget(m_generateButton);
    controlLayout->addWidget(m_previewButton);
    controlLayout->addWidget(m_printButton);

    return controlLayout;
}

void ChineseCopybook::setupPreviewArea()
{
    // 创建预览区域的容器
    QWidget* previewContainer = new QWidget();
    QVBoxLayout* previewLayout = new QVBoxLayout(previewContainer);
    previewLayout->setContentsMargins(5, 5, 5, 5);
    previewLayout->setSpacing(5);

    // 页面导航控件
    QWidget* navWidget = new QWidget();
    QHBoxLayout* navLayout = new QHBoxLayout(navWidget);
    navLayout->setContentsMargins(0, 0, 0, 0);

    m_prevPageButton = new QPushButton(tr("上一页"));
    m_nextPageButton = new QPushButton(tr("下一页"));
    m_pageInfoLabel = new QLabel(tr("页面: 0/0"));

    m_prevPageButton->setEnabled(false);
    m_nextPageButton->setEnabled(false);

    navLayout->addWidget(m_prevPageButton);
    navLayout->addWidget(m_pageInfoLabel);
    navLayout->addWidget(m_nextPageButton);
    navLayout->addStretch();

    connect(m_prevPageButton, &QPushButton::clicked, this, &ChineseCopybook::onPrevPage);
    connect(m_nextPageButton, &QPushButton::clicked, this, &ChineseCopybook::onNextPage);

    // 滚动区域
    m_previewArea = new QScrollArea();
    m_previewArea->setWidgetResizable(true);
    m_previewArea->setStyleSheet(
        "QScrollArea {"
        "    border: 1px solid #ccc;"
        "    background-color: #f5f5f5;"
        "}"
    );

    m_previewLabel = new QLabel();
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: white; border: 2px solid #333; margin: 10px;");  // A4边框样式
    m_previewLabel->setText(tr("字帖预览区域\n\n点击\"生成\"按钮查看字帖效果"));
    m_previewLabel->setMinimumSize(400, 300);

    m_previewArea->setWidget(m_previewLabel);

    // 组装布局
    previewLayout->addWidget(navWidget);
    previewLayout->addWidget(m_previewArea);

    // 用容器替换原来的滚动区域
    m_previewArea = new QScrollArea();
    m_previewArea->setWidget(previewContainer);
    m_previewArea->setWidgetResizable(true);
}

void ChineseCopybook::onTabChanged(int index)
{
    m_config.type = static_cast<CopybookType>(index);
    updateConfig();
}

void ChineseCopybook::onTextChanged()
{
    m_config.text = m_textEdit->toPlainText();
    updateConfig();
}

void ChineseCopybook::onGridTypeChanged(int id)
{
    m_config.gridType = static_cast<GridType>(id);
    updateConfig();
}

void ChineseCopybook::onGridColorChanged(int id)
{
    switch (id) {
    case 0: m_config.gridColor = QColor(0, 128, 0); break;  // 绿色
    case 1: m_config.gridColor = Qt::black; break;         // 黑色
    case 2: m_config.gridColor = Qt::red; break;           // 红色
    }
    updateConfig();
}

void ChineseCopybook::onCustomColorChanged()
{
    QColor color = QColorDialog::getColor(m_customColor, this, tr("选择格子颜色"));
    if (color.isValid()) {
        m_customColor = color;
        m_config.gridColor = color;
        m_customColorButton->setText(tr("自定义 (%1)").arg(color.name()));
        updateConfig();
    }
}

void ChineseCopybook::onOptionsChanged()
{
    m_config.showPinyinRow = m_showPinyinRowCheck->isChecked();
    m_config.fillPinyin = m_fillPinyinCheck->isChecked();
    m_config.showChineseRow = m_showChineseRowCheck->isChecked();
    m_config.fillChinese = m_fillChineseCheck->isChecked();
    updateConfig();
}

void ChineseCopybook::onFontSizeChanged(int size)
{
    m_config.fontSize = size;
    updateConfig();
}

void ChineseCopybook::onGridSizeChanged(int size)
{
    m_config.gridSize = size;
    updateConfig();
}

void ChineseCopybook::onGenerate()
{
    generateMultiPageCopybook();

    if (!m_pagePixmaps.isEmpty()) {
        m_currentPageIndex = 0;
        m_previewPixmap = m_pagePixmaps.first();

        // 更新预览
        QSize labelSize = m_previewLabel->size();
        if (labelSize.width() < 100) labelSize = QSize(600, 800);  // 默认大小

        m_previewLabel->setPixmap(m_previewPixmap.scaled(
            labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // 更新页面信息
        updatePageInfo();
    }
}

void ChineseCopybook::onPreview()
{
    generateMultiPageCopybook();

    if (!m_pagePixmaps.isEmpty()) {
        // 创建预览对话框
        QDialog* previewDialog = new QDialog(this);
        previewDialog->setWindowTitle(tr("字帖预览"));
        previewDialog->resize(900, 700);

        QVBoxLayout* layout = new QVBoxLayout(previewDialog);

        // 页面导航
        QWidget* navWidget = new QWidget();
        QHBoxLayout* navLayout = new QHBoxLayout(navWidget);

        QPushButton* prevBtn = new QPushButton(tr("上一页"));
        QPushButton* nextBtn = new QPushButton(tr("下一页"));
        QLabel* pageLabel = new QLabel();

        navLayout->addWidget(prevBtn);
        navLayout->addWidget(pageLabel);
        navLayout->addWidget(nextBtn);
        navLayout->addStretch();

        // 图片显示
        QScrollArea* scrollArea = new QScrollArea();
        QLabel* imageLabel = new QLabel();
        imageLabel->setAlignment(Qt::AlignCenter);
        scrollArea->setWidget(imageLabel);
        scrollArea->setWidgetResizable(true);

        layout->addWidget(navWidget);
        layout->addWidget(scrollArea);

        // 初始化
        int currentIndex = 0;
        auto updatePreview = [=]() mutable {
            imageLabel->setPixmap(m_pagePixmaps.at(currentIndex));
            pageLabel->setText(tr("页面: %1/%2").arg(currentIndex + 1).arg(m_pagePixmaps.size()));
            prevBtn->setEnabled(currentIndex > 0);
            nextBtn->setEnabled(currentIndex < m_pagePixmaps.size() - 1);
        };

        connect(prevBtn, &QPushButton::clicked, [=, &currentIndex, &updatePreview]() mutable {
            if (currentIndex > 0) {
                currentIndex--;
                updatePreview();
            }
        });

        connect(nextBtn, &QPushButton::clicked, [=, &currentIndex, &updatePreview]() mutable {
            if (currentIndex < m_pagePixmaps.size() - 1) {
                currentIndex++;
                updatePreview();
            }
        });

        updatePreview();
        previewDialog->exec();
    }
}

void ChineseCopybook::onPrint()
{
    generateCopybook();

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);

    QPrintDialog printDialog(&printer, this);
    printDialog.setWindowTitle(tr("打印字帖"));

    if (printDialog.exec() == QDialog::Accepted) {
        printCopybook(&printer);
    }
}

void ChineseCopybook::onPrevPage()
{
    if (m_currentPageIndex > 0) {
        m_currentPageIndex--;
        m_previewPixmap = m_pagePixmaps.at(m_currentPageIndex);

        QSize labelSize = m_previewLabel->size();
        if (labelSize.width() < 100) labelSize = QSize(600, 800);

        m_previewLabel->setPixmap(m_previewPixmap.scaled(
            labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updatePageInfo();
    }
}

void ChineseCopybook::onNextPage()
{
    if (m_currentPageIndex < m_pagePixmaps.size() - 1) {
        m_currentPageIndex++;
        m_previewPixmap = m_pagePixmaps.at(m_currentPageIndex);

        QSize labelSize = m_previewLabel->size();
        if (labelSize.width() < 100) labelSize = QSize(600, 800);

        m_previewLabel->setPixmap(m_previewPixmap.scaled(
            labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updatePageInfo();
    }
}

void ChineseCopybook::updatePageInfo()
{
    m_pageInfoLabel->setText(tr("页面: %1/%2").arg(m_currentPageIndex + 1).arg(m_pagePixmaps.size()));
    m_prevPageButton->setEnabled(m_currentPageIndex > 0);
    m_nextPageButton->setEnabled(m_currentPageIndex < m_pagePixmaps.size() - 1);
}

void ChineseCopybook::updateConfig()
{
    // 更新配置后可以在这里触发预览更新等操作
}

void ChineseCopybook::generateCopybook()
{
    if (m_config.text.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入要生成字帖的文字内容！"));
        return;
    }

    // 计算页面大小
    QSize pageSize(794, 1123);  // A4 大小 (像素)
    m_previewPixmap = QPixmap(pageSize);
    m_previewPixmap.fill(Qt::white);

    QPainter painter(&m_previewPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 设置字体
    QFont font = m_config.textFont;
    font.setPixelSize(m_config.fontSize);
    painter.setFont(font);

    // 计算布局
    int margin = 50;
    int gridSize = m_config.gridSize;
    int spacing = 10;

    int availableWidth = pageSize.width() - 2 * margin;
    int availableHeight = pageSize.height() - 2 * margin;

    int colsPerRow = availableWidth / (gridSize + spacing);
    int rowsPerPage = availableHeight / (gridSize + spacing * 2);  // 考虑拼音行

    // 如果显示拼音行，调整行数
    if (m_config.showPinyinRow) {
        rowsPerPage = rowsPerPage / 2;
    }

    QString text = m_config.text;
    text.remove(QRegularExpression("\\s"));  // 移除空白字符

    int charIndex = 0;
    int currentRow = 0;

    while (charIndex < text.length() && currentRow < rowsPerPage) {
        for (int col = 0; col < colsPerRow && charIndex < text.length(); ++col) {
            QChar ch = text.at(charIndex);

            int x = margin + col * (gridSize + spacing);
            int y = margin + currentRow * (gridSize + spacing * 2);

            // 绘制拼音行
            if (m_config.showPinyinRow) {
                QRect pinyinRect(x, y, gridSize, spacing * 2);
                if (m_config.fillPinyin) {
                    QString pinyin = getCharacterPinyin(ch);
                    drawPinyinRow(painter, pinyinRect, pinyin);
                }
                y += spacing * 2;
            }

            // 绘制字符格子
            QRect charRect(x, y, gridSize, gridSize);

            // 绘制格子
            if (m_config.gridType != GridType::None) {
                drawGrid(painter, charRect, m_config.gridType, m_config.gridColor);
            }

            // 绘制字符
            if (m_config.fillChinese || m_config.type == CopybookType::Tracing) {
                bool isTemplate = (m_config.type == CopybookType::Template);
                bool showStroke = (m_config.type == CopybookType::StrokeOrder);
                drawCharacter(painter, charRect, ch, isTemplate, showStroke);
            }

            charIndex++;
        }
        currentRow++;
    }

    painter.end();
}

void ChineseCopybook::generateMultiPageCopybook()
{
    if (m_config.text.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入要生成字帖的文字内容！"));
        return;
    }

    m_pagePixmaps.clear();

    QString text = m_config.text;
    text.remove(QRegularExpression("\\s"));  // 移除空白字符

    if (text.isEmpty()) {
        return;
    }

    // A4页面大小 (像素)
    QSize pageSize(794, 1123);

    int charIndex = 0;
    int pageNumber = 1;

    while (charIndex < text.length()) {
        QPixmap pagePixmap(pageSize);
        pagePixmap.fill(Qt::white);

        QPainter painter(&pagePixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        // 绘制A4边框
        QRect pageRect(0, 0, pageSize.width(), pageSize.height());
        drawA4Border(painter, pageRect);

        // 绘制页面内容
        drawPage(painter, pageRect, text, charIndex);

        // 添加页码
        painter.setFont(QFont("Arial", 10));
        painter.setPen(Qt::black);
        QString pageInfo = tr("第 %1 页").arg(pageNumber);
        painter.drawText(pageRect.bottomRight() - QPoint(100, 20), pageInfo);

        painter.end();

        m_pagePixmaps.append(pagePixmap);
        pageNumber++;

        // 防止无限循环
        if (pageNumber > 100) {
            break;
        }
    }
}

void ChineseCopybook::drawA4Border(QPainter& painter, const QRect& pageRect)
{
    painter.save();

    // 绘制外边框 (模拟A4纸边缘)
    painter.setPen(QPen(Qt::black, 3));
    painter.drawRect(pageRect.adjusted(5, 5, -5, -5));

    // 绘制内边框 (页面内容区域)
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawRect(pageRect.adjusted(15, 15, -15, -15));

    painter.restore();
}

void ChineseCopybook::drawPage(QPainter& painter, const QRect& pageRect, const QString& text, int& charIndex)
{
    // 设置字体
    QFont font = m_config.textFont;
    font.setPixelSize(m_config.fontSize);
    painter.setFont(font);

    // 计算内容区域 (减去边框和边距)
    QRect contentRect = pageRect.adjusted(25, 25, -25, -50);  // 留出页码空间

    int gridSize = m_config.gridSize;
    int spacing = 10;

    int availableWidth = contentRect.width();
    int availableHeight = contentRect.height();

    int colsPerRow = availableWidth / (gridSize + spacing);
    int totalRowHeight = gridSize + spacing;

    // 如果显示拼音行，调整行高
    if (m_config.showPinyinRow) {
        totalRowHeight += spacing * 2;
    }

    int rowsPerPage = availableHeight / totalRowHeight;

    int currentRow = 0;
    int startCharIndex = charIndex;

    while (charIndex < text.length() && currentRow < rowsPerPage) {
        for (int col = 0; col < colsPerRow && charIndex < text.length(); ++col) {
            QChar ch = text.at(charIndex);

            int x = contentRect.left() + col * (gridSize + spacing);
            int y = contentRect.top() + currentRow * totalRowHeight;

            // 绘制拼音行
            if (m_config.showPinyinRow) {
                QRect pinyinRect(x, y, gridSize, spacing * 2);
                if (m_config.fillPinyin) {
                    QString pinyin = getCharacterPinyin(ch);
                    drawPinyinRow(painter, pinyinRect, pinyin);
                }
                y += spacing * 2;
            }

            // 绘制字符格子
            QRect charRect(x, y, gridSize, gridSize);

            // 绘制格子
            if (m_config.gridType != GridType::None) {
                drawGrid(painter, charRect, m_config.gridType, m_config.gridColor);
            }

            // 绘制字符
            if (m_config.fillChinese || m_config.type == CopybookType::Tracing) {
                bool isTemplate = (m_config.type == CopybookType::Template);
                bool showStroke = (m_config.type == CopybookType::StrokeOrder);
                drawCharacter(painter, charRect, ch, isTemplate, showStroke);
            }

            charIndex++;
        }
        currentRow++;
    }
}

void ChineseCopybook::drawGrid(QPainter& painter, const QRect& rect, GridType type, const QColor& color)
{
    painter.save();
    painter.setPen(QPen(color, 1));

    switch (type) {
    case GridType::Square:
        drawSquareGrid(painter, rect, color);
        break;
    case GridType::TianZi:
        drawTianZiGrid(painter, rect, color);
        break;
    case GridType::MiZi:
        drawMiZiGrid(painter, rect, color);
        break;
    case GridType::MiZiHuiGong:
        drawMiZiHuiGongGrid(painter, rect, color);
        break;
    case GridType::HuiGong:
        drawHuiGongGrid(painter, rect, color);
        break;
    default:
        break;
    }

    painter.restore();
}

void ChineseCopybook::drawSquareGrid(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setPen(QPen(color, 2));
    painter.drawRect(rect);
}

void ChineseCopybook::drawTianZiGrid(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setPen(QPen(color, 2));
    painter.drawRect(rect);

    painter.setPen(QPen(color, 1));
    // 垂直中线
    painter.drawLine(rect.center().x(), rect.top(), rect.center().x(), rect.bottom());
    // 水平中线
    painter.drawLine(rect.left(), rect.center().y(), rect.right(), rect.center().y());
}

void ChineseCopybook::drawMiZiGrid(QPainter& painter, const QRect& rect, const QColor& color)
{
    drawTianZiGrid(painter, rect, color);

    painter.setPen(QPen(color, 1));
    // 对角线
    painter.drawLine(rect.topLeft(), rect.bottomRight());
    painter.drawLine(rect.topRight(), rect.bottomLeft());
}

void ChineseCopybook::drawMiZiHuiGongGrid(QPainter& painter, const QRect& rect, const QColor& color)
{
    drawMiZiGrid(painter, rect, color);

    painter.setPen(QPen(color, 1));
    int margin = rect.width() / 8;
    QRect innerRect = rect.adjusted(margin, margin, -margin, -margin);
    painter.drawRect(innerRect);
}

void ChineseCopybook::drawHuiGongGrid(QPainter& painter, const QRect& rect, const QColor& color)
{
    painter.setPen(QPen(color, 2));
    painter.drawRect(rect);

    painter.setPen(QPen(color, 1));
    int margin = rect.width() / 6;
    QRect innerRect = rect.adjusted(margin, margin, -margin, -margin);
    painter.drawRect(innerRect);
}

void ChineseCopybook::drawCharacter(QPainter& painter, const QRect& rect, const QString& character,
                                   bool isTemplate, bool showStroke)
{
    painter.save();

    QFont font = painter.font();

    if (isTemplate) {
        // 模板模式：绘制淡色字符轮廓
        painter.setPen(QPen(QColor(200, 200, 200), 2));
        painter.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.addText(rect.center() - QPointF(font.pixelSize()/3, -font.pixelSize()/3), font, character);
        painter.drawPath(path);
    } else if (showStroke) {
        // 笔顺模式：显示笔画顺序（简化实现）
        painter.setPen(QPen(Qt::black, 2));
        painter.drawText(rect, Qt::AlignCenter, character);

        // 可以在这里添加笔顺号码等
        painter.setPen(QPen(Qt::red, 1));
        painter.setFont(QFont(font.family(), font.pixelSize()/4));
        // 简化：在角落显示数字1
        painter.drawText(rect.topLeft() + QPoint(5, 15), "1");
    } else {
        // 普通模式：绘制完整字符
        painter.setPen(QPen(Qt::black, 1));
        painter.drawText(rect, Qt::AlignCenter, character);
    }

    painter.restore();
}

void ChineseCopybook::drawPinyinRow(QPainter& painter, const QRect& rect, const QString& pinyin)
{
    painter.save();

    QFont font = painter.font();
    font.setPixelSize(font.pixelSize() / 3);
    painter.setFont(font);
    painter.setPen(Qt::black);

    painter.drawText(rect, Qt::AlignCenter, pinyin);

    painter.restore();
}

QString ChineseCopybook::getCharacterPinyin(const QString& character)
{
    // 简化实现：返回字符的拼音
    // 实际应用中可以使用拼音库或API
    static QMap<QString, QString> pinyinMap;
    if (pinyinMap.isEmpty()) {
        pinyinMap["汉"] = "hàn";
        pinyinMap["字"] = "zì";
        pinyinMap["练"] = "liàn";
        pinyinMap["习"] = "xí";
        // 可以扩展更多字符的拼音
    }

    return pinyinMap.value(character, "");
}

QStringList ChineseCopybook::getCharacterStrokeOrder(const QString& character)
{
    // 简化实现：返回字符的笔画顺序
    // 实际应用中可以使用笔画数据库
    Q_UNUSED(character)
    return QStringList();
}

void ChineseCopybook::printCopybook(QPrinter* printer)
{
    if (m_pagePixmaps.isEmpty()) {
        generateMultiPageCopybook();
    }

    if (m_pagePixmaps.isEmpty()) {
        return;
    }

    QPainter painter(printer);
    QRect pageRect = printer->pageLayout().paintRectPixels(printer->resolution());

    for (int i = 0; i < m_pagePixmaps.size(); ++i) {
        if (i > 0) {
            printer->newPage();
        }

        QPixmap scaledPixmap = m_pagePixmaps.at(i).scaled(
            pageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 居中绘制
        QPoint drawPoint = pageRect.center() - scaledPixmap.rect().center();
        painter.drawPixmap(drawPoint, scaledPixmap);
    }

    // 显式结束QPainter以避免警告
    painter.end();
}

