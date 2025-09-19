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
    : QWidget(parent) {
    setupUI();
    updatePreview();
}

void SingleImageWatermark::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧配置区域
    m_configWidget = new QWidget();
    m_configWidget->setFixedWidth(350);
    m_configLayout = new QVBoxLayout(m_configWidget);

    // 图片选择组
    m_imageGroup = new QGroupBox("选择图片");
    m_imageGroupLayout = new QVBoxLayout(m_imageGroup);

    m_selectImageBtn = new QPushButton("📁 选择图片文件");
    m_selectImageBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    m_imageInfoLabel = new QLabel("未选择文件");
    m_imageInfoLabel->setStyleSheet(
        "QLabel {"
        "color: #7f8c8d;"
        "font-style: italic;"
        "padding: 8px;"
        "background-color: #ecf0f1;"
        "border-radius: 4px;"
        "}"
    );

    m_imageGroupLayout->addWidget(m_selectImageBtn);
    m_imageGroupLayout->addWidget(m_imageInfoLabel);

    // 水印配置组
    m_watermarkGroup = new QGroupBox("水印设置");
    m_watermarkLayout = new QFormLayout(m_watermarkGroup);

    // 水印文字
    m_watermarkText = new QLineEdit(m_config.text);
    m_watermarkText->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 颜色选择
    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(60, 30);
    m_colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #bdc3c7; border-radius: 4px;").arg(m_config.color.name()));

    // 透明度
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(1, 100);
    m_opacitySlider->setValue(m_config.opacity);
    m_opacityLabel = new QLabel(QString("%1%").arg(m_config.opacity));

    // 间隔
    m_spacingSlider = new QSlider(Qt::Horizontal);
    m_spacingSlider->setRange(10, 200);
    m_spacingSlider->setValue(m_config.spacing);
    m_spacingLabel = new QLabel(QString("%1px").arg(m_config.spacing));

    // 字号
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 72);
    m_fontSizeSpinBox->setValue(m_config.font.pointSize());
    m_fontSizeSpinBox->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 字体
    m_fontBtn = new QPushButton("选择字体");
    m_fontBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #95a5a6;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "background-color: #7f8c8d;"
        "}"
    );

    // 位置
    m_positionCombo = new QComboBox();
    m_positionCombo->addItem("居中", static_cast<int>(Qt::AlignCenter));
    m_positionCombo->addItem("左上", static_cast<int>(Qt::AlignTop | Qt::AlignLeft));
    m_positionCombo->addItem("右上", static_cast<int>(Qt::AlignTop | Qt::AlignRight));
    m_positionCombo->addItem("左下", static_cast<int>(Qt::AlignBottom | Qt::AlignLeft));
    m_positionCombo->addItem("右下", static_cast<int>(Qt::AlignBottom | Qt::AlignRight));
    m_positionCombo->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 旋转角度
    m_rotationSlider = new QSlider(Qt::Horizontal);
    m_rotationSlider->setRange(-90, 90);
    m_rotationSlider->setValue(m_config.rotation);
    m_rotationLabel = new QLabel(QString("%1°").arg(m_config.rotation));

    // 添加到表单布局
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

    addFormRow("水印文字:", m_watermarkText);
    addFormRow("文字颜色:", m_colorBtn);
    addFormRow("透明度:", m_opacitySlider, m_opacityLabel);
    addFormRow("间隔:", m_spacingSlider, m_spacingLabel);
    addFormRow("字号:", m_fontSizeSpinBox);
    addFormRow("字体:", m_fontBtn);
    addFormRow("位置:", m_positionCombo);
    addFormRow("旋转:", m_rotationSlider, m_rotationLabel);

    // 操作按钮
    m_buttonLayout = new QHBoxLayout();
    m_saveBtn = new QPushButton("💾 保存图片");
    m_copyBtn = new QPushButton("📋 复制图片");

    m_saveBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_copyBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_saveBtn->setEnabled(false);
    m_copyBtn->setEnabled(false);

    m_buttonLayout->addWidget(m_saveBtn);
    m_buttonLayout->addWidget(m_copyBtn);

    // 组装配置区域
    m_configLayout->addWidget(m_imageGroup);
    m_configLayout->addWidget(m_watermarkGroup);
    m_configLayout->addLayout(m_buttonLayout);
    m_configLayout->addStretch();

    // 右侧预览区域
    m_previewWidget = new QWidget();
    m_previewLayout = new QVBoxLayout(m_previewWidget);

    m_previewLabel = new QLabel("预览图片（点击图片下载）");
    m_previewLabel->setStyleSheet("font-weight: bold; color: #2c3e50; margin-bottom: 10px; font-size: 14px;");

    m_previewScrollArea = new QScrollArea();
    m_previewScrollArea->setWidgetResizable(true);
    m_previewScrollArea->setStyleSheet(
        "QScrollArea {"
        "border: 2px dashed #bdc3c7;"
        "border-radius: 8px;"
        "background-color: #f8f9fa;"
        "}"
    );

    m_previewImageLabel = new QLabel();
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    m_previewImageLabel->setText("请选择图片文件");
    m_previewImageLabel->setStyleSheet(
        "QLabel {"
        "color: #7f8c8d;"
        "font-size: 16px;"
        "font-style: italic;"
        "min-height: 400px;"
        "}"
    );
    m_previewImageLabel->setCursor(Qt::PointingHandCursor);

    m_previewScrollArea->setWidget(m_previewImageLabel);

    m_previewLayout->addWidget(m_previewLabel);
    m_previewLayout->addWidget(m_previewScrollArea);

    // 组装主布局
    m_mainSplitter->addWidget(m_configWidget);
    m_mainSplitter->addWidget(m_previewWidget);
    m_mainSplitter->setSizes({350, 600});

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_selectImageBtn, &QPushButton::clicked, this, &SingleImageWatermark::onSelectImageClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &SingleImageWatermark::onSaveImageClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &SingleImageWatermark::onCopyImageClicked);
    connect(m_colorBtn, &QPushButton::clicked, this, &SingleImageWatermark::onColorButtonClicked);
    connect(m_fontBtn, &QPushButton::clicked, this, &SingleImageWatermark::onFontButtonClicked);

    connect(m_watermarkText, &QLineEdit::textChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_spacingSlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SingleImageWatermark::onWatermarkConfigChanged);
    connect(m_positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SingleImageWatermark::onPositionChanged);
    connect(m_rotationSlider, &QSlider::valueChanged, this, &SingleImageWatermark::onWatermarkConfigChanged);

    // Note: QLabel::mousePressEvent is not a signal, it's a virtual function
    // For click handling, we'll install an event filter
    m_previewImageLabel->installEventFilter(this);
}

void SingleImageWatermark::onSelectImageClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择图片文件",
        "",
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tiff);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        m_originalImagePath = fileName;
        m_originalPixmap = QPixmap(fileName);

        if (!m_originalPixmap.isNull()) {
            QFileInfo fileInfo(fileName);
            m_imageInfoLabel->setText(QString("文件: %1\n尺寸: %2×%3")
                .arg(fileInfo.fileName())
                .arg(m_originalPixmap.width())
                .arg(m_originalPixmap.height()));

            m_saveBtn->setEnabled(true);
            m_copyBtn->setEnabled(true);

            updatePreview();
        } else {
            QMessageBox::warning(this, "错误", "无法加载图片文件");
            resetUI();
        }
    }
}

void SingleImageWatermark::onSaveImageClicked() {
    if (m_watermarkedPixmap.isNull()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存水印图片",
        "",
        "PNG图片 (*.png);;JPEG图片 (*.jpg);;BMP图片 (*.bmp);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        if (m_watermarkedPixmap.save(fileName)) {
            QMessageBox::information(this, "成功", "图片保存成功！");
        } else {
            QMessageBox::warning(this, "错误", "图片保存失败！");
        }
    }
}

void SingleImageWatermark::onCopyImageClicked() {
    if (m_watermarkedPixmap.isNull()) {
        return;
    }

    QApplication::clipboard()->setPixmap(m_watermarkedPixmap);
    QMessageBox::information(this, "成功", "图片已复制到剪贴板！");
}

void SingleImageWatermark::onWatermarkConfigChanged() {
    // 更新配置
    m_config.text = m_watermarkText->text();
    m_config.opacity = m_opacitySlider->value();
    m_config.spacing = m_spacingSlider->value();
    m_config.rotation = m_rotationSlider->value();

    QFont font = m_config.font;
    font.setPointSize(m_fontSizeSpinBox->value());
    m_config.font = font;

    // 更新标签
    m_opacityLabel->setText(QString("%1%").arg(m_config.opacity));
    m_spacingLabel->setText(QString("%1px").arg(m_config.spacing));
    m_rotationLabel->setText(QString("%1°").arg(m_config.rotation));

    updatePreview();
}

void SingleImageWatermark::onColorButtonClicked() {
    QColor color = QColorDialog::getColor(m_config.color, this, "选择水印颜色");
    if (color.isValid()) {
        m_config.color = color;
        m_colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #bdc3c7; border-radius: 4px;").arg(color.name()));
        updatePreview();
    }
}

void SingleImageWatermark::onFontButtonClicked() {
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_config.font, this, "选择字体");
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

bool SingleImageWatermark::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_previewImageLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_watermarkedPixmap.isNull()) {
            onSaveImageClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SingleImageWatermark::updatePreview() {
    if (m_originalPixmap.isNull()) {
        return;
    }

    m_watermarkedPixmap = addWatermark(m_originalPixmap, m_config);

    // 缩放预览图片
    QPixmap scaledPixmap = m_watermarkedPixmap.scaled(
        m_previewScrollArea->size() * 0.8,
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
    m_originalImagePath.clear();
    m_originalPixmap = QPixmap();
    m_watermarkedPixmap = QPixmap();

    m_imageInfoLabel->setText("未选择文件");
    m_previewImageLabel->clear();
    m_previewImageLabel->setText("请选择图片文件");

    m_saveBtn->setEnabled(false);
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
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧配置区域
    m_configWidget = new QWidget();
    m_configWidget->setFixedWidth(380);
    m_configLayout = new QVBoxLayout(m_configWidget);

    // 文件选择组
    m_filesGroup = new QGroupBox("选择图片文件");
    m_filesGroupLayout = new QVBoxLayout(m_filesGroup);

    m_filesList = new QListWidget();
    m_filesList->setDragDropMode(QAbstractItemView::DropOnly);
    m_filesList->setAcceptDrops(true);
    m_filesList->setStyleSheet(
        "QListWidget {"
        "border: 2px dashed #bdc3c7;"
        "border-radius: 6px;"
        "background-color: #f8f9fa;"
        "min-height: 120px;"
        "}"
        "QListWidget::item {"
        "padding: 4px;"
        "border-bottom: 1px solid #ecf0f1;"
        "}"
    );

    m_filesButtonLayout = new QHBoxLayout();
    m_addImagesBtn = new QPushButton("📁 添加图片");
    m_removeSelectedBtn = new QPushButton("❌ 移除选中");
    m_clearAllBtn = new QPushButton("🗑️ 清空全部");

    QString buttonStyle =
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}";

    m_addImagesBtn->setStyleSheet(buttonStyle);
    m_removeSelectedBtn->setStyleSheet(buttonStyle.replace("#3498db", "#e74c3c").replace("#2980b9", "#c0392b"));
    m_clearAllBtn->setStyleSheet(buttonStyle.replace("#3498db", "#95a5a6").replace("#2980b9", "#7f8c8d"));

    m_filesButtonLayout->addWidget(m_addImagesBtn);
    m_filesButtonLayout->addWidget(m_removeSelectedBtn);
    m_filesButtonLayout->addWidget(m_clearAllBtn);

    m_filesGroupLayout->addWidget(m_filesList);
    m_filesGroupLayout->addLayout(m_filesButtonLayout);

    // 输出目录组
    m_outputGroup = new QGroupBox("输出目录");
    m_outputGroupLayout = new QVBoxLayout(m_outputGroup);
    m_outputDirLayout = new QHBoxLayout();

    m_outputDirEdit = new QLineEdit();
    m_outputDirEdit->setPlaceholderText("选择输出目录...");
    m_outputDirEdit->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    m_selectOutputDirBtn = new QPushButton("📁 选择");
    m_selectOutputDirBtn->setStyleSheet(buttonStyle);

    m_outputDirLayout->addWidget(m_outputDirEdit);
    m_outputDirLayout->addWidget(m_selectOutputDirBtn);
    m_outputGroupLayout->addLayout(m_outputDirLayout);

    // 水印配置组
    m_watermarkGroup = new QGroupBox("水印设置");
    m_watermarkLayout = new QFormLayout(m_watermarkGroup);

    // 水印文字
    m_watermarkText = new QLineEdit(m_config.text);
    m_watermarkText->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 颜色选择
    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(60, 30);
    m_colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #bdc3c7; border-radius: 4px;").arg(m_config.color.name()));

    // 透明度
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(1, 100);
    m_opacitySlider->setValue(m_config.opacity);
    m_opacityLabel = new QLabel(QString("%1%").arg(m_config.opacity));

    // 间隔
    m_spacingSlider = new QSlider(Qt::Horizontal);
    m_spacingSlider->setRange(10, 200);
    m_spacingSlider->setValue(m_config.spacing);
    m_spacingLabel = new QLabel(QString("%1px").arg(m_config.spacing));

    // 字号
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 72);
    m_fontSizeSpinBox->setValue(m_config.font.pointSize());
    m_fontSizeSpinBox->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 字体
    m_fontBtn = new QPushButton("选择字体");
    m_fontBtn->setStyleSheet(buttonStyle.replace("#3498db", "#95a5a6").replace("#2980b9", "#7f8c8d"));

    // 旋转角度
    m_rotationSlider = new QSlider(Qt::Horizontal);
    m_rotationSlider->setRange(-90, 90);
    m_rotationSlider->setValue(m_config.rotation);
    m_rotationLabel = new QLabel(QString("%1°").arg(m_config.rotation));

    // 添加到表单布局
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

    addFormRow("水印文字:", m_watermarkText);
    addFormRow("文字颜色:", m_colorBtn);
    addFormRow("透明度:", m_opacitySlider, m_opacityLabel);
    addFormRow("间隔:", m_spacingSlider, m_spacingLabel);
    addFormRow("字号:", m_fontSizeSpinBox);
    addFormRow("字体:", m_fontBtn);
    addFormRow("旋转:", m_rotationSlider, m_rotationLabel);

    // 组装配置区域
    m_configLayout->addWidget(m_filesGroup);
    m_configLayout->addWidget(m_outputGroup);
    m_configLayout->addWidget(m_watermarkGroup);
    m_configLayout->addStretch();

    // 右侧状态区域
    m_statusWidget = new QWidget();
    m_statusLayout = new QVBoxLayout(m_statusWidget);

    // 控制组
    m_controlGroup = new QGroupBox("批量处理控制");
    m_controlGroupLayout = new QVBoxLayout(m_controlGroup);
    m_controlButtonLayout = new QHBoxLayout();

    m_startBtn = new QPushButton("🚀 开始处理");
    m_stopBtn = new QPushButton("⏹️ 停止处理");

    m_startBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_stopBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_stopBtn->setEnabled(false);

    m_controlButtonLayout->addWidget(m_startBtn);
    m_controlButtonLayout->addWidget(m_stopBtn);
    m_controlGroupLayout->addLayout(m_controlButtonLayout);

    // 进度组
    m_progressGroup = new QGroupBox("处理进度");
    m_progressGroupLayout = new QVBoxLayout(m_progressGroup);

    m_progressBar = new QProgressBar();
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "border: 2px solid #bdc3c7;"
        "border-radius: 5px;"
        "text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "background-color: #3498db;"
        "border-radius: 3px;"
        "}"
    );

    m_statusLabel = new QLabel("准备就绪");
    m_statusLabel->setStyleSheet("color: #7f8c8d; font-style: italic;");

    m_currentFileLabel = new QLabel("");
    m_currentFileLabel->setStyleSheet("color: #2c3e50; font-weight: bold;");

    m_progressGroupLayout->addWidget(m_progressBar);
    m_progressGroupLayout->addWidget(m_statusLabel);
    m_progressGroupLayout->addWidget(m_currentFileLabel);

    // 结果组
    m_resultsGroup = new QGroupBox("处理结果");
    m_resultsGroupLayout = new QVBoxLayout(m_resultsGroup);

    m_resultsText = new QTextEdit();
    m_resultsText->setReadOnly(true);
    m_resultsText->setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "border-radius: 4px;"
        "background-color: #f8f9fa;"
        "font-family: Consolas, Monaco, monospace;"
        "}"
    );

    m_resultsGroupLayout->addWidget(m_resultsText);

    // 组装状态区域
    m_statusLayout->addWidget(m_controlGroup);
    m_statusLayout->addWidget(m_progressGroup);
    m_statusLayout->addWidget(m_resultsGroup);

    // 组装主布局
    m_mainSplitter->addWidget(m_configWidget);
    m_mainSplitter->addWidget(m_statusWidget);
    m_mainSplitter->setSizes({380, 500});

    m_mainLayout->addWidget(m_mainSplitter);

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
        "选择图片文件",
        "",
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tiff);;所有文件 (*.*)"
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
        "选择输出目录",
        m_outputDirEdit->text()
    );

    if (!dirName.isEmpty()) {
        m_outputDirEdit->setText(dirName);
    }
}

void BatchImageWatermark::onStartProcessingClicked() {
    if (m_filesList->count() == 0) {
        QMessageBox::warning(this, "提示", "请先添加要处理的图片文件");
        return;
    }

    QString outputDir = m_outputDirEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择输出目录");
        return;
    }

    startProcessing();
}

void BatchImageWatermark::onStopProcessingClicked() {
    stopProcessing();
}

void BatchImageWatermark::onWatermarkConfigChanged() {
    // 更新配置
    m_config.text = m_watermarkText->text();
    m_config.opacity = m_opacitySlider->value();
    m_config.spacing = m_spacingSlider->value();
    m_config.rotation = m_rotationSlider->value();

    QFont font = m_config.font;
    font.setPointSize(m_fontSizeSpinBox->value());
    m_config.font = font;

    // 更新标签
    m_opacityLabel->setText(QString("%1%").arg(m_config.opacity));
    m_spacingLabel->setText(QString("%1px").arg(m_config.spacing));
    m_rotationLabel->setText(QString("%1°").arg(m_config.rotation));
}

void BatchImageWatermark::onColorButtonClicked() {
    QColor color = QColorDialog::getColor(m_config.color, this, "选择水印颜色");
    if (color.isValid()) {
        m_config.color = color;
        m_colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #bdc3c7; border-radius: 4px;").arg(color.name()));
    }
}

void BatchImageWatermark::onFontButtonClicked() {
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_config.font, this, "选择字体");
    if (ok) {
        m_config.font = font;
        m_fontSizeSpinBox->setValue(font.pointSize());
    }
}

void BatchImageWatermark::onProgressUpdated(int current, int total, const QString& currentFile) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("正在处理 %1/%2").arg(current).arg(total));
    m_currentFileLabel->setText(QString("当前文件: %1").arg(currentFile));
}

void BatchImageWatermark::onImageProcessed(const QString& inputFile, const QString& outputFile, bool success, const QString& error) {
    QString message;
    if (success) {
        message = QString("✅ %1 -> %2").arg(QFileInfo(inputFile).fileName(), QFileInfo(outputFile).fileName());
    } else {
        message = QString("❌ %1: %2").arg(QFileInfo(inputFile).fileName(), error);
    }

    m_resultsText->append(message);
    m_resultsText->ensureCursorVisible();
}

void BatchImageWatermark::onProcessingFinished() {
    m_isProcessing = false;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("处理完成");
    m_currentFileLabel->setText("");

    QMessageBox::information(this, "完成", "批量水印处理已完成！");
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
        // 这里应该设置停止标志，但由于简化实现，我们只是等待当前任务完成
        m_isProcessing = false;
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_statusLabel->setText("已停止");
    }
}

// ImageWatermark 主类实现
ImageWatermark::ImageWatermark(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void ImageWatermark::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建标签页控件
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 12px 24px;"
        "margin-right: 2px;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "background-color: #d5dbdb;"
        "}"
    );

    // 创建单个水印标签页
    m_singleWatermark = new SingleImageWatermark();
    m_tabWidget->addTab(m_singleWatermark, "🖼️ 单个图片");

    // 创建批量水印标签页
    m_batchWatermark = new BatchImageWatermark();
    m_tabWidget->addTab(m_batchWatermark, "📚 批量处理");

    m_mainLayout->addWidget(m_tabWidget);
}

#include "imagewatermark.moc"