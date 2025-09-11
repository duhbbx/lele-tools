#include "faviconproduction.h"
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QClipboard>
#include <QPainter>

REGISTER_DYNAMICOBJECT(FaviconProduction);

FaviconProduction::FaviconProduction() : QWidget(nullptr), DynamicObjectBase(), isGenerating(false)
{
    initializeSizes();
    setupUI();
    
    connect(selectImageBtn, &QPushButton::clicked, this, &FaviconProduction::onSelectImage);
    connect(generateBtn, &QPushButton::clicked, this, &FaviconProduction::onGenerateFavicons);
    connect(selectOutputBtn, &QPushButton::clicked, this, &FaviconProduction::onSelectOutputDir);
    connect(clearBtn, &QPushButton::clicked, this, &FaviconProduction::onClearAll);
    connect(generateHtmlBtn, &QPushButton::clicked, this, &FaviconProduction::onGenerateHtml);
    
    outputDirectory = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/favicons";
    outputPathEdit->setText(outputDirectory);
}

FaviconProduction::~FaviconProduction() {}

void FaviconProduction::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    setupInputArea();
    setupSizeSelection();
    setupPreviewArea();
    setupOutputArea();
    
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->addWidget(inputGroup);
    leftLayout->addWidget(sizeGroup);
    
    QWidget* rightWidget = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->addWidget(previewGroup);
    rightLayout->addWidget(outputGroup);
    
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(rightWidget);
    mainLayout->addWidget(mainSplitter);
}

void FaviconProduction::setupInputArea()
{
    inputGroup = new QGroupBox("📁 源图片");
    inputLayout = new QVBoxLayout(inputGroup);
    
    inputButtonLayout = new QHBoxLayout();
    selectImageBtn = new QPushButton("📂 选择图片");
    clearBtn = new QPushButton("🗑️ 清空");
    
    inputButtonLayout->addWidget(selectImageBtn);
    inputButtonLayout->addWidget(clearBtn);
    inputButtonLayout->addStretch();
    
    imagePathLabel = new QLabel("未选择图片");
    imageSizeLabel = new QLabel("图片尺寸: 0 x 0");
    imagePreviewLabel = new QLabel("预览");
    imagePreviewLabel->setFixedSize(120, 120);
    imagePreviewLabel->setStyleSheet("border: 2px dashed #cccccc; border-radius: 0px;");
    imagePreviewLabel->setAlignment(Qt::AlignCenter);
    imagePreviewLabel->setScaledContents(true);
    
    inputLayout->addLayout(inputButtonLayout);
    inputLayout->addWidget(imagePathLabel);
    inputLayout->addWidget(imageSizeLabel);
    inputLayout->addWidget(imagePreviewLabel);
}

void FaviconProduction::setupSizeSelection()
{
    sizeGroup = new QGroupBox("📐 尺寸选择");
    sizeLayout = new QVBoxLayout(sizeGroup);
    
    sizeListWidget = new QListWidget();
    sizeListWidget->setMaximumHeight(200);
    
    for (const FaviconSize& size : faviconSizes) {
        auto* item = new QListWidgetItem(
            QString("%1x%2 - %3").arg(size.width).arg(size.height).arg(size.description)
        );
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(size.isSelected ? Qt::Checked : Qt::Unchecked);
        sizeListWidget->addItem(item);
    }
    
    sizeButtonLayout = new QHBoxLayout();
    selectAllBtn = new QPushButton("全选");
    deselectAllBtn = new QPushButton("全不选");
    previewBtn = new QPushButton("🔍 预览");
    previewBtn->setEnabled(false);
    
    connect(selectAllBtn, &QPushButton::clicked, [this]() {
        for (int i = 0; i < sizeListWidget->count(); ++i) {
            sizeListWidget->item(i)->setCheckState(Qt::Checked);
        }
    });
    connect(deselectAllBtn, &QPushButton::clicked, [this]() {
        for (int i = 0; i < sizeListWidget->count(); ++i) {
            sizeListWidget->item(i)->setCheckState(Qt::Unchecked);
        }
    });
    
    sizeButtonLayout->addWidget(selectAllBtn);
    sizeButtonLayout->addWidget(deselectAllBtn);
    sizeButtonLayout->addWidget(previewBtn);
    sizeButtonLayout->addStretch();
    
    sizeLayout->addWidget(sizeListWidget);
    sizeLayout->addLayout(sizeButtonLayout);
}

void FaviconProduction::setupPreviewArea()
{
    previewGroup = new QGroupBox("👁️ 预览效果");
    auto* previewMainLayout = new QVBoxLayout(previewGroup);
    
    previewScrollArea = new QScrollArea();
    previewScrollArea->setMinimumHeight(150);
    previewWidget = new QWidget();
    previewGridLayout = new QGridLayout(previewWidget);
    previewScrollArea->setWidget(previewWidget);
    
    previewMainLayout->addWidget(previewScrollArea);
}

void FaviconProduction::setupOutputArea()
{
    outputGroup = new QGroupBox("💾 输出设置");
    outputLayout = new QVBoxLayout(outputGroup);
    
    outputPathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel("输出目录:");
    outputPathEdit = new QLineEdit();
    selectOutputBtn = new QPushButton("📁 浏览");
    
    outputPathLayout->addWidget(pathLabel);
    outputPathLayout->addWidget(outputPathEdit);
    outputPathLayout->addWidget(selectOutputBtn);
    
    outputButtonLayout = new QHBoxLayout();
    generateBtn = new QPushButton("🚀 生成Favicon");
    generateBtn->setEnabled(false);
    generateHtmlBtn = new QPushButton("📋 生成HTML代码");
    generateHtmlBtn->setEnabled(false);
    
    outputButtonLayout->addWidget(generateBtn);
    outputButtonLayout->addWidget(generateHtmlBtn);
    outputButtonLayout->addStretch();
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    statusLabel = new QLabel("就绪");
    
    codeTabWidget = new QTabWidget();
    codeTabWidget->setMaximumHeight(120);
    htmlCodeEdit = new QTextEdit();
    htmlCodeEdit->setReadOnly(true);
    manifestCodeEdit = new QTextEdit();
    manifestCodeEdit->setReadOnly(true);
    
    codeTabWidget->addTab(htmlCodeEdit, "HTML代码");
    codeTabWidget->addTab(manifestCodeEdit, "Manifest");
    
    outputLayout->addLayout(outputPathLayout);
    outputLayout->addLayout(outputButtonLayout);
    outputLayout->addWidget(progressBar);
    outputLayout->addWidget(statusLabel);
    outputLayout->addWidget(codeTabWidget);
}

void FaviconProduction::initializeSizes()
{
    faviconSizes = {
        {16, 16, "标准favicon", true},
        {32, 32, "高清favicon", true},
        {48, 48, "Windows站点图标", false},
        {57, 57, "iPhone", false},
        {72, 72, "iPad", false},
        {96, 96, "Android Chrome", false},
        {144, 144, "Windows磁贴", false},
        {180, 180, "iPhone 6 Plus", false},
        {192, 192, "Android Chrome", false}
    };
}

void FaviconProduction::onSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (!filePath.isEmpty()) {
        loadImage(filePath);
    }
}

void FaviconProduction::onGenerateFavicons()
{
    if (currentImagePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择源图片");
        return;
    }
    
    QList<int> selectedIndices;
    for (int i = 0; i < sizeListWidget->count(); ++i) {
        if (sizeListWidget->item(i)->checkState() == Qt::Checked) {
            selectedIndices.append(i);
        }
    }
    
    if (selectedIndices.isEmpty()) {
        QMessageBox::warning(this, "错误", "请至少选择一个尺寸");
        return;
    }

    if (const QDir dir; !dir.exists(outputDirectory)) {
        dir.mkpath(outputDirectory);
    }
    
    progressBar->setVisible(true);
    progressBar->setMaximum(selectedIndices.size());
    
    for (int i = 0; i < selectedIndices.size(); ++i) {
        generateFavicon(faviconSizes[selectedIndices[i]], outputDirectory);
        progressBar->setValue(i + 1);
        QApplication::processEvents();
    }
    
    progressBar->setVisible(false);
    generateHtmlBtn->setEnabled(true);
    updateStatus(QString("成功生成 %1 个favicon文件").arg(selectedIndices.size()), false);
    onGenerateHtml();
}

void FaviconProduction::onSelectOutputDir()
{
    if (const QString dir = QFileDialog::getExistingDirectory(this, "选择输出目录", outputDirectory); !dir.isEmpty()) {
        outputDirectory = dir;
        outputPathEdit->setText(dir);
    }
}

void FaviconProduction::onClearAll()
{
    currentImagePath.clear();
    originalPixmap = QPixmap();
    imagePathLabel->setText("未选择图片");
    imageSizeLabel->setText("图片尺寸: 0 x 0");
    imagePreviewLabel->clear();
    imagePreviewLabel->setText("预览");
    generateBtn->setEnabled(false);
    generateHtmlBtn->setEnabled(false);
    htmlCodeEdit->clear();
    manifestCodeEdit->clear();
    updateStatus("已清空所有内容", false);
}

void FaviconProduction::onGenerateHtml()
{
    QString htmlCode = generateHtmlCode();
    htmlCodeEdit->setPlainText(htmlCode);
    
    QString manifestCode = R"({
  "name": "Your App Name",
  "short_name": "App",
  "icons": [
    {
      "src": "favicon-192x192.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
})";
    manifestCodeEdit->setPlainText(manifestCode);
    updateStatus("HTML代码已生成", false);
}

void FaviconProduction::loadImage(const QString& filePath)
{
    const QPixmap pixmap(filePath);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "错误", "无法加载图片文件");
        return;
    }
    
    currentImagePath = filePath;
    originalPixmap = pixmap;
    
    imagePathLabel->setText(QFileInfo(filePath).fileName());
    imageSizeLabel->setText(QString("图片尺寸: %1 x %2").arg(pixmap.width()).arg(pixmap.height()));

    const QPixmap preview = pixmap.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    imagePreviewLabel->setPixmap(preview);
    
    generateBtn->setEnabled(true);
    updateStatus("图片加载完成", false);
}

void FaviconProduction::generateFavicon(const FaviconSize& size, const QString& outputDir)
{
    if (originalPixmap.isNull()) return;
    
    QPixmap scaledPixmap = originalPixmap.scaled(size.width, size.height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    if (scaledPixmap.width() != size.width || scaledPixmap.height() != size.height) {
        QPixmap squarePixmap(size.width, size.height);
        squarePixmap.fill(Qt::transparent);
        
        QPainter painter(&squarePixmap);
        int x = (size.width - scaledPixmap.width()) / 2;
        int y = (size.height - scaledPixmap.height()) / 2;
        painter.drawPixmap(x, y, scaledPixmap);
        scaledPixmap = squarePixmap;
    }
    
    QString fileName = QString("favicon-%1x%2.png").arg(size.width).arg(size.height);
    QString filePath = QDir(outputDir).filePath(fileName);
    scaledPixmap.save(filePath, "PNG");
}

QString FaviconProduction::generateHtmlCode()
{
    QString html = "<!-- Favicon 代码 -->\n";
    html += "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"favicon-16x16.png\">\n";
    html += "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"favicon-32x32.png\">\n";
    html += "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"favicon-180x180.png\">\n";
    html += "<link rel=\"manifest\" href=\"site.webmanifest\">\n";
    return html;
}

void FaviconProduction::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? "color: red;" : "color: green;");
}

// 空实现避免编译错误
void FaviconProduction::onPreviewSize() {}
void FaviconProduction::onSizeSelectionChanged() {}
void FaviconProduction::updatePreview() {}