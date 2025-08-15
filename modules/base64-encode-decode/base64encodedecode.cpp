#include "base64encodedecode.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QInputDialog>
#include <QWheelEvent>

REGISTER_DYNAMICOBJECT(Base64EncodeDecode);

// ImagePreviewLabel 实现
ImagePreviewLabel::ImagePreviewLabel(QWidget *parent)
    : QLabel(parent)
{
    setMinimumSize(200, 150);
    setStyleSheet("border: 2px dashed #ccc; background-color: #f9f9f9; border-radius: 8px;");
    setAlignment(Qt::AlignCenter);
    setText("🖼️ 拖拽图片到此处\n👆 或点击选择图片");
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    
    setWordWrap(true);
}

void ImagePreviewLabel::setImageFromPath(const QString &imagePath)
{
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        originalPixmap = pixmap;
        updateDisplayedPixmap();
        setStyleSheet("border: 2px solid #4CAF50; background-color: white; border-radius: 8px;");
    }
}

void ImagePreviewLabel::setImageFromBase64(const QString &base64Data)
{
    QByteArray imageData = QByteArray::fromBase64(base64Data.toUtf8());
    QPixmap pixmap;
    if (pixmap.loadFromData(imageData)) {
        originalPixmap = pixmap;
        updateDisplayedPixmap();
        setStyleSheet("border: 2px solid #4CAF50; background-color: white; border-radius: 8px;");
    }
}

void ImagePreviewLabel::clearImage()
{
    originalPixmap = QPixmap();
    clear();
    setText("🖼️ 拖拽图片到此处\n👆 或点击选择图片");
    setStyleSheet("border: 2px dashed #ccc; background-color: #f9f9f9; border-radius: 8px;");
}

void ImagePreviewLabel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString fileName = urls.first().toLocalFile();
            if (isImageFile(fileName)) {
                event->acceptProposedAction();
                setStyleSheet("border: 2px solid #2196F3; background-color: #E3F2FD; border-radius: 8px;");
                return;
            }
        }
    }
    event->ignore();
}

void ImagePreviewLabel::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString fileName = urls.first().toLocalFile();
        if (isImageFile(fileName)) {
            emit imageDropped(fileName);
            event->acceptProposedAction();
        }
    }
    setStyleSheet("border: 2px dashed #ccc; background-color: #f9f9f9; border-radius: 8px;");
}

void ImagePreviewLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit imageClicked();
    }
    QLabel::mousePressEvent(event);
}

void ImagePreviewLabel::updateDisplayedPixmap()
{
    if (!originalPixmap.isNull()) {
        QPixmap scaledPixmap = originalPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }
}

bool ImagePreviewLabel::isImageFile(const QString &fileName)
{
    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "tiff", "webp"};
    QFileInfo fileInfo(fileName);
    return imageExtensions.contains(fileInfo.suffix().toLower());
}

// Base64EncodeDecode 主类实现
Base64EncodeDecode::Base64EncodeDecode() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    applyStyles();
    showMessage("Base64 编码解码器就绪");
}

void Base64EncodeDecode::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 创建标签页
    mainTabWidget = new QTabWidget();
    
    // 文本编码解码页
    setupTextTab();
    mainTabWidget->addTab(textTab, "📝 文本编码/解码");
    
    // 图片编码解码页
    setupImageTab();
    mainTabWidget->addTab(imageTab, "🖼️ 图片编码/解码");
    
    mainLayout->addWidget(mainTabWidget);
}

void Base64EncodeDecode::setupTextTab()
{
    textTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(textTab);
    
    // 输入区域
    QGroupBox *inputGroup = new QGroupBox("输入文本");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    
    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText("输入要编码的文本...");
    inputTextEdit->setAcceptRichText(false);
    inputLayout->addWidget(inputTextEdit);
    
    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    encodeButton = new QPushButton("📤 编码");
    decodeButton = new QPushButton("📥 解码");
    clearButton = new QPushButton("🗑️ 清空");
    
    buttonLayout->addWidget(encodeButton);
    buttonLayout->addWidget(decodeButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    
    // 输出区域
    QGroupBox *outputGroup = new QGroupBox("输出结果");
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    
    outputTextEdit = new QTextEdit();
    outputTextEdit->setPlaceholderText("编码/解码结果将显示在这里...");
    outputTextEdit->setAcceptRichText(false);
    outputLayout->addWidget(outputTextEdit);
    
    // 复制按钮
    QHBoxLayout *copyLayout = new QHBoxLayout();
    copyButton = new QPushButton("📋 复制结果");
    copyLayout->addWidget(copyButton);
    copyLayout->addStretch();
    
    layout->addWidget(inputGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(outputGroup);
    layout->addLayout(copyLayout);
    
    // 连接信号
    connect(encodeButton, &QPushButton::clicked, this, &Base64EncodeDecode::onEncodeText);
    connect(decodeButton, &QPushButton::clicked, this, &Base64EncodeDecode::onDecodeText);
    connect(clearButton, &QPushButton::clicked, this, &Base64EncodeDecode::onClearText);
    connect(copyButton, &QPushButton::clicked, this, &Base64EncodeDecode::onCopyResult);
}

void Base64EncodeDecode::setupImageTab()
{
    imageTab = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(imageTab);
    
    // 左侧图片预览区域
    QVBoxLayout *leftLayout = new QVBoxLayout();
    
    QLabel *previewLabel = new QLabel("图片预览");
    previewLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    leftLayout->addWidget(previewLabel);
    
    imagePreviewLabel = new ImagePreviewLabel();
    imageScrollArea = new QScrollArea();
    imageScrollArea->setWidget(imagePreviewLabel);
    imageScrollArea->setWidgetResizable(true);
    imageScrollArea->setMinimumSize(300, 250);
    leftLayout->addWidget(imageScrollArea);
    
    // 图片信息
    imageInfoLabel = new QLabel("未选择图片");
    imageInfoLabel->setStyleSheet("color: #666; font-size: 12px; padding: 5px;");
    leftLayout->addWidget(imageInfoLabel);
    
    // 图片操作按钮
    QHBoxLayout *imageButtonLayout = new QHBoxLayout();
    selectImageButton = new QPushButton("📁 选择图片");
    saveImageButton = new QPushButton("💾 保存图片");
    saveImageButton->setEnabled(false);
    
    imageButtonLayout->addWidget(selectImageButton);
    imageButtonLayout->addWidget(saveImageButton);
    imageButtonLayout->addStretch();
    leftLayout->addLayout(imageButtonLayout);
    
    // 右侧Base64文本区域
    QVBoxLayout *rightLayout = new QVBoxLayout();
    
    QLabel *base64Label = new QLabel("Base64 编码");
    base64Label->setStyleSheet("font-weight: bold; font-size: 14px;");
    rightLayout->addWidget(base64Label);
    
    imageBase64Edit = new QTextEdit();
    imageBase64Edit->setPlaceholderText("Base64编码数据将显示在这里...\n\n手动输入Base64数据然后点击\"解码预览\"按钮");
    imageBase64Edit->setAcceptRichText(false);
    rightLayout->addWidget(imageBase64Edit);
    
    // Base64操作按钮
    QHBoxLayout *base64ButtonLayout = new QHBoxLayout();
    decodePreviewButton = new QPushButton("🔍 解码预览");
    QPushButton *copyBase64Button = new QPushButton("📋 复制Base64");
    
    decodePreviewButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    
    base64ButtonLayout->addWidget(decodePreviewButton);
    base64ButtonLayout->addWidget(copyBase64Button);
    base64ButtonLayout->addStretch();
    rightLayout->addLayout(base64ButtonLayout);
    
    // 添加到主布局
    layout->addLayout(leftLayout, 1);
    layout->addLayout(rightLayout, 1);
    
    // 连接信号
    connect(imagePreviewLabel, &ImagePreviewLabel::imageDropped, this, &Base64EncodeDecode::onImageDropped);
    connect(imagePreviewLabel, &ImagePreviewLabel::imageClicked, this, &Base64EncodeDecode::onImageSelectClicked);
    connect(selectImageButton, &QPushButton::clicked, this, &Base64EncodeDecode::onImageSelectClicked);
    connect(saveImageButton, &QPushButton::clicked, this, &Base64EncodeDecode::onSaveImageClicked);
    connect(decodePreviewButton, &QPushButton::clicked, this, &Base64EncodeDecode::onImageBase64Changed);
    connect(copyBase64Button, &QPushButton::clicked, [this]() {
        QString base64Text = imageBase64Edit->toPlainText();
        if (!base64Text.isEmpty()) {
            QApplication::clipboard()->setText(base64Text);
            showMessage("Base64数据已复制到剪贴板");
        }
    });
}

void Base64EncodeDecode::onEncodeText()
{
    QString inputText = inputTextEdit->toPlainText();
    if (inputText.isEmpty()) {
        showMessage("请输入要编码的文本", true);
        return;
    }
    
    QByteArray textData = inputText.toUtf8();
    QString base64String = textData.toBase64();
    outputTextEdit->setPlainText(base64String);
    showMessage("文本编码完成");
}

void Base64EncodeDecode::onDecodeText()
{
    QString base64Text = inputTextEdit->toPlainText();
    if (base64Text.isEmpty()) {
        showMessage("请输入要解码的Base64文本", true);
        return;
    }
    
    QByteArray base64Data = QByteArray::fromBase64(base64Text.toUtf8());
    QString decodedText = QString::fromUtf8(base64Data);
    outputTextEdit->setPlainText(decodedText);
    showMessage("文本解码完成");
}

void Base64EncodeDecode::onClearText()
{
    inputTextEdit->clear();
    outputTextEdit->clear();
    showMessage("已清空文本");
}

void Base64EncodeDecode::onCopyResult()
{
    QString result = outputTextEdit->toPlainText();
    if (!result.isEmpty()) {
        QApplication::clipboard()->setText(result);
        showMessage("结果已复制到剪贴板");
    }
}

void Base64EncodeDecode::onImageDropped(const QString &imagePath)
{
    loadImageFromPath(imagePath);
}

void Base64EncodeDecode::onImageSelectClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择图片",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tiff *.webp);;所有文件 (*.*)"
    );
    
    if (!fileName.isEmpty()) {
        loadImageFromPath(fileName);
    }
}

void Base64EncodeDecode::onImageBase64Changed()
{
    QString base64Text = imageBase64Edit->toPlainText().trimmed();
    if (base64Text.isEmpty()) {
        showMessage("请输入Base64数据", true);
        return;
    }
    
    // 移除可能的data URL前缀
    if (base64Text.startsWith("data:image/")) {
        int commaIndex = base64Text.indexOf(',');
        if (commaIndex != -1) {
            base64Text = base64Text.mid(commaIndex + 1);
        }
    }
    
    QByteArray imageData = QByteArray::fromBase64(base64Text.toUtf8());
    QPixmap pixmap;
    
    if (pixmap.loadFromData(imageData)) {
        imagePreviewLabel->setImageFromBase64(base64Text);
        saveImageButton->setEnabled(true);
        updateImageInfo(QString("解码图片 (%1x%2)").arg(pixmap.width()).arg(pixmap.height()), imageData.size());
        showMessage("Base64解码成功");
    } else {
        showMessage("无效的Base64图片数据", true);
    }
}

void Base64EncodeDecode::onSaveImageClicked()
{
    QString base64Text = imageBase64Edit->toPlainText().trimmed();
    if (base64Text.isEmpty()) {
        showMessage("没有可保存的图片数据", true);
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存图片",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/decoded_image.png",
        "PNG图片 (*.png);;JPEG图片 (*.jpg);;所有文件 (*.*)"
    );
    
    if (!fileName.isEmpty()) {
        // 移除可能的data URL前缀
        if (base64Text.startsWith("data:image/")) {
            int commaIndex = base64Text.indexOf(',');
            if (commaIndex != -1) {
                base64Text = base64Text.mid(commaIndex + 1);
            }
        }
        
        QByteArray imageData = QByteArray::fromBase64(base64Text.toUtf8());
        QFile file(fileName);
        
        if (file.open(QIODevice::WriteOnly)) {
            file.write(imageData);
            file.close();
            showMessage("图片保存成功: " + fileName);
        } else {
            showMessage("保存图片失败", true);
        }
    }
}

void Base64EncodeDecode::loadImageFromPath(const QString &imagePath)
{
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        showMessage("无法打开图片文件", true);
        return;
    }
    
    QByteArray imageData = file.readAll();
    file.close();
    
    // 检查文件大小
    if (imageData.size() > 10 * 1024 * 1024) { // 10MB
        showMessage("图片文件过大，建议小于10MB", true);
        return;
    }
    
    QString base64String = imageData.toBase64();
    imageBase64Edit->setPlainText(base64String);
    
    imagePreviewLabel->setImageFromPath(imagePath);
    saveImageButton->setEnabled(true);
    
    QFileInfo fileInfo(imagePath);
    updateImageInfo(fileInfo.fileName(), imageData.size());
    
    showMessage("图片加载并编码完成");
}

void Base64EncodeDecode::updateImageInfo(const QString &fileName, qint64 fileSize)
{
    QString sizeText;
    if (fileSize < 1024) {
        sizeText = QString::number(fileSize) + " bytes";
    } else if (fileSize < 1024 * 1024) {
        sizeText = QString::number(fileSize / 1024.0, 'f', 1) + " KB";
    } else {
        sizeText = QString::number(fileSize / (1024.0 * 1024.0), 'f', 2) + " MB";
    }
    
    imageInfoLabel->setText(QString("文件: %1\n大小: %2").arg(fileName).arg(sizeText));
}

void Base64EncodeDecode::applyStyles()
{
    setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    font-size: 13px;"
        "    color: #2c3e50;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 8px;"
        "    margin-top: 1ex;"
        "    padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    background-color: white;"
        "}"
        
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 8px 16px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #21618c;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #bdc3c7;"
        "    color: #7f8c8d;"
        "}"
        
        "QTextEdit {"
        "    border: 2px solid #ecf0f1;"
        "    border-radius: 6px;"
        "    padding: 8px;"
        "    font-family: 'Consolas', 'Monaco', monospace;"
        "    font-size: 12px;"
        "    background-color: white;"
        "}"
        "QTextEdit:focus {"
        "    border-color: #3498db;"
        "}"
    );
}

void Base64EncodeDecode::showMessage(const QString &message, bool isError)
{
    // 这里可以添加状态栏或其他提示方式
    qDebug() << (isError ? "Error:" : "Info:") << message;
}

#include "base64encodedecode.moc"
