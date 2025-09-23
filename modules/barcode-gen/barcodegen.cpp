#include "barcodegen.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFontDatabase>
#include <QStandardPaths>

REGISTER_DYNAMICOBJECT(BarcodeGen);

// BarcodePreview 实现
BarcodePreview::BarcodePreview(QWidget *parent)
    : QFrame(parent), m_hasBarcode(false)
{
    setFrameStyle(QFrame::Box);
    setMinimumSize(320, 120);
    setAcceptDrops(true);
    setStyleSheet("QFrame { border: 2px dashed #ccc; background-color: #f9f9f9; }");
}

void BarcodePreview::setBarcode(const QPixmap &pixmap)
{
    m_barcodePixmap = pixmap;
    m_hasBarcode = !pixmap.isNull();
    update();
}

void BarcodePreview::setBarcodeItem(const BarcodeItem &item)
{
    m_barcodeItem = item;
    setBarcode(item.barcodePixmap);
}

void BarcodePreview::clearBarcode()
{
    m_barcodePixmap = QPixmap();
    m_hasBarcode = false;
    update();
}

void BarcodePreview::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = contentsRect();
    
    if (m_hasBarcode && !m_barcodePixmap.isNull()) {
        // 居中显示条形码
        QRect pixmapRect = m_barcodePixmap.rect();
        pixmapRect.moveCenter(rect.center());
        painter.drawPixmap(pixmapRect, m_barcodePixmap);
    } else {
        // 显示提示文字
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(rect, Qt::AlignCenter, "条形码预览区域\n点击生成按钮生成条形码");
    }
}

void BarcodePreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_hasBarcode) {
        emit barcodeClicked();
    }
    QFrame::mousePressEvent(event);
}

void BarcodePreview::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_hasBarcode) return;
    
    QMenu menu(this);
    menu.addAction("保存条形码", this, &BarcodePreview::saveRequested);
    menu.exec(event->globalPos());
}

// ColorButton实现现在在common/colorbutton.cpp中

// BarcodeGen 主类实现
BarcodeGen::BarcodeGen() : QWidget(nullptr), DynamicObjectBase(), m_updatingUI(false)
{
    m_currentType = BarcodeType::Code128;
    m_statusTimer = new QTimer(this);
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300);
    
    connect(m_updateTimer, &QTimer::timeout, this, &BarcodeGen::updateBarcodePreview);
    
    setupUI();
    initializePresets();
    loadSettings();
}

BarcodeGen::~BarcodeGen()
{
    saveSettings();
}

void BarcodeGen::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplitter, 1); // 添加stretch factor
    
    // 设置左右面板
    leftPanel = new QWidget;
    rightPanel = new QWidget;
    
    leftLayout = new QVBoxLayout(leftPanel);
    rightLayout = new QVBoxLayout(rightPanel);
    
    leftLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    setupInputArea();
    setupStyleArea();
    setupBatchArea();
    setupPreviewArea();
    setupHistoryArea();
    setupStatusArea();
    
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({400, 350});
    
    setAcceptDrops(true);
}

void BarcodeGen::setupInputArea()
{
    inputGroup = new QGroupBox(tr("条形码生成"), leftPanel);
    inputLayout = new QGridLayout(inputGroup);
    
    // 条形码类型
    inputLayout->addWidget(new QLabel(tr("类型:")), 0, 0);
    barcodeTypeCombo = new QComboBox;
    barcodeTypeCombo->addItem("Code 128", static_cast<int>(BarcodeType::Code128));
    barcodeTypeCombo->addItem("Code 39", static_cast<int>(BarcodeType::Code39));
    barcodeTypeCombo->addItem("Code 93", static_cast<int>(BarcodeType::Code93));
    barcodeTypeCombo->addItem("EAN-13", static_cast<int>(BarcodeType::EAN13));
    barcodeTypeCombo->addItem("EAN-8", static_cast<int>(BarcodeType::EAN8));
    barcodeTypeCombo->addItem("UPC-A", static_cast<int>(BarcodeType::UPCA));
    barcodeTypeCombo->addItem("UPC-E", static_cast<int>(BarcodeType::UPCE));
    inputLayout->addWidget(barcodeTypeCombo, 0, 1, 1, 2);
    
    // 输入文本
    inputLayout->addWidget(new QLabel(tr("内容:")), 1, 0);
    textEdit = new QLineEdit;
    textEdit->setPlaceholderText(tr("请输入要生成条形码的内容"));
    inputLayout->addWidget(textEdit, 1, 1, 1, 2);
    
    // 预设模板
    inputLayout->addWidget(new QLabel(tr("预设:")), 2, 0);
    presetCombo = new QComboBox;
    inputLayout->addWidget(presetCombo, 2, 1, 1, 2);
    
    // 按钮
    generateBtn = new QPushButton(tr("生成条形码"));
    clearBtn = new QPushButton(tr("清空"));
    
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(generateBtn);
    btnLayout->addWidget(clearBtn);
    inputLayout->addLayout(btnLayout, 3, 0, 1, 3);
    
    leftLayout->addWidget(inputGroup);
    
    // 连接信号
    connect(barcodeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &BarcodeGen::onBarcodeTypeChanged);
    connect(textEdit, &QLineEdit::textChanged, this, &BarcodeGen::onTextChanged);
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &BarcodeGen::onPresetSelected);
    connect(generateBtn, &QPushButton::clicked, this, &BarcodeGen::onGenerateBarcode);
    connect(clearBtn, &QPushButton::clicked, [this]() {
        textEdit->clear();
        barcodePreview->clearBarcode();
        clearResultDisplay();
    });
}

void BarcodeGen::setupStyleArea()
{
    styleGroup = new QGroupBox(tr("样式设置"), leftPanel);
    styleLayout = new QGridLayout(styleGroup);
    
    // 颜色设置
    styleLayout->addWidget(new QLabel(tr("前景色:")), 0, 0);
    foregroundColorBtn = new ColorButton(Qt::black);
    styleLayout->addWidget(foregroundColorBtn, 0, 1);
    
    styleLayout->addWidget(new QLabel(tr("背景色:")), 0, 2);
    backgroundColorBtn = new ColorButton(Qt::white);
    styleLayout->addWidget(backgroundColorBtn, 0, 3);
    
    // 尺寸设置
    styleLayout->addWidget(new QLabel(tr("宽度:")), 1, 0);
    widthSpinBox = new QSpinBox;
    widthSpinBox->setRange(100, 1000);
    widthSpinBox->setValue(300);
    styleLayout->addWidget(widthSpinBox, 1, 1);
    
    styleLayout->addWidget(new QLabel(tr("高度:")), 1, 2);
    heightSpinBox = new QSpinBox;
    heightSpinBox->setRange(50, 500);
    heightSpinBox->setValue(100);
    styleLayout->addWidget(heightSpinBox, 1, 3);
    
    // 文本设置
    showTextCheckBox = new QCheckBox(tr("显示文本"));
    showTextCheckBox->setChecked(true);
    styleLayout->addWidget(showTextCheckBox, 2, 0, 1, 2);
    
    styleLayout->addWidget(new QLabel(tr("字体大小:")), 2, 2);
    fontSizeSpinBox = new QSpinBox;
    fontSizeSpinBox->setRange(8, 24);
    fontSizeSpinBox->setValue(12);
    styleLayout->addWidget(fontSizeSpinBox, 2, 3);
    
    leftLayout->addWidget(styleGroup);
    
    // 连接信号
    connect(foregroundColorBtn, &ColorButton::colorChanged, 
            this, &BarcodeGen::onForegroundColorChanged);
    connect(backgroundColorBtn, &ColorButton::colorChanged, 
            this, &BarcodeGen::onBackgroundColorChanged);
    connect(widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &BarcodeGen::onStyleChanged);
    connect(heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &BarcodeGen::onStyleChanged);
    connect(showTextCheckBox, &QCheckBox::toggled, this, &BarcodeGen::onStyleChanged);
    connect(fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &BarcodeGen::onStyleChanged);
}

void BarcodeGen::setupBatchArea()
{
    batchGroup = new QGroupBox(tr("批量处理"), leftPanel);
    batchLayout = new QVBoxLayout(batchGroup);
    
    batchTextEdit = new QTextEdit;
    batchTextEdit->setMinimumHeight(60);
    batchTextEdit->setPlaceholderText(tr("每行一个内容，用于批量生成条形码"));
    batchLayout->addWidget(batchTextEdit, 1); // 添加stretch factor使其可拉伸

    batchButtonLayout = new QHBoxLayout;
    batchGenerateBtn = new QPushButton(tr("批量生成"));
    addBatchItemBtn = new QPushButton(tr("添加当前"));
    clearBatchListBtn = new QPushButton(tr("清空列表"));
    importBatchBtn = new QPushButton(tr("导入"));
    exportBatchBtn = new QPushButton(tr("导出"));

    // 设置按钮的大小策略，让高度跟随内容
    batchGenerateBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    addBatchItemBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    clearBatchListBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    importBatchBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    exportBatchBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    batchButtonLayout->addWidget(batchGenerateBtn);
    batchButtonLayout->addWidget(addBatchItemBtn);
    batchButtonLayout->addWidget(clearBatchListBtn);
    batchButtonLayout->addWidget(importBatchBtn);
    batchButtonLayout->addWidget(exportBatchBtn);
    batchLayout->addLayout(batchButtonLayout, 0); // 按钮布局不拉伸
    
    batchProgressBar = new QProgressBar;
    batchProgressBar->setVisible(false);
    batchLayout->addWidget(batchProgressBar);
    
    leftLayout->addWidget(batchGroup);
    
    // 连接信号
    connect(batchGenerateBtn, &QPushButton::clicked, this, &BarcodeGen::onBatchGenerate);
    connect(addBatchItemBtn, &QPushButton::clicked, this, &BarcodeGen::onAddBatchItem);
    connect(clearBatchListBtn, &QPushButton::clicked, this, &BarcodeGen::onClearBatchList);
    connect(importBatchBtn, &QPushButton::clicked, this, &BarcodeGen::onImportBatch);
    connect(exportBatchBtn, &QPushButton::clicked, this, &BarcodeGen::onExportBatch);
}

void BarcodeGen::setupPreviewArea()
{
    previewGroup = new QGroupBox(tr("预览"), rightPanel);
    previewLayout = new QVBoxLayout(previewGroup);
    
    barcodePreview = new BarcodePreview;
    previewLayout->addWidget(barcodePreview);
    
    previewButtonLayout = new QHBoxLayout;
    saveBtn = new QPushButton(tr("保存"));
    copyBtn = new QPushButton(tr("复制"));
    addToHistoryBtn = new QPushButton(tr("添加到历史"));
    
    previewButtonLayout->addWidget(saveBtn);
    previewButtonLayout->addWidget(copyBtn);
    previewButtonLayout->addWidget(addToHistoryBtn);
    previewLayout->addLayout(previewButtonLayout);
    
    rightLayout->addWidget(previewGroup);
    
    // 连接信号
    connect(saveBtn, &QPushButton::clicked, this, &BarcodeGen::onSaveBarcode);
    connect(copyBtn, &QPushButton::clicked, this, &BarcodeGen::onCopyToClipboard);
    connect(addToHistoryBtn, &QPushButton::clicked, this, &BarcodeGen::onAddToHistory);
}

void BarcodeGen::setupHistoryArea()
{
    historyGroup = new QGroupBox(tr("历史记录"), rightPanel);
    historyLayout = new QVBoxLayout(historyGroup);

    historyTable = new QTableWidget;
    historyTable->setColumnCount(4);
    historyTable->setHorizontalHeaderLabels({tr("内容"), tr("类型"), tr("时间"), tr("状态")});
    historyTable->horizontalHeader()->setStretchLastSection(true);
    historyTable->setMinimumHeight(120);
    historyLayout->addWidget(historyTable, 1); // 添加stretch factor使其可拉伸

    historyButtonLayout = new QHBoxLayout;
    clearHistoryBtn = new QPushButton(tr("清空历史"));
    historyCountLabel = new QLabel(tr("共 0 条记录"));

    // 设置按钮的大小策略，让高度跟随内容
    clearHistoryBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    historyCountLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    historyButtonLayout->addWidget(clearHistoryBtn);
    historyButtonLayout->addStretch();
    historyButtonLayout->addWidget(historyCountLabel);
    historyLayout->addLayout(historyButtonLayout, 0); // 按钮布局不拉伸
    
    rightLayout->addWidget(historyGroup);
    
    // 连接信号
    connect(clearHistoryBtn, &QPushButton::clicked, this, &BarcodeGen::onClearHistory);
    connect(historyTable, &QTableWidget::itemClicked, this, &BarcodeGen::onHistoryItemClicked);
}

void BarcodeGen::setupStatusArea()
{
    statusLayout = new QHBoxLayout;
    statusLabel = new QLabel(tr("就绪"));
    progressBar = new QProgressBar;
    progressBar->setVisible(false);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
    
    mainLayout->addLayout(statusLayout);
}

void BarcodeGen::initializePresets()
{
    addPreset(tr("商品条码 (EAN-13)"), BarcodeType::EAN13, "1234567890123", tr("13位数字的商品条码"));
    addPreset(tr("商品条码 (UPC-A)"), BarcodeType::UPCA, "123456789012", tr("12位数字的商品条码"));
    addPreset(tr("通用条码 (Code 128)"), BarcodeType::Code128, "Hello World", tr("支持数字、字母和符号"));
    addPreset(tr("简单条码 (Code 39)"), BarcodeType::Code39, "HELLO123", tr("支持数字、大写字母和部分符号"));
    
    presetCombo->addItem(tr("选择预设模板"), -1);
    for (const auto &preset : m_presets) {
        presetCombo->addItem(preset.name, static_cast<int>(preset.type));
    }
}

void BarcodeGen::addPreset(const QString &name, BarcodeType type, const QString &templateText, const QString &description)
{
    BarcodePreset preset;
    preset.name = name;
    preset.type = type;
    preset.templateText = templateText;
    preset.description = description;
    m_presets.append(preset);
}

// 槽函数实现
void BarcodeGen::onGenerateBarcode()
{
    QString text = textEdit->text().trimmed();
    if (text.isEmpty()) {
        statusLabel->setText(tr("请输入要生成条形码的内容"));
        return;
    }
    
    BarcodeType type = static_cast<BarcodeType>(barcodeTypeCombo->currentData().toInt());
    BarcodeStyle style = getCurrentStyle();
    
    if (!isValidBarcodeData(text, type)) {
        statusLabel->setText(tr("输入内容不符合该条形码类型的要求"));
        QMessageBox::warning(this, tr("输入错误"), tr("输入内容不符合该条形码类型的要求"));
        return;
    }
    
    BarcodeItem item;
    item.text = text;
    item.type = type;
    item.style = style;
    item.createTime = QDateTime::currentDateTime();
    
    QPixmap barcode = generateBarcodeInternal(text, type, style);
    if (!barcode.isNull()) {
        item.barcodePixmap = barcode;
        item.isValid = true;
        m_currentBarcode = barcode;
        displayResult(item);
        statusLabel->setText(tr("条形码生成成功"));
    } else {
        item.isValid = false;
        item.errorMessage = "生成失败";
        statusLabel->setText(tr("条形码生成失败"));
    }
}

void BarcodeGen::onTextChanged()
{
    if (!m_updatingUI) {
        m_updateTimer->start();
    }
}

void BarcodeGen::onBarcodeTypeChanged()
{
    m_currentType = static_cast<BarcodeType>(barcodeTypeCombo->currentData().toInt());
    if (!textEdit->text().isEmpty()) {
        m_updateTimer->start();
    }
}

void BarcodeGen::onStyleChanged()
{
    if (!m_updatingUI && !textEdit->text().isEmpty()) {
        m_updateTimer->start();
    }
}

void BarcodeGen::onForegroundColorChanged(const QColor &color)
{
    m_currentStyle.foregroundColor = color;
    onStyleChanged();
}

void BarcodeGen::onBackgroundColorChanged(const QColor &color)
{
    m_currentStyle.backgroundColor = color;
    onStyleChanged();
}

void BarcodeGen::onPresetSelected()
{
    int index = presetCombo->currentIndex() - 1; // 减去"选择预设模板"项
    if (index >= 0 && index < m_presets.size()) {
        m_updatingUI = true;
        
        const BarcodePreset &preset = m_presets[index];
        textEdit->setText(preset.templateText);
        
        // 设置条形码类型
        for (int i = 0; i < barcodeTypeCombo->count(); ++i) {
            if (static_cast<BarcodeType>(barcodeTypeCombo->itemData(i).toInt()) == preset.type) {
                barcodeTypeCombo->setCurrentIndex(i);
                break;
            }
        }
        
        m_updatingUI = false;
        
        statusLabel->setText(QString("已选择预设: %1").arg(preset.name));
        m_updateTimer->start();
    }
}

void BarcodeGen::onBatchGenerate()
{
    QString text = batchTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        statusLabel->setText(tr("请输入批量处理的内容"));
        return;
    }
    
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        statusLabel->setText(tr("没有有效的输入内容"));
        return;
    }
    
    batchProgressBar->setVisible(true);
    batchProgressBar->setMaximum(lines.size());
    batchProgressBar->setValue(0);
    
    BarcodeType type = static_cast<BarcodeType>(barcodeTypeCombo->currentData().toInt());
    BarcodeStyle style = getCurrentStyle();
    
    m_batchItems.clear();
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        BarcodeItem item;
        item.text = line;
        item.type = type;
        item.style = style;
        item.createTime = QDateTime::currentDateTime();
        
        if (isValidBarcodeData(line, type)) {
            QPixmap barcode = generateBarcodeInternal(line, type, style);
            if (!barcode.isNull()) {
                item.barcodePixmap = barcode;
                item.isValid = true;
            } else {
                item.isValid = false;
                item.errorMessage = "生成失败";
            }
        } else {
            item.isValid = false;
            item.errorMessage = "格式不正确";
        }
        
        m_batchItems.append(item);
        batchProgressBar->setValue(i + 1);
        
        // 处理事件，避免界面卡死
        QApplication::processEvents();
    }
    
    batchProgressBar->setVisible(false);
    statusLabel->setText(QString("批量生成完成，共处理 %1 项").arg(m_batchItems.size()));
    
    // 显示批量处理结果对话框
    showBatchResults();
}

void BarcodeGen::onAddBatchItem()
{
    QString text = textEdit->text().trimmed();
    if (text.isEmpty()) {
        statusLabel->setText(tr("请先输入要添加的内容"));
        return;
    }
    
    QString currentText = batchTextEdit->toPlainText();
    if (!currentText.isEmpty()) {
        currentText += "\n";
    }
    currentText += text;
    batchTextEdit->setPlainText(currentText);
    
    statusLabel->setText(tr("已添加到批量列表"));
}

void BarcodeGen::onClearBatchList()
{
    batchTextEdit->clear();
    m_batchItems.clear();
    statusLabel->setText(tr("批量列表已清空"));
}

void BarcodeGen::onImportBatch()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "导入批量数据", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "文本文件 (*.txt);;CSV文件 (*.csv);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件"));
        return;
    }
    
    QTextStream in(&file);
    QStringList items;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            // 如果是CSV格式，取第一列
            if (fileName.endsWith(".csv")) {
                QStringList parts = line.split(',');
                if (!parts.isEmpty()) {
                    items.append(parts.first().trimmed());
                }
            } else {
                items.append(line);
            }
        }
    }
    
    if (!items.isEmpty()) {
        batchTextEdit->setPlainText(items.join('\n'));
        statusLabel->setText(QString("已导入 %1 项数据").arg(items.size()));
    } else {
        statusLabel->setText(tr("文件中没有有效数据"));
    }
}

void BarcodeGen::onExportBatch()
{
    if (m_batchItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有批量数据可导出"));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出批量结果", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/batch_barcodes.csv",
        "CSV文件 (*.csv)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法创建文件"));
        return;
    }
    
    QTextStream out(&file);
    
    // 写入标题行
    out << "内容,类型,状态,错误信息,创建时间\n";
    
    // 写入数据
    for (const BarcodeItem &item : m_batchItems) {
        out << QString("%1,%2,%3,%4,%5\n")
               .arg(item.text)
               .arg(getBarcodeTypeName(item.type))
               .arg(item.isValid ? "成功" : "失败")
               .arg(item.errorMessage)
               .arg(item.createTime.toString("yyyy-MM-dd hh:mm:ss"));
    }
    
    statusLabel->setText(QString("批量结果已导出到: %1").arg(fileName));
}

void BarcodeGen::onSaveBarcode()
{
    if (m_currentBarcode.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("没有可保存的条形码"));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "保存条形码", 
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/barcode.png",
        "PNG图片 (*.png);;JPEG图片 (*.jpg);;BMP图片 (*.bmp)");
    
    if (!fileName.isEmpty()) {
        if (m_currentBarcode.save(fileName)) {
            statusLabel->setText(QString("条形码已保存到: %1").arg(fileName));
        } else {
            statusLabel->setText(tr("保存失败"));
            QMessageBox::warning(this, tr("错误"), tr("保存条形码失败"));
        }
    }
}

void BarcodeGen::onCopyToClipboard()
{
    if (m_currentBarcode.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("没有可复制的条形码"));
        return;
    }
    
    QApplication::clipboard()->setPixmap(m_currentBarcode);
    statusLabel->setText(tr("条形码已复制到剪贴板"));
}

void BarcodeGen::onAddToHistory()
{
    if (m_currentBarcode.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("没有可添加的条形码"));
        return;
    }
    
    BarcodeItem item;
    item.text = textEdit->text();
    item.type = static_cast<BarcodeType>(barcodeTypeCombo->currentData().toInt());
    item.barcodePixmap = m_currentBarcode;
    item.style = getCurrentStyle();
    item.createTime = QDateTime::currentDateTime();
    item.isValid = true;
    
    addToHistory(item);
    statusLabel->setText(tr("已添加到历史记录"));
}

void BarcodeGen::onClearHistory()
{
    m_historyItems.clear();
    historyTable->setRowCount(0);
    historyCountLabel->setText(tr("共 0 条记录"));
    statusLabel->setText(tr("历史记录已清空"));
}

void BarcodeGen::onHistoryItemClicked(QTableWidgetItem *item)
{
    int row = item->row();
    if (row >= 0 && row < m_historyItems.size()) {
        const BarcodeItem &historyItem = m_historyItems[row];
        
        m_updatingUI = true;
        textEdit->setText(historyItem.text);
        
        // 设置条形码类型
        for (int i = 0; i < barcodeTypeCombo->count(); ++i) {
            if (static_cast<BarcodeType>(barcodeTypeCombo->itemData(i).toInt()) == historyItem.type) {
                barcodeTypeCombo->setCurrentIndex(i);
                break;
            }
        }
        
        // 设置样式
        foregroundColorBtn->setColor(historyItem.style.foregroundColor);
        backgroundColorBtn->setColor(historyItem.style.backgroundColor);
        widthSpinBox->setValue(historyItem.style.width);
        heightSpinBox->setValue(historyItem.style.height);
        showTextCheckBox->setChecked(historyItem.style.showText);
        fontSizeSpinBox->setValue(historyItem.style.fontSize);
        
        m_updatingUI = false;
        
        // 显示条形码
        barcodePreview->setBarcodeItem(historyItem);
        m_currentBarcode = historyItem.barcodePixmap;
        
        statusLabel->setText(tr("已加载历史记录"));
    }
}

// 核心功能实现
BarcodeStyle BarcodeGen::getCurrentStyle() const
{
    BarcodeStyle style;
    style.foregroundColor = foregroundColorBtn->color();
    style.backgroundColor = backgroundColorBtn->color();
    style.width = widthSpinBox->value();
    style.height = heightSpinBox->value();
    style.showText = showTextCheckBox->isChecked();
    style.fontSize = fontSizeSpinBox->value();
    style.margin = 10; // 固定边距
    return style;
}

void BarcodeGen::updateBarcodePreview()
{
    QString text = textEdit->text().trimmed();
    if (text.isEmpty()) {
        barcodePreview->clearBarcode();
        return;
    }
    
    BarcodeType type = static_cast<BarcodeType>(barcodeTypeCombo->currentData().toInt());
    BarcodeStyle style = getCurrentStyle();
    
    if (!isValidBarcodeData(text, type)) {
        barcodePreview->clearBarcode();
        return;
    }
    
    QPixmap barcode = generateBarcodeInternal(text, type, style);
    if (!barcode.isNull()) {
        barcodePreview->setBarcode(barcode);
        m_currentBarcode = barcode;
    } else {
        barcodePreview->clearBarcode();
    }
}

void BarcodeGen::displayResult(const BarcodeItem &item)
{
    if (item.isValid) {
        barcodePreview->setBarcodeItem(item);
    } else {
        barcodePreview->clearBarcode();
        statusLabel->setText(item.errorMessage);
    }
}

void BarcodeGen::clearResultDisplay()
{
    barcodePreview->clearBarcode();
    m_currentBarcode = QPixmap();
}

bool BarcodeGen::isValidBarcodeData(const QString &text, BarcodeType type) const
{
    if (text.isEmpty()) return false;
    
    switch (type) {
        case BarcodeType::EAN13:
            return text.length() == 13 && text.toULongLong() > 0;
        case BarcodeType::EAN8:
            return text.length() == 8 && text.toULongLong() > 0;
        case BarcodeType::UPCA:
            return text.length() == 12 && text.toULongLong() > 0;
        case BarcodeType::UPCE:
            return text.length() == 8 && text.toULongLong() > 0;
        case BarcodeType::Code39:
            return QRegularExpression("^[A-Z0-9 \\-\\.\\$\\/\\+\\%]+$").match(text).hasMatch();
        case BarcodeType::Code93:
        case BarcodeType::Code128:
        default:
            return true; // 这些类型支持大部分字符
    }
}

QString BarcodeGen::formatBarcodeData(const QString &text, BarcodeType type) const
{
    // 根据不同类型格式化数据
    switch (type) {
        case BarcodeType::Code39:
            return text.toUpper();
        default:
            return text;
    }
}

void BarcodeGen::addToHistory(const BarcodeItem &item)
{
    m_historyItems.prepend(item);
    
    // 限制历史记录数量
    if (m_historyItems.size() > 100) {
        m_historyItems.removeLast();
    }
    
    // 更新历史记录表格
    historyTable->setRowCount(m_historyItems.size());
    
    for (int i = 0; i < m_historyItems.size(); ++i) {
        const BarcodeItem &histItem = m_historyItems[i];
        
        historyTable->setItem(i, 0, new QTableWidgetItem(histItem.text));
        historyTable->setItem(i, 1, new QTableWidgetItem(getBarcodeTypeName(histItem.type)));
        historyTable->setItem(i, 2, new QTableWidgetItem(histItem.createTime.toString("yyyy-MM-dd hh:mm")));
        historyTable->setItem(i, 3, new QTableWidgetItem(histItem.isValid ? "成功" : "失败"));
    }
    
    historyCountLabel->setText(QString("共 %1 条记录").arg(m_historyItems.size()));
}

QString BarcodeGen::getBarcodeTypeName(BarcodeType type) const
{
    switch (type) {
        case BarcodeType::Code128: return "Code 128";
        case BarcodeType::Code39: return "Code 39";
        case BarcodeType::Code93: return "Code 93";
        case BarcodeType::EAN13: return "EAN-13";
        case BarcodeType::EAN8: return "EAN-8";
        case BarcodeType::UPCA: return "UPC-A";
        case BarcodeType::UPCE: return "UPC-E";
        case BarcodeType::ITF: return "ITF";
        case BarcodeType::Codabar: return "Codabar";
        case BarcodeType::DataMatrix: return "Data Matrix";
        case BarcodeType::PDF417: return "PDF417";
        default: return "Unknown";
    }
}

void BarcodeGen::showBatchResults()
{
    // 创建批量结果对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("批量生成结果"));
    dialog.resize(600, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QTableWidget *resultTable = new QTableWidget;
    resultTable->setColumnCount(3);
    resultTable->setHorizontalHeaderLabels({"内容", "状态", "错误信息"});
    resultTable->setRowCount(m_batchItems.size());
    
    for (int i = 0; i < m_batchItems.size(); ++i) {
        const BarcodeItem &item = m_batchItems[i];
        resultTable->setItem(i, 0, new QTableWidgetItem(item.text));
        resultTable->setItem(i, 1, new QTableWidgetItem(item.isValid ? "成功" : "失败"));
        resultTable->setItem(i, 2, new QTableWidgetItem(item.errorMessage));
    }
    
    resultTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(resultTable);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *saveAllBtn = new QPushButton(tr("保存全部"));
    QPushButton *closeBtn = new QPushButton(tr("关闭"));
    
    buttonLayout->addWidget(saveAllBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);
    
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(saveAllBtn, &QPushButton::clicked, [this, &dialog]() {
        saveBatchResults();
        dialog.accept();
    });
    
    dialog.exec();
}

void BarcodeGen::saveBatchResults()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择保存目录",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    
    if (dirPath.isEmpty()) return;
    
    int successCount = 0;
    for (int i = 0; i < m_batchItems.size(); ++i) {
        const BarcodeItem &item = m_batchItems[i];
        if (item.isValid && !item.barcodePixmap.isNull()) {
            QString fileName = QString("%1/barcode_%2_%3.png")
                .arg(dirPath)
                .arg(i + 1, 3, 10, QChar('0'))
                .arg(QString(item.text).replace(QRegularExpression("[^a-zA-Z0-9]"), "_"));
            
            if (item.barcodePixmap.save(fileName)) {
                successCount++;
            }
        }
    }
    
    statusLabel->setText(QString("批量保存完成，成功保存 %1 个文件").arg(successCount));
}

void BarcodeGen::loadSettings()
{
    // 加载设置（简化实现）
    QSettings settings;
    settings.beginGroup("BarcodeGen");
    
    // 恢复样式设置
    QColor fg = settings.value("foregroundColor", QColor(Qt::black)).value<QColor>();
    QColor bg = settings.value("backgroundColor", QColor(Qt::white)).value<QColor>();
    int width = settings.value("width", 300).toInt();
    int height = settings.value("height", 100).toInt();
    bool showText = settings.value("showText", true).toBool();
    int fontSize = settings.value("fontSize", 12).toInt();
    
    foregroundColorBtn->setColor(fg);
    backgroundColorBtn->setColor(bg);
    widthSpinBox->setValue(width);
    heightSpinBox->setValue(height);
    showTextCheckBox->setChecked(showText);
    fontSizeSpinBox->setValue(fontSize);
    
    settings.endGroup();
}

void BarcodeGen::saveSettings()
{
    // 保存设置（简化实现）
    QSettings settings;
    settings.beginGroup("BarcodeGen");
    
    settings.setValue("foregroundColor", foregroundColorBtn->color());
    settings.setValue("backgroundColor", backgroundColorBtn->color());
    settings.setValue("width", widthSpinBox->value());
    settings.setValue("height", heightSpinBox->value());
    settings.setValue("showText", showTextCheckBox->isChecked());
    settings.setValue("fontSize", fontSizeSpinBox->value());
    
    settings.endGroup();
}

void BarcodeGen::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void BarcodeGen::dropEvent(QDropEvent *event)
{
    QString text = event->mimeData()->text();
    if (!text.isEmpty()) {
        textEdit->setText(text);
        m_updateTimer->start();
    }
}

// 条形码生成核心算法（简化实现）
QPixmap BarcodeGen::generateBarcodeInternal(const QString &text, BarcodeType type, const BarcodeStyle &style)
{
    if (text.isEmpty()) return QPixmap();
    
    QPixmap pixmap(style.width, style.height);
    pixmap.fill(style.backgroundColor);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false); // 条形码不需要抗锯齿
    
    // 计算条形码绘制区域
    int margin = style.margin;
    int textHeight = style.showText ? style.fontSize + 5 : 0;
    int barcodeHeight = style.height - 2 * margin - textHeight;
    int barcodeWidth = style.width - 2 * margin;
    
    QRect barcodeRect(margin, margin, barcodeWidth, barcodeHeight);
    
    // 根据不同类型生成条形码模式
    QVector<int> pattern = generateBarcodePattern(text, type);
    if (pattern.isEmpty()) return QPixmap();
    
    // 绘制条形码
    drawBarcodePattern(painter, barcodeRect, pattern, style.foregroundColor);
    
    // 绘制文本
    if (style.showText) {
        QRect textRect(margin, margin + barcodeHeight + 2, barcodeWidth, textHeight);
        painter.setPen(style.foregroundColor);
        painter.setFont(QFont(style.fontFamily, style.fontSize));
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    // 显式结束QPainter以避免警告
    painter.end();

    return pixmap;
}

QVector<int> BarcodeGen::generateBarcodePattern(const QString &text, BarcodeType type)
{
    QVector<int> pattern;
    
    switch (type) {
        case BarcodeType::Code128:
            pattern = generateCode128Pattern(text);
            break;
        case BarcodeType::Code39:
            pattern = generateCode39Pattern(text);
            break;
        case BarcodeType::EAN13:
            pattern = generateEAN13Pattern(text);
            break;
        case BarcodeType::EAN8:
            pattern = generateEAN8Pattern(text);
            break;
        case BarcodeType::UPCA:
            pattern = generateUPCAPattern(text);
            break;
        default:
            // 默认使用 Code 128
            pattern = generateCode128Pattern(text);
            break;
    }
    
    return pattern;
}

void BarcodeGen::drawBarcodePattern(QPainter &painter, const QRect &rect, const QVector<int> &pattern, const QColor &color)
{
    if (pattern.isEmpty()) return;
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    
    int totalWidth = 0;
    for (int width : pattern) {
        totalWidth += width;
    }
    
    if (totalWidth == 0) return;
    
    double scale = double(rect.width()) / totalWidth;
    double x = rect.x();
    
    for (int i = 0; i < pattern.size(); ++i) {
        double barWidth = pattern[i] * scale;
        
        // 奇数索引绘制黑条，偶数索引为空白
        if (i % 2 == 0) {
            QRectF barRect(x, rect.y(), barWidth, rect.height());
            painter.drawRect(barRect);
        }
        
        x += barWidth;
    }
}

// Code 128 条形码模式生成（简化版）
QVector<int> BarcodeGen::generateCode128Pattern(const QString &text)
{
    QVector<int> pattern;
    
    // 起始码 (11010010000)
    pattern << 2 << 1 << 2 << 1 << 2 << 3 << 1;
    
    // 为每个字符生成模式（简化实现）
    for (QChar ch : text) {
        int ascii = ch.unicode();
        
        // 简化的字符编码模式
        if (ascii >= 32 && ascii <= 126) {
            // 根据ASCII值生成简单的模式
            int code = ascii - 32;
            pattern << (code % 4 + 1) << 1 << (code % 3 + 1) << 1 << (code % 4 + 1) << 1 << (code % 3 + 2);
        }
    }
    
    // 校验码（简化）
    pattern << 2 << 1 << 3 << 1 << 2 << 1 << 1;
    
    // 结束码 (1100011101011)
    pattern << 2 << 3 << 1 << 1 << 2 << 1 << 2;
    
    return pattern;
}

// Code 39 条形码模式生成（简化版）
QVector<int> BarcodeGen::generateCode39Pattern(const QString &text)
{
    QVector<int> pattern;
    
    // Code 39 字符表（简化版）
    QHash<QChar, QVector<int>> code39Table;
    code39Table['0'] = {1, 1, 2, 2, 1, 1, 2, 1, 1};
    code39Table['1'] = {2, 1, 1, 2, 1, 1, 2, 1, 1};
    code39Table['2'] = {1, 1, 1, 2, 2, 1, 2, 1, 1};
    code39Table['3'] = {2, 1, 1, 2, 2, 1, 1, 1, 1};
    code39Table['4'] = {1, 1, 2, 2, 1, 1, 1, 1, 2};
    code39Table['5'] = {2, 1, 2, 2, 1, 1, 1, 1, 1};
    code39Table['6'] = {1, 1, 1, 2, 1, 1, 1, 1, 2};
    code39Table['7'] = {1, 1, 2, 2, 1, 1, 1, 1, 1};
    code39Table['8'] = {2, 1, 1, 2, 1, 1, 1, 1, 2};
    code39Table['9'] = {1, 1, 1, 2, 2, 1, 1, 1, 2};
    code39Table['A'] = {2, 1, 1, 1, 1, 2, 2, 1, 1};
    code39Table['B'] = {1, 1, 1, 1, 2, 2, 2, 1, 1};
    code39Table['C'] = {2, 1, 1, 1, 2, 2, 1, 1, 1};
    code39Table[' '] = {1, 1, 2, 1, 1, 2, 1, 2, 1};
    code39Table['*'] = {1, 1, 2, 1, 1, 2, 1, 1, 2}; // 起始/结束符
    
    // 起始符
    pattern << code39Table['*'];
    pattern << 1; // 间隔
    
    // 字符编码
    QString upperText = text.toUpper();
    for (QChar ch : upperText) {
        if (code39Table.contains(ch)) {
            pattern << code39Table[ch];
            pattern << 1; // 字符间隔
        }
    }
    
    // 结束符
    pattern << code39Table['*'];
    
    return pattern;
}

// EAN-13 条形码模式生成（简化版）
QVector<int> BarcodeGen::generateEAN13Pattern(const QString &text)
{
    if (text.length() != 13) return QVector<int>();
    
    QVector<int> pattern;
    
    // 起始码
    pattern << 1 << 1 << 1;
    
    // 左侧数据区（简化实现）
    for (int i = 1; i <= 6; ++i) {
        int digit = text.mid(i, 1).toInt();
        // 简化的左侧编码
        pattern << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 4 + 1);
    }
    
    // 中间分隔符
    pattern << 1 << 1 << 1 << 1 << 1;
    
    // 右侧数据区（简化实现）
    for (int i = 7; i <= 12; ++i) {
        int digit = text.mid(i, 1).toInt();
        // 简化的右侧编码
        pattern << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1);
    }
    
    // 结束码
    pattern << 1 << 1 << 1;
    
    return pattern;
}

// EAN-8 条形码模式生成（简化版）
QVector<int> BarcodeGen::generateEAN8Pattern(const QString &text)
{
    if (text.length() != 8) return QVector<int>();
    
    QVector<int> pattern;
    
    // 起始码
    pattern << 1 << 1 << 1;
    
    // 左侧数据区
    for (int i = 0; i < 4; ++i) {
        int digit = text.mid(i, 1).toInt();
        pattern << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 4 + 1);
    }
    
    // 中间分隔符
    pattern << 1 << 1 << 1 << 1 << 1;
    
    // 右侧数据区
    for (int i = 4; i < 8; ++i) {
        int digit = text.mid(i, 1).toInt();
        pattern << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1);
    }
    
    // 结束码
    pattern << 1 << 1 << 1;
    
    return pattern;
}

// UPC-A 条形码模式生成（简化版）
QVector<int> BarcodeGen::generateUPCAPattern(const QString &text)
{
    if (text.length() != 12) return QVector<int>();
    
    QVector<int> pattern;
    
    // 起始码
    pattern << 1 << 1 << 1;
    
    // 左侧数据区
    for (int i = 0; i < 6; ++i) {
        int digit = text.mid(i, 1).toInt();
        pattern << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 4 + 1);
    }
    
    // 中间分隔符
    pattern << 1 << 1 << 1 << 1 << 1;
    
    // 右侧数据区
    for (int i = 6; i < 12; ++i) {
        int digit = text.mid(i, 1).toInt();
        pattern << (digit % 3 + 1) << (digit % 2 + 1) << (digit % 3 + 1) << (digit % 2 + 1);
    }
    
    // 结束码
    pattern << 1 << 1 << 1;
    
    return pattern;
}


