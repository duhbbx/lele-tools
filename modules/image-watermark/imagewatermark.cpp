#include "imagewatermark.h"
#include <QtMath>
#include <QMouseEvent>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(ImageWatermark);

// WatermarkWorker 实现
WatermarkWorker::WatermarkWorker(QObject* parent)
    : QObject(parent), m_shouldStop(false) {
}

void WatermarkWorker::processImages(const QStringList& inputFiles, const QString& outputDir, const WatermarkConfig& config) {
    QMutexLocker locker(&m_mutex);
    m_inputFiles = inputFiles;
    m_outputDir = outputDir;
    m_config = config;
    m_shouldStop = false;
    locker.unlock();

    QTimer::singleShot(0, this, &WatermarkWorker::doWork);
}

void WatermarkWorker::doWork() {
    QMutexLocker locker(&m_mutex);
    QStringList files = m_inputFiles;
    QString outputDir = m_outputDir;
    WatermarkConfig config = m_config;
    locker.unlock();

    for (int i = 0; i < files.size(); ++i) {
        if (m_shouldStop) {
            break;
        }

        const QString& inputFile = files[i];
        emit progressUpdated(i + 1, files.size(), QFileInfo(inputFile).fileName());

        try {
            // 加载图片
            QPixmap pixmap(inputFile);
            if (pixmap.isNull()) {
                emit imageProcessed(inputFile, "", false, "无法加载图片");
                continue;
            }

            // 添加水印
            QPixmap watermarkedPixmap = addWatermarkToImage(pixmap, config);

            // 保存图片
            QFileInfo fileInfo(inputFile);
            QString outputFile = outputDir + "/" + fileInfo.baseName() + "_watermarked." + fileInfo.suffix();

            // 确保输出目录存在
            QDir().mkpath(outputDir);

            if (watermarkedPixmap.save(outputFile)) {
                emit imageProcessed(inputFile, outputFile, true, "");
            } else {
                emit imageProcessed(inputFile, outputFile, false, "保存失败");
            }

        } catch (const std::exception& e) {
            emit imageProcessed(inputFile, "", false, QString("处理错误: %1").arg(e.what()));
        }

        // 短暂延迟，避免界面卡顿
        QThread::msleep(10);
    }

    emit finished();
}

QPixmap WatermarkWorker::addWatermarkToImage(const QPixmap& image, const WatermarkConfig& config) {
    QPixmap result = image;
    QPainter painter(&result);

    // 设置抗锯齿
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 设置字体和颜色
    painter.setFont(config.font);
    QColor watermarkColor = config.color;
    watermarkColor.setAlpha(255 * config.opacity / 100);
    painter.setPen(watermarkColor);

    // 获取文字尺寸
    QFontMetrics fontMetrics(config.font);
    QRect textRect = fontMetrics.boundingRect(config.text);

    if (config.tiled) {
        // 平铺水印
        int textWidth = textRect.width();
        int textHeight = textRect.height();
        int spacing = config.spacing;

        // 计算平铺的行列数
        int cols = (result.width() + spacing) / (textWidth + spacing) + 1;
        int rows = (result.height() + spacing) / (textHeight + spacing) + 1;

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                int x = col * (textWidth + spacing);
                int y = row * (textHeight + spacing) + textHeight;

                painter.save();

                // 设置旋转
                if (config.rotation != 0) {
                    painter.translate(x + textWidth / 2, y - textHeight / 2);
                    painter.rotate(config.rotation);
                    painter.translate(-textWidth / 2, textHeight / 2);
                    painter.drawText(0, 0, config.text);
                } else {
                    painter.drawText(x, y, config.text);
                }

                painter.restore();
            }
        }
    } else {
        // 单个水印
        QRect imageRect = result.rect();
        QRect targetRect;

        // 根据位置计算目标矩形
        if (config.position & Qt::AlignLeft) {
            targetRect.setLeft(20);
        } else if (config.position & Qt::AlignRight) {
            targetRect.setLeft(imageRect.width() - textRect.width() - 20);
        } else {
            targetRect.setLeft((imageRect.width() - textRect.width()) / 2);
        }

        if (config.position & Qt::AlignTop) {
            targetRect.setTop(textRect.height() + 20);
        } else if (config.position & Qt::AlignBottom) {
            targetRect.setTop(imageRect.height() - 20);
        } else {
            targetRect.setTop((imageRect.height() + textRect.height()) / 2);
        }

        targetRect.setSize(textRect.size());

        painter.save();

        // 设置旋转
        if (config.rotation != 0) {
            painter.translate(targetRect.center());
            painter.rotate(config.rotation);
            painter.translate(-targetRect.width() / 2, textRect.height() / 2);
            painter.drawText(0, 0, config.text);
        } else {
            painter.drawText(targetRect, Qt::AlignCenter, config.text);
        }

        painter.restore();
    }

    return result;
}

// SingleImageWatermark 实现
SingleImageWatermark::SingleImageWatermark(QWidget* parent)
    : QWidget(parent), m_currentIndex(-1) {
    setupUI();
    setAcceptDrops(true);
}

void SingleImageWatermark::setupUI() {
    setStyleSheet(R"(
        QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }
        QPushButton:hover { background-color:#e9ecef; }
        QPushButton:disabled { color:#adb5bd; }
        QLabel { font-size:9pt; color:#495057; }
    )");

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);

    // === 左侧: 文件列表 + 配置 ===
    m_leftWidget = new QWidget();
    m_leftWidget->setFixedWidth(320);
    m_leftLayout = new QVBoxLayout(m_leftWidget);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(4);

    // 文件列表
    m_imageListWidget = new QListWidget();
    m_imageListWidget->setMaximumHeight(120);
    m_imageListWidget->setStyleSheet(
        "QListWidget { border:1px solid #dee2e6; border-radius:4px; background:#f8f9fa; font-size:9pt; }"
        "QListWidget::item { padding:2px 4px; }"
        "QListWidget::item:selected { background:#d0ebff; color:#1971c2; }"
    );

    m_imageButtonLayout = new QHBoxLayout();
    m_selectImagesBtn = new QPushButton("Select Images");
    m_removeSelectedBtn = new QPushButton("Remove");
    m_imageButtonLayout->addWidget(m_selectImagesBtn);
    m_imageButtonLayout->addWidget(m_removeSelectedBtn);

    m_leftLayout->addWidget(m_imageListWidget);
    m_leftLayout->addLayout(m_imageButtonLayout);

    // 水印配置 (flat, no group box)
    m_watermarkLayout = new QFormLayout();
    m_watermarkLayout->setContentsMargins(0, 6, 0, 0);
    m_watermarkLayout->setSpacing(4);

    m_watermarkText = new QLineEdit(m_config.text);
    m_watermarkText->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");

    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(50, 22);
    m_colorBtn->setStyleSheet(QString("background-color:%1; border:1px solid #dee2e6; border-radius:4px;").arg(m_config.color.name()));

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(1, 100);
    m_opacitySlider->setValue(m_config.opacity);
    m_opacityLabel = new QLabel(QString("%1%").arg(m_config.opacity));

    m_spacingSlider = new QSlider(Qt::Horizontal);
    m_spacingSlider->setRange(10, 400);
    m_spacingSlider->setValue(m_config.spacing);
    m_spacingLabel = new QLabel(QString("%1px").arg(m_config.spacing));

    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 72);
    m_fontSizeSpinBox->setValue(m_config.font.pointSize());
    m_fontSizeSpinBox->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");

    m_fontBtn = new QPushButton("Font...");

    m_positionCombo = new QComboBox();
    m_positionCombo->addItem("Center", static_cast<int>(Qt::AlignCenter));
    m_positionCombo->addItem("Top-Left", static_cast<int>(Qt::AlignTop | Qt::AlignLeft));
    m_positionCombo->addItem("Top-Right", static_cast<int>(Qt::AlignTop | Qt::AlignRight));
    m_positionCombo->addItem("Bottom-Left", static_cast<int>(Qt::AlignBottom | Qt::AlignLeft));
    m_positionCombo->addItem("Bottom-Right", static_cast<int>(Qt::AlignBottom | Qt::AlignRight));
    m_positionCombo->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");

    m_rotationSlider = new QSlider(Qt::Horizontal);
    m_rotationSlider->setRange(-90, 90);
    m_rotationSlider->setValue(m_config.rotation);
    m_rotationLabel = new QLabel(QString("%1").arg(m_config.rotation));

    auto addFormRow = [this](const QString& label, QWidget* widget, QWidget* extraWidget = nullptr) {
        if (extraWidget) {
            auto* hLayout = new QHBoxLayout();
            hLayout->addWidget(widget);
            hLayout->addWidget(extraWidget);
            auto* container = new QWidget();
            container->setLayout(hLayout);
            m_watermarkLayout->addRow(label, container);
        } else {
            m_watermarkLayout->addRow(label, widget);
        }
    };

    addFormRow("Text:", m_watermarkText);
    addFormRow("Color:", m_colorBtn);
    addFormRow("Opacity:", m_opacitySlider, m_opacityLabel);
    addFormRow("Spacing:", m_spacingSlider, m_spacingLabel);
    addFormRow("Size:", m_fontSizeSpinBox);
    addFormRow("Font:", m_fontBtn);
    addFormRow("Position:", m_positionCombo);
    addFormRow("Rotation:", m_rotationSlider, m_rotationLabel);

    m_leftLayout->addLayout(m_watermarkLayout);

    // 操作按钮
    m_buttonLayout = new QHBoxLayout();
    m_saveBtn = new QPushButton("Save");
    m_saveAllBtn = new QPushButton("Save All");
    m_copyBtn = new QPushButton("Copy");

    m_saveBtn->setEnabled(false);
    m_saveAllBtn->setEnabled(false);
    m_copyBtn->setEnabled(false);

    m_buttonLayout->addWidget(m_saveBtn);
    m_buttonLayout->addWidget(m_saveAllBtn);
    m_buttonLayout->addWidget(m_copyBtn);

    m_leftLayout->addLayout(m_buttonLayout);
    m_leftLayout->addStretch();

    // === 右侧预览区域 ===
    m_previewWidget = new QWidget();
    m_previewLayout = new QVBoxLayout(m_previewWidget);
    m_previewLayout->setContentsMargins(0, 0, 0, 0);

    m_previewScrollArea = new QScrollArea();
    m_previewScrollArea->setWidgetResizable(true);
    m_previewScrollArea->setStyleSheet(
        "QScrollArea { border:1px solid #dee2e6; border-radius:4px; background:#f8f9fa; }"
    );

    m_previewImageLabel = new QLabel();
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    m_previewImageLabel->setText("Drop or select images");
    m_previewImageLabel->setStyleSheet("QLabel { color:#adb5bd; font-size:9pt; min-height:200px; }");
    m_previewImageLabel->setContextMenuPolicy(Qt::CustomContextMenu);

    m_previewScrollArea->setWidget(m_previewImageLabel);
    m_previewLayout->addWidget(m_previewScrollArea);

    // 组装主布局
    m_mainLayout->addWidget(m_leftWidget);
    m_mainLayout->addWidget(m_previewWidget, 1);

    // 连接信号
    connect(m_selectImagesBtn, &QPushButton::clicked, this, &SingleImageWatermark::onSelectImagesClicked);
    connect(m_removeSelectedBtn, &QPushButton::clicked, this, [this]() {
        auto items = m_imageListWidget->selectedItems();
        for (auto* item : items) {
            int row = m_imageListWidget->row(item);
            m_imagePaths.removeAt(row);
            delete m_imageListWidget->takeItem(row);
        }
        if (m_imagePaths.isEmpty()) {
            resetUI();
        } else {
            if (m_currentIndex >= m_imagePaths.size())
                m_currentIndex = m_imagePaths.size() - 1;
            m_imageListWidget->setCurrentRow(m_currentIndex);
        }
    });
    connect(m_saveBtn, &QPushButton::clicked, this, &SingleImageWatermark::onSaveImageClicked);
    connect(m_saveAllBtn, &QPushButton::clicked, this, &SingleImageWatermark::onSaveAllClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &SingleImageWatermark::onCopyImageClicked);
    connect(m_colorBtn, &QPushButton::clicked, this, &SingleImageWatermark::onColorButtonClicked);
    connect(m_fontBtn, &QPushButton::clicked, this, &SingleImageWatermark::onFontButtonClicked);

    connect(m_watermarkText, &QLineEdit::textChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_spacingSlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SingleImageWatermark::onPositionChanged);
    connect(m_rotationSlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);

    connect(m_imageListWidget, &QListWidget::currentRowChanged, this, &SingleImageWatermark::onImageListSelectionChanged);
    connect(m_previewImageLabel, &QWidget::customContextMenuRequested, this, &SingleImageWatermark::onPreviewContextMenu);
}

void SingleImageWatermark::addImageFiles(const QStringList& files) {
    for (const QString& f : files) {
        if (m_imagePaths.contains(f)) continue;
        QPixmap test(f);
        if (test.isNull()) continue;
        m_imagePaths.append(f);
        m_imageListWidget->addItem(QFileInfo(f).fileName());
    }
    if (!m_imagePaths.isEmpty()) {
        m_saveAllBtn->setEnabled(true);
        if (m_currentIndex < 0) {
            m_imageListWidget->setCurrentRow(0);
        }
    }
}

void SingleImageWatermark::onSelectImagesClicked() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        "Select Images",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff);;All Files (*.*)"
    );
    if (!fileNames.isEmpty()) {
        addImageFiles(fileNames);
    }
}

void SingleImageWatermark::onImageListSelectionChanged() {
    int row = m_imageListWidget->currentRow();
    if (row < 0 || row >= m_imagePaths.size()) {
        m_originalPixmap = QPixmap();
        m_watermarkedPixmap = QPixmap();
        m_saveBtn->setEnabled(false);
        m_copyBtn->setEnabled(false);
        m_previewImageLabel->clear();
        m_previewImageLabel->setText("Drop or select images");
        m_currentIndex = -1;
        return;
    }
    m_currentIndex = row;
    m_originalPixmap = QPixmap(m_imagePaths[row]);
    m_saveBtn->setEnabled(!m_originalPixmap.isNull());
    m_copyBtn->setEnabled(!m_originalPixmap.isNull());
    updatePreview();
}

void SingleImageWatermark::onSaveImageClicked() {
    if (m_watermarkedPixmap.isNull()) return;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Watermarked Image",
        "",
        "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp);;All Files (*.*)"
    );

    if (!fileName.isEmpty()) {
        if (m_watermarkedPixmap.save(fileName)) {
            QMessageBox::information(this, "Done", "Image saved.");
        } else {
            QMessageBox::warning(this, "Error", "Failed to save image.");
        }
    }
}

void SingleImageWatermark::onSaveAllClicked() {
    if (m_imagePaths.isEmpty()) return;

    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
    if (dir.isEmpty()) return;

    int success = 0;
    int fail = 0;
    for (const QString& path : m_imagePaths) {
        QPixmap pix(path);
        if (pix.isNull()) { fail++; continue; }
        QPixmap wm = addWatermark(pix, m_config);
        QFileInfo fi(path);
        QString outPath = dir + "/" + fi.baseName() + "_watermarked." + fi.suffix();
        if (wm.save(outPath)) {
            success++;
        } else {
            fail++;
        }
    }
    QMessageBox::information(this, "Done",
        QString("Saved %1 images. %2 failed.").arg(success).arg(fail));
}

void SingleImageWatermark::onCopyImageClicked() {
    if (m_watermarkedPixmap.isNull()) return;
    QApplication::clipboard()->setPixmap(m_watermarkedPixmap);
}

void SingleImageWatermark::onPreviewContextMenu(const QPoint& pos) {
    if (m_watermarkedPixmap.isNull()) return;
    QMenu menu(this);
    QAction* copyAction = menu.addAction("Copy Image");
    QAction* selected = menu.exec(m_previewImageLabel->mapToGlobal(pos));
    if (selected == copyAction) {
        QApplication::clipboard()->setPixmap(m_watermarkedPixmap);
    }
}

void SingleImageWatermark::onWatermarkConfigChanged() {
    m_config.text = m_watermarkText->text();
    m_config.opacity = m_opacitySlider->value();
    m_config.spacing = m_spacingSlider->value();
    m_config.rotation = m_rotationSlider->value();

    QFont font = m_config.font;
    font.setPointSize(m_fontSizeSpinBox->value());
    m_config.font = font;

    m_opacityLabel->setText(QString("%1%").arg(m_config.opacity));
    m_spacingLabel->setText(QString("%1px").arg(m_config.spacing));
    m_rotationLabel->setText(QString("%1").arg(m_config.rotation));

    updatePreview();
}

void SingleImageWatermark::onColorButtonClicked() {
    QColor color = QColorDialog::getColor(m_config.color, this, "Select Watermark Color");
    if (color.isValid()) {
        m_config.color = color;
        m_colorBtn->setStyleSheet(QString("background-color:%1; border:1px solid #dee2e6; border-radius:4px;").arg(color.name()));
        updatePreview();
    }
}

void SingleImageWatermark::onFontButtonClicked() {
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_config.font, this, "Select Font");
    if (ok) {
        m_config.font = font;
        m_fontSizeSpinBox->setValue(font.pointSize());
        updatePreview();
    }
}

void SingleImageWatermark::onPositionChanged() {
    QVariant data = m_positionCombo->currentData();
    m_config.position = static_cast<Qt::Alignment>(data.toInt());
    updatePreview();
}

void SingleImageWatermark::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void SingleImageWatermark::dropEvent(QDropEvent* event) {
    QStringList files;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
        }
    }
    if (!files.isEmpty()) {
        addImageFiles(files);
    }
}

void SingleImageWatermark::updatePreview() {
    if (m_originalPixmap.isNull()) return;

    m_watermarkedPixmap = addWatermark(m_originalPixmap, m_config);

    QPixmap scaledPixmap = m_watermarkedPixmap.scaled(
        m_previewScrollArea->size() * 0.9,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    m_previewImageLabel->setPixmap(scaledPixmap);
    m_previewImageLabel->setText("");
}

QPixmap SingleImageWatermark::addWatermark(const QPixmap& image, const WatermarkConfig& config) {
    QPixmap result = image;
    QPainter painter(&result);

    // 设置抗锯齿
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 设置字体和颜色
    painter.setFont(config.font);
    QColor watermarkColor = config.color;
    watermarkColor.setAlpha(255 * config.opacity / 100);
    painter.setPen(watermarkColor);

    // 获取文字尺寸
    QFontMetrics fontMetrics(config.font);
    QRect textRect = fontMetrics.boundingRect(config.text);

    if (config.tiled) {
        // 平铺水印
        int textWidth = textRect.width();
        int textHeight = textRect.height();
        int spacing = config.spacing;

        // 计算平铺的行列数
        int cols = (result.width() + spacing) / (textWidth + spacing) + 1;
        int rows = (result.height() + spacing) / (textHeight + spacing) + 1;

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                int x = col * (textWidth + spacing);
                int y = row * (textHeight + spacing) + textHeight;

                painter.save();

                // 设置旋转
                if (config.rotation != 0) {
                    painter.translate(x + textWidth / 2, y - textHeight / 2);
                    painter.rotate(config.rotation);
                    painter.translate(-textWidth / 2, textHeight / 2);
                    painter.drawText(0, 0, config.text);
                } else {
                    painter.drawText(x, y, config.text);
                }

                painter.restore();
            }
        }
    } else {
        // 单个水印
        QRect imageRect = result.rect();
        QRect targetRect;

        // 根据位置计算目标矩形
        if (config.position & Qt::AlignLeft) {
            targetRect.setLeft(20);
        } else if (config.position & Qt::AlignRight) {
            targetRect.setLeft(imageRect.width() - textRect.width() - 20);
        } else {
            targetRect.setLeft((imageRect.width() - textRect.width()) / 2);
        }

        if (config.position & Qt::AlignTop) {
            targetRect.setTop(textRect.height() + 20);
        } else if (config.position & Qt::AlignBottom) {
            targetRect.setTop(imageRect.height() - 20);
        } else {
            targetRect.setTop((imageRect.height() + textRect.height()) / 2);
        }

        targetRect.setSize(textRect.size());

        painter.save();

        // 设置旋转
        if (config.rotation != 0) {
            painter.translate(targetRect.center());
            painter.rotate(config.rotation);
            painter.translate(-targetRect.width() / 2, textRect.height() / 2);
            painter.drawText(0, 0, config.text);
        } else {
            painter.drawText(targetRect, Qt::AlignCenter, config.text);
        }

        painter.restore();
    }

    return result;
}

void SingleImageWatermark::resetUI() {
    m_imagePaths.clear();
    m_imageListWidget->clear();
    m_currentIndex = -1;
    m_originalPixmap = QPixmap();
    m_watermarkedPixmap = QPixmap();

    m_previewImageLabel->clear();
    m_previewImageLabel->setText("Drop or select images");

    m_saveBtn->setEnabled(false);
    m_saveAllBtn->setEnabled(false);
    m_copyBtn->setEnabled(false);
}

// BatchImageWatermark 实现
BatchImageWatermark::BatchImageWatermark(QWidget* parent)
    : QWidget(parent), m_workerThread(nullptr), m_worker(nullptr), m_isProcessing(false) {
    setupUI();
}

BatchImageWatermark::~BatchImageWatermark() {
    if (m_isProcessing) {
        stopProcessing();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void BatchImageWatermark::setupUI() {
    setStyleSheet(R"(
        QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }
        QPushButton:hover { background-color:#e9ecef; }
        QPushButton:disabled { color:#adb5bd; }
        QLabel { font-size:9pt; color:#495057; }
    )");

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
    m_mainLayout->setSpacing(6);

    // === 左侧配置区域 ===
    m_configWidget = new QWidget();
    m_configWidget->setFixedWidth(350);
    m_configLayout = new QVBoxLayout(m_configWidget);
    m_configLayout->setContentsMargins(0, 0, 0, 0);
    m_configLayout->setSpacing(4);

    // 文件列表
    m_filesList = new QListWidget();
    m_filesList->setDragDropMode(QAbstractItemView::DropOnly);
    m_filesList->setAcceptDrops(true);
    m_filesList->setMaximumHeight(140);
    m_filesList->setStyleSheet(
        "QListWidget { border:1px solid #dee2e6; border-radius:4px; background:#f8f9fa; font-size:9pt; }"
        "QListWidget::item { padding:2px 4px; }"
        "QListWidget::item:selected { background:#d0ebff; color:#1971c2; }"
    );

    m_filesButtonLayout = new QHBoxLayout();
    m_addImagesBtn = new QPushButton("Add Images");
    m_removeSelectedBtn = new QPushButton("Remove");
    m_clearAllBtn = new QPushButton("Clear All");

    m_filesButtonLayout->addWidget(m_addImagesBtn);
    m_filesButtonLayout->addWidget(m_removeSelectedBtn);
    m_filesButtonLayout->addWidget(m_clearAllBtn);

    m_configLayout->addWidget(m_filesList);
    m_configLayout->addLayout(m_filesButtonLayout);

    // 输出目录
    m_outputDirLayout = new QHBoxLayout();
    m_outputDirEdit = new QLineEdit();
    m_outputDirEdit->setPlaceholderText("Output directory...");
    m_outputDirEdit->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");
    m_selectOutputDirBtn = new QPushButton("Browse");

    m_outputDirLayout->addWidget(m_outputDirEdit);
    m_outputDirLayout->addWidget(m_selectOutputDirBtn);
    m_configLayout->addLayout(m_outputDirLayout);

    // 水印配置 (flat)
    m_watermarkLayout = new QFormLayout();
    m_watermarkLayout->setContentsMargins(0, 6, 0, 0);
    m_watermarkLayout->setSpacing(4);

    m_watermarkText = new QLineEdit(m_config.text);
    m_watermarkText->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");

    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(50, 22);
    m_colorBtn->setStyleSheet(QString("background-color:%1; border:1px solid #dee2e6; border-radius:4px;").arg(m_config.color.name()));

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(1, 100);
    m_opacitySlider->setValue(m_config.opacity);
    m_opacityLabel = new QLabel(QString("%1%").arg(m_config.opacity));

    m_spacingSlider = new QSlider(Qt::Horizontal);
    m_spacingSlider->setRange(10, 400);
    m_spacingSlider->setValue(m_config.spacing);
    m_spacingLabel = new QLabel(QString("%1px").arg(m_config.spacing));

    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 72);
    m_fontSizeSpinBox->setValue(m_config.font.pointSize());
    m_fontSizeSpinBox->setStyleSheet("padding:4px; border:1px solid #dee2e6; border-radius:4px; font-size:9pt;");

    m_fontBtn = new QPushButton("Font...");

    m_rotationSlider = new QSlider(Qt::Horizontal);
    m_rotationSlider->setRange(-90, 90);
    m_rotationSlider->setValue(m_config.rotation);
    m_rotationLabel = new QLabel(QString("%1").arg(m_config.rotation));

    auto addFormRow = [this](const QString& label, QWidget* widget, QWidget* extraWidget = nullptr) {
        if (extraWidget) {
            auto* hLayout = new QHBoxLayout();
            hLayout->addWidget(widget);
            hLayout->addWidget(extraWidget);
            auto* container = new QWidget();
            container->setLayout(hLayout);
            m_watermarkLayout->addRow(label, container);
        } else {
            m_watermarkLayout->addRow(label, widget);
        }
    };

    addFormRow("Text:", m_watermarkText);
    addFormRow("Color:", m_colorBtn);
    addFormRow("Opacity:", m_opacitySlider, m_opacityLabel);
    addFormRow("Spacing:", m_spacingSlider, m_spacingLabel);
    addFormRow("Size:", m_fontSizeSpinBox);
    addFormRow("Font:", m_fontBtn);
    addFormRow("Rotation:", m_rotationSlider, m_rotationLabel);

    m_configLayout->addLayout(m_watermarkLayout);
    m_configLayout->addStretch();

    // === 右侧状态区域 ===
    m_statusWidget = new QWidget();
    m_statusLayout = new QVBoxLayout(m_statusWidget);
    m_statusLayout->setContentsMargins(0, 0, 0, 0);
    m_statusLayout->setSpacing(4);

    // 控制按钮
    m_controlButtonLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("Start");
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setEnabled(false);

    m_controlButtonLayout->addWidget(m_startBtn);
    m_controlButtonLayout->addWidget(m_stopBtn);
    m_statusLayout->addLayout(m_controlButtonLayout);

    // 进度
    m_progressBar = new QProgressBar();
    m_progressBar->setStyleSheet(
        "QProgressBar { border:1px solid #dee2e6; border-radius:4px; text-align:center; font-size:9pt; }"
        "QProgressBar::chunk { background-color:#339af0; border-radius:3px; }"
    );

    m_statusLabel = new QLabel("Ready");
    m_currentFileLabel = new QLabel("");

    m_statusLayout->addWidget(m_progressBar);
    m_statusLayout->addWidget(m_statusLabel);
    m_statusLayout->addWidget(m_currentFileLabel);

    // 结果
    m_resultsText = new QTextEdit();
    m_resultsText->setReadOnly(true);
    m_resultsText->setStyleSheet(
        "QTextEdit { border:1px solid #dee2e6; border-radius:4px; background:#f8f9fa; font-family:Consolas,Monaco,monospace; font-size:9pt; }"
    );

    m_statusLayout->addWidget(m_resultsText, 1);

    // 组装主布局
    m_mainLayout->addWidget(m_configWidget);
    m_mainLayout->addWidget(m_statusWidget, 1);

    // 连接信号
    connect(m_addImagesBtn, &QPushButton::clicked, this, &BatchImageWatermark::onAddImagesClicked);
    connect(m_removeSelectedBtn, &QPushButton::clicked, this, &BatchImageWatermark::onRemoveSelectedClicked);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &BatchImageWatermark::onClearAllClicked);
    connect(m_selectOutputDirBtn, &QPushButton::clicked, this, &BatchImageWatermark::onSelectOutputDirClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &BatchImageWatermark::onStartProcessingClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BatchImageWatermark::onStopProcessingClicked);
    connect(m_colorBtn, &QPushButton::clicked, this, &BatchImageWatermark::onColorButtonClicked);
    connect(m_fontBtn, &QPushButton::clicked, this, &BatchImageWatermark::onFontButtonClicked);

    connect(m_watermarkText, &QLineEdit::textChanged, this, &BatchImageWatermark::onWatermarkConfigChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &BatchImageWatermark::onWatermarkConfigChanged);
    connect(m_spacingSlider, &QSlider::valueChanged, this, &BatchImageWatermark::onWatermarkConfigChanged);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BatchImageWatermark::onWatermarkConfigChanged);
    connect(m_rotationSlider, &QSlider::valueChanged, this, &BatchImageWatermark::onWatermarkConfigChanged);

    // 设置默认输出目录
    QString defaultOutputDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/watermarked_images";
    m_outputDirEdit->setText(defaultOutputDir);

    updateUI();
}

void BatchImageWatermark::onAddImagesClicked() {
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        "Select Images",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff);;All Files (*.*)"
    );

    for (const QString& fileName : fileNames) {
        // 检查是否已存在
        bool exists = false;
        for (int i = 0; i < m_filesList->count(); ++i) {
            if (m_filesList->item(i)->data(Qt::UserRole).toString() == fileName) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            QFileInfo fileInfo(fileName);
            auto* item = new QListWidgetItem(fileInfo.fileName());
            item->setData(Qt::UserRole, fileName);
            item->setToolTip(fileName);
            m_filesList->addItem(item);
        }
    }

    updateUI();
}

void BatchImageWatermark::onRemoveSelectedClicked() {
    QList<QListWidgetItem*> selectedItems = m_filesList->selectedItems();
    for (QListWidgetItem* item : selectedItems) {
        delete m_filesList->takeItem(m_filesList->row(item));
    }
    updateUI();
}

void BatchImageWatermark::onClearAllClicked() {
    m_filesList->clear();
    m_resultsText->clear();
    updateUI();
}

void BatchImageWatermark::onSelectOutputDirClicked() {
    QString dirName = QFileDialog::getExistingDirectory(
        this,
        "Select Output Directory",
        m_outputDirEdit->text()
    );

    if (!dirName.isEmpty()) {
        m_outputDirEdit->setText(dirName);
    }
}

void BatchImageWatermark::onStartProcessingClicked() {
    if (m_filesList->count() == 0) {
        QMessageBox::warning(this, "Notice", "Please add images first.");
        return;
    }

    QString outputDir = m_outputDirEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, "Notice", "Please select an output directory.");
        return;
    }

    startProcessing();
}

void BatchImageWatermark::onStopProcessingClicked() {
    stopProcessing();
}

void BatchImageWatermark::onWatermarkConfigChanged() {
    m_config.text = m_watermarkText->text();
    m_config.opacity = m_opacitySlider->value();
    m_config.spacing = m_spacingSlider->value();
    m_config.rotation = m_rotationSlider->value();

    QFont font = m_config.font;
    font.setPointSize(m_fontSizeSpinBox->value());
    m_config.font = font;

    m_opacityLabel->setText(QString("%1%").arg(m_config.opacity));
    m_spacingLabel->setText(QString("%1px").arg(m_config.spacing));
    m_rotationLabel->setText(QString("%1").arg(m_config.rotation));
}

void BatchImageWatermark::onColorButtonClicked() {
    QColor color = QColorDialog::getColor(m_config.color, this, "Select Watermark Color");
    if (color.isValid()) {
        m_config.color = color;
        m_colorBtn->setStyleSheet(QString("background-color:%1; border:1px solid #dee2e6; border-radius:4px;").arg(color.name()));
    }
}

void BatchImageWatermark::onFontButtonClicked() {
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_config.font, this, "Select Font");
    if (ok) {
        m_config.font = font;
        m_fontSizeSpinBox->setValue(font.pointSize());
    }
}

void BatchImageWatermark::onProgressUpdated(int current, int total, const QString& currentFile) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Processing %1/%2").arg(current).arg(total));
    m_currentFileLabel->setText(currentFile);
}

void BatchImageWatermark::onImageProcessed(const QString& inputFile, const QString& outputFile, bool success, const QString& error) {
    QString message;
    if (success) {
        message = QString("%1 -> %2").arg(QFileInfo(inputFile).fileName(), QFileInfo(outputFile).fileName());
    } else {
        message = QString("FAIL %1: %2").arg(QFileInfo(inputFile).fileName(), error);
    }

    m_resultsText->append(message);
    m_resultsText->ensureCursorVisible();
}

void BatchImageWatermark::onProcessingFinished() {
    m_isProcessing = false;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("Done");
    m_currentFileLabel->setText("");

    QMessageBox::information(this, "Done", "Batch watermark processing complete.");
}

void BatchImageWatermark::updateUI() {
    bool hasFiles = m_filesList->count() > 0;
    bool hasOutputDir = !m_outputDirEdit->text().trimmed().isEmpty();

    m_startBtn->setEnabled(hasFiles && hasOutputDir && !m_isProcessing);
    m_removeSelectedBtn->setEnabled(hasFiles && !m_isProcessing);
    m_clearAllBtn->setEnabled(hasFiles && !m_isProcessing);
}

void BatchImageWatermark::startProcessing() {
    // 收集输入文件
    QStringList inputFiles;
    for (int i = 0; i < m_filesList->count(); ++i) {
        inputFiles.append(m_filesList->item(i)->data(Qt::UserRole).toString());
    }

    QString outputDir = m_outputDirEdit->text().trimmed();

    // 创建工作线程
    m_workerThread = new QThread(this);
    m_worker = new WatermarkWorker();
    m_worker->moveToThread(m_workerThread);

    // 连接信号
    connect(m_worker, &WatermarkWorker::progressUpdated, this, &BatchImageWatermark::onProgressUpdated);
    connect(m_worker, &WatermarkWorker::imageProcessed, this, &BatchImageWatermark::onImageProcessed);
    connect(m_worker, &WatermarkWorker::finished, this, &BatchImageWatermark::onProcessingFinished);

    // 启动处理
    m_isProcessing = true;
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_resultsText->clear();

    m_workerThread->start();
    m_worker->processImages(inputFiles, outputDir, m_config);
}

void BatchImageWatermark::stopProcessing() {
    if (m_worker) {
        m_isProcessing = false;
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_statusLabel->setText("Stopped");
    }
}

// ImageWatermark 主类实现
ImageWatermark::ImageWatermark(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void ImageWatermark::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border:1px solid #dee2e6; background-color:white; }"
        "QTabBar::tab { background:#f8f9fa; color:#495057; padding:6px 14px; margin-right:2px; border-top-left-radius:4px; border-top-right-radius:4px; font-size:9pt; }"
        "QTabBar::tab:selected { background:white; border-bottom:2px solid #339af0; }"
        "QTabBar::tab:hover { background:#e9ecef; }"
    );

    m_singleWatermark = new SingleImageWatermark();
    m_tabWidget->addTab(m_singleWatermark, "Single Image");

    m_batchWatermark = new BatchImageWatermark();
    m_tabWidget->addTab(m_batchWatermark, "Batch Process");

    m_mainLayout->addWidget(m_tabWidget);
}
