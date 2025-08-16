#include "imagecompression.h"
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>

REGISTER_DYNAMICOBJECT(ImageCompression);

// 简化的工作线程实现
void ImageProcessWorker::processImage(const ProcessParams& params)
{
    emit progressUpdated(50);
    QImage image(params.inputPath);
    if (image.isNull()) {
        emit imageProcessed(false, "无法加载图片", 0, 0);
        return;
    }
    
    // 简单的尺寸调整
    if (params.width > 0 && params.height > 0) {
        image = image.scaled(params.width, params.height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    bool success = image.save(params.outputPath, params.format.toUtf8().data(), params.quality);
    emit progressUpdated(100);
    
    if (success) {
        QFileInfo newFile(params.outputPath);
        emit imageProcessed(true, "图片处理完成", QFileInfo(params.inputPath).size(), newFile.size());
    } else {
        emit imageProcessed(false, "保存失败", 0, 0);
    }
}

// 简化的图片查看器
ImageViewer::ImageViewer(QWidget* parent) : QGraphicsView(parent), cropEnabled(false)
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
}

void ImageViewer::setImage(const QPixmap& pixmap)
{
    scene->clear();
    pixmapItem = scene->addPixmap(pixmap);
    fitInView(pixmapItem, Qt::KeepAspectRatio);
}

void ImageViewer::setCropEnabled(bool enabled) { cropEnabled = enabled; }
void ImageViewer::clearCropSelection() { rubberBand->hide(); }
void ImageViewer::mousePressEvent(QMouseEvent* event) { QGraphicsView::mousePressEvent(event); }
void ImageViewer::mouseMoveEvent(QMouseEvent* event) { QGraphicsView::mouseMoveEvent(event); }
void ImageViewer::mouseReleaseEvent(QMouseEvent* event) { QGraphicsView::mouseReleaseEvent(event); }
void ImageViewer::wheelEvent(QWheelEvent* event) { QGraphicsView::wheelEvent(event); }
void ImageViewer::updateCropRect() {}

// 主类实现
ImageCompression::ImageCompression() : QWidget(nullptr), DynamicObjectBase(), isProcessing(false)
{
    setupUI();
    
    // 创建工作线程
    workerThread = new QThread(this);
    worker = new ImageProcessWorker();
    worker->moveToThread(workerThread);
    
    connect(selectBtn, &QPushButton::clicked, this, &ImageCompression::onSelectImage);
    connect(saveBtn, &QPushButton::clicked, this, &ImageCompression::onSaveImage);
    connect(clearBtn, &QPushButton::clicked, this, &ImageCompression::onClearAll);
    connect(worker, &ImageProcessWorker::imageProcessed, this, &ImageCompression::onImageProcessed);
    connect(worker, &ImageProcessWorker::progressUpdated, this, &ImageCompression::onProgressUpdated);
    
    workerThread->start();
}

ImageCompression::~ImageCompression()
{
    if (workerThread && workerThread->isRunning()) {
        workerThread->quit();
        workerThread->wait(3000);
    }
}

void ImageCompression::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    
    // 文件选择
    fileGroup = new QGroupBox("📁 图片文件");
    fileLayout = new QVBoxLayout(fileGroup);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    selectBtn = new QPushButton("📂 选择图片");
    saveBtn = new QPushButton("💾 保存");
    clearBtn = new QPushButton("🗑️ 清空");
    saveBtn->setEnabled(false);
    
    btnLayout->addWidget(selectBtn);
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addStretch();
    
    filePathLabel = new QLabel("未选择文件");
    originalSizeLabel = new QLabel("文件大小: 0 KB");
    
    fileLayout->addLayout(btnLayout);
    fileLayout->addWidget(filePathLabel);
    fileLayout->addWidget(originalSizeLabel);
    
    // 图片查看器
    viewerGroup = new QGroupBox("🖼️ 图片预览");
    viewerLayout = new QVBoxLayout(viewerGroup);
    imageViewer = new ImageViewer();
    imageViewer->setMinimumHeight(400);
    viewerLayout->addWidget(imageViewer);
    
    // 控制面板
    controlWidget = new QWidget();
    controlLayout = new QVBoxLayout(controlWidget);
    
    // 压缩设置
    compressGroup = new QGroupBox("🗜️ 压缩设置");
    compressLayout = new QGridLayout(compressGroup);
    
    qualityLabel = new QLabel("质量:");
    qualitySpinBox = new QSpinBox();
    qualitySpinBox->setRange(1, 100);
    qualitySpinBox->setValue(85);
    qualitySpinBox->setSuffix("%");
    
    formatLabel = new QLabel("格式:");
    formatCombo = new QComboBox();
    formatCombo->addItems(QStringList() << "JPEG" << "PNG" << "WEBP");
    
    compressLayout->addWidget(qualityLabel, 0, 0);
    compressLayout->addWidget(qualitySpinBox, 0, 1);
    compressLayout->addWidget(formatLabel, 1, 0);
    compressLayout->addWidget(formatCombo, 1, 1);
    
    // 尺寸设置
    sizeGroup = new QGroupBox("📏 尺寸设置");
    sizeLayout = new QGridLayout(sizeGroup);
    
    widthLabel = new QLabel("宽度:");
    widthSpinBox = new QSpinBox();
    widthSpinBox->setRange(1, 5000);
    widthSpinBox->setSuffix("px");
    
    heightLabel = new QLabel("高度:");
    heightSpinBox = new QSpinBox();
    heightSpinBox->setRange(1, 5000);
    heightSpinBox->setSuffix("px");
    
    sizeLayout->addWidget(widthLabel, 0, 0);
    sizeLayout->addWidget(widthSpinBox, 0, 1);
    sizeLayout->addWidget(heightLabel, 1, 0);
    sizeLayout->addWidget(heightSpinBox, 1, 1);
    
    controlLayout->addWidget(compressGroup);
    controlLayout->addWidget(sizeGroup);
    // 移除多余的stretch，让图片预览区域占用更多空间
    
    // 状态区域
    statusLabel = new QLabel("就绪");
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    
    // 主布局
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->addWidget(fileGroup);
    leftLayout->addWidget(viewerGroup);
    
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(controlWidget);
    
    mainLayout->addWidget(mainSplitter);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statusLabel);
    
    setStyleSheet(R"(
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
        }
    )");
}

void ImageCompression::onSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (!filePath.isEmpty()) {
        loadImage(filePath);
    }
}

void ImageCompression::onSaveImage()
{
    if (currentImagePath.isEmpty()) return;
    
    QString savePath = QFileDialog::getSaveFileName(this, "保存图片", "", "图片文件 (*.png *.jpg *.jpeg)");
    if (savePath.isEmpty()) return;
    
    ImageProcessWorker::ProcessParams params;
    params.inputPath = currentImagePath;
    params.outputPath = savePath;
    params.format = formatCombo->currentText();
    params.quality = qualitySpinBox->value();
    params.width = widthSpinBox->value();
    params.height = heightSpinBox->value();
    
    progressBar->setVisible(true);
    updateStatus("正在处理图片...", false);
    
    QMetaObject::invokeMethod(worker, "processImage", Qt::QueuedConnection, Q_ARG(ImageProcessWorker::ProcessParams, params));
}

void ImageCompression::onClearAll()
{
    currentImagePath.clear();
    imageViewer->setImage(QPixmap());
    filePathLabel->setText("未选择文件");
    originalSizeLabel->setText("文件大小: 0 KB");
    saveBtn->setEnabled(false);
    updateStatus("已清空", false);
}

void ImageCompression::loadImage(const QString& filePath)
{
    QPixmap pixmap(filePath);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "错误", "无法加载图片");
        return;
    }
    
    currentImagePath = filePath;
    imageViewer->setImage(pixmap);
    filePathLabel->setText(QFileInfo(filePath).fileName());
    
    QFileInfo info(filePath);
    originalSizeLabel->setText(QString("文件大小: %1 KB").arg(info.size() / 1024));
    
    widthSpinBox->setValue(pixmap.width());
    heightSpinBox->setValue(pixmap.height());
    
    saveBtn->setEnabled(true);
    updateStatus("图片加载完成", false);
}

void ImageCompression::onImageProcessed(bool success, const QString& message, qint64 originalSize, qint64 newSize)
{
    progressBar->setVisible(false);
    if (success) {
        double ratio = originalSize > 0 ? (1.0 - (double)newSize / originalSize) * 100.0 : 0.0;
        updateStatus(QString("处理完成！压缩了 %1%").arg(ratio, 0, 'f', 1), false);
    } else {
        updateStatus(message, true);
    }
}

void ImageCompression::onProgressUpdated(int percentage)
{
    progressBar->setValue(percentage);
}

void ImageCompression::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? "color: red;" : "color: green;");
}

// 空实现避免编译错误
void ImageCompression::setupFileArea() {}
void ImageCompression::setupImageViewer() {}
void ImageCompression::setupControlArea() {}
void ImageCompression::setupPreviewArea() {}
void ImageCompression::onPreviewChanges() {}
void ImageCompression::onResetSettings() {}
void ImageCompression::onQualityChanged() {}
void ImageCompression::onSizeChanged() {}
void ImageCompression::onFormatChanged() {}
void ImageCompression::onCropToggled() {}
void ImageCompression::onCropRectChanged(const QRect&) {}
void ImageCompression::updateImageInfo() {}
void ImageCompression::updatePreview() {}
QString ImageCompression::formatFileSize(qint64) { return ""; }
void ImageCompression::resetControls() {}

#include "imagecompression.moc"