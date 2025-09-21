#include "opencvimageprocessor.h"
#include <QApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDir>
#include <QMimeData>

REGISTER_DYNAMICOBJECT(OpenCvImageProcessor);

OpenCvImageProcessor::OpenCvImageProcessor()
    : QWidget(nullptr)
    , DynamicObjectBase()
    , m_mainSplitter(nullptr)
    , m_imageDisplay(nullptr)
    , m_controlTabs(nullptr)
    , m_parameterPanel(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_currentMode(NONE)
    , m_videoTimer(nullptr)
    , m_isVideoMode(false)
    , m_isCameraMode(false)
    , m_worker(nullptr)
    , m_workerThread(nullptr)
    , m_isProcessing(false)
{
    setupUI();
    connectSignals();

#ifdef WITH_OPENCV
    // 初始化OpenCV
    qDebug() << "OpenCV version:" << QString::fromStdString(cv::getBuildInformation());
#else
    showError(tr("OpenCV support not available. Please rebuild with OpenCV enabled."));
#endif
}

OpenCvImageProcessor::~OpenCvImageProcessor()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

#ifdef WITH_OPENCV
    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }
#endif
}

void OpenCvImageProcessor::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // 工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout;

    m_openButton = new QPushButton(tr("打开图片"), this);
    m_openButton->setIcon(QIcon(":/icons/open.png"));
    toolbarLayout->addWidget(m_openButton);

    m_saveButton = new QPushButton(tr("保存图片"), this);
    m_saveButton->setIcon(QIcon(":/icons/save.png"));
    m_saveButton->setEnabled(false);
    toolbarLayout->addWidget(m_saveButton);

    QFrame* separator1 = new QFrame(this);
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    toolbarLayout->addWidget(separator1);

    QPushButton* openVideoButton = new QPushButton(tr("打开视频"), this);
    toolbarLayout->addWidget(openVideoButton);
    connect(openVideoButton, &QPushButton::clicked, this, &OpenCvImageProcessor::openVideo);

    QPushButton* openCameraButton = new QPushButton(tr("打开摄像头"), this);
    toolbarLayout->addWidget(openCameraButton);
    connect(openCameraButton, &QPushButton::clicked, this, &OpenCvImageProcessor::openCamera);

    QPushButton* stopVideoButton = new QPushButton(tr("停止"), this);
    toolbarLayout->addWidget(stopVideoButton);
    connect(stopVideoButton, &QPushButton::clicked, this, &OpenCvImageProcessor::stopVideo);

    QFrame* separator2 = new QFrame(this);
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    toolbarLayout->addWidget(separator2);

    m_resetButton = new QPushButton(tr("重置"), this);
    m_resetButton->setEnabled(false);
    toolbarLayout->addWidget(m_resetButton);

    m_copyButton = new QPushButton(tr("复制"), this);
    m_copyButton->setEnabled(false);
    toolbarLayout->addWidget(m_copyButton);

    m_pasteButton = new QPushButton(tr("粘贴"), this);
    toolbarLayout->addWidget(m_pasteButton);

    toolbarLayout->addStretch();

    mainLayout->addLayout(toolbarLayout);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    setupImageDisplay();
    setupControlPanel();

    m_mainSplitter->addWidget(m_imageDisplay);
    m_mainSplitter->addWidget(m_controlTabs);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_mainSplitter);

    setupStatusBar();
    mainLayout->addLayout(setupStatusBar());

    setLayout(mainLayout);
}

void OpenCvImageProcessor::setupImageDisplay()
{
    m_imageDisplay = new ImageDisplayWidget(this);
    m_imageDisplay->setMinimumSize(400, 300);
}

void OpenCvImageProcessor::setupControlPanel()
{
    m_controlTabs = new QTabWidget(this);
    m_controlTabs->setMaximumWidth(350);

    // 处理模式选择页面
    QWidget* modeTab = new QWidget;
    QVBoxLayout* modeLayout = new QVBoxLayout(modeTab);

    // 处理模式选择
    QGroupBox* modeGroup = new QGroupBox(tr("处理模式"), modeTab);
    QVBoxLayout* modeGroupLayout = new QVBoxLayout(modeGroup);

    m_modeCombo = new QComboBox(modeGroup);
    m_modeCombo->addItem(tr("无处理"), NONE);
    m_modeCombo->addItem(tr("灰度化"), GRAYSCALE);
    m_modeCombo->addItem(tr("二值化"), BINARY_THRESHOLD);
    m_modeCombo->addItem(tr("自适应阈值"), ADAPTIVE_THRESHOLD);
    m_modeCombo->addItem(tr("高斯模糊"), GAUSSIAN_BLUR);
    m_modeCombo->addItem(tr("中值滤波"), MEDIAN_BLUR);
    m_modeCombo->addItem(tr("双边滤波"), BILATERAL_FILTER);
    m_modeCombo->addItem(tr("锐化"), SHARPEN);
    m_modeCombo->addItem(tr("直方图均衡化"), HISTOGRAM_EQUALIZATION);
    m_modeCombo->addItem(tr("CLAHE"), CLAHE);
    m_modeCombo->addItem(tr("Canny边缘检测"), CANNY_EDGE);
    m_modeCombo->addItem(tr("Sobel边缘检测"), SOBEL_EDGE);
    m_modeCombo->addItem(tr("Laplacian边缘检测"), LAPLACIAN_EDGE);
    m_modeCombo->addItem(tr("Harris角点检测"), HARRIS_CORNERS);
    m_modeCombo->addItem(tr("Shi-Tomasi角点检测"), SHI_TOMASI_CORNERS);
    m_modeCombo->addItem(tr("缩放"), RESIZE);
    m_modeCombo->addItem(tr("旋转"), ROTATE);
    m_modeCombo->addItem(tr("仿射变换"), AFFINE_TRANSFORM);
    m_modeCombo->addItem(tr("透视变换"), PERSPECTIVE_TRANSFORM);
    m_modeCombo->addItem(tr("均值滤波"), MEAN_FILTER);
    m_modeCombo->addItem(tr("高斯滤波"), GAUSSIAN_FILTER);
    m_modeCombo->addItem(tr("Sobel滤波"), SOBEL_FILTER);
    m_modeCombo->addItem(tr("Laplacian滤波"), LAPLACIAN_FILTER);

    modeGroupLayout->addWidget(m_modeCombo);

    m_processButton = new QPushButton(tr("应用处理"), modeGroup);
    m_processButton->setEnabled(false);
    modeGroupLayout->addWidget(m_processButton);

    modeLayout->addWidget(modeGroup);
    modeLayout->addStretch();

    m_controlTabs->addTab(modeTab, tr("处理模式"));

    // 参数页面
    m_parameterPanel = new ParameterPanel(this);
    QScrollArea* paramScrollArea = new QScrollArea;
    paramScrollArea->setWidget(m_parameterPanel);
    paramScrollArea->setWidgetResizable(true);
    m_controlTabs->addTab(paramScrollArea, tr("参数设置"));

    // 信息页面
    QWidget* infoTab = new QWidget;
    QVBoxLayout* infoLayout = new QVBoxLayout(infoTab);

    QTextEdit* infoText = new QTextEdit(infoTab);
    infoText->setReadOnly(true);
    infoText->setHtml(tr(
        "<h3>OpenCV图像处理工具</h3>"
        "<p><b>功能介绍：</b></p>"
        "<ul>"
        "<li><b>基础处理：</b>灰度化、二值化、模糊、锐化等</li>"
        "<li><b>边缘检测：</b>Canny、Sobel、Laplacian</li>"
        "<li><b>角点检测：</b>Harris、Shi-Tomasi</li>"
        "<li><b>几何变换：</b>缩放、旋转、仿射、透视变换</li>"
        "<li><b>滤波处理：</b>各种滤波器</li>"
        "</ul>"
        "<p><b>使用方法：</b></p>"
        "<ol>"
        "<li>点击\"打开图片\"加载图像</li>"
        "<li>选择处理模式</li>"
        "<li>调整参数</li>"
        "<li>点击\"应用处理\"</li>"
        "<li>保存或复制结果</li>"
        "</ol>"
    ));
    infoLayout->addWidget(infoText);

    m_controlTabs->addTab(infoTab, tr("使用说明"));
}

QHBoxLayout* OpenCvImageProcessor::setupStatusBar()
{
    QHBoxLayout* statusLayout = new QHBoxLayout;

    m_statusLabel = new QLabel(tr("就绪"), this);
    statusLayout->addWidget(m_statusLabel);

    statusLayout->addStretch();

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    statusLayout->addWidget(m_progressBar);

    return statusLayout;
}

void OpenCvImageProcessor::connectSignals()
{
    connect(m_openButton, &QPushButton::clicked, this, &OpenCvImageProcessor::openImage);
    connect(m_saveButton, &QPushButton::clicked, this, &OpenCvImageProcessor::saveImage);
    connect(m_resetButton, &QPushButton::clicked, this, &OpenCvImageProcessor::resetImage);
    connect(m_copyButton, &QPushButton::clicked, this, &OpenCvImageProcessor::copyToClipboard);
    connect(m_pasteButton, &QPushButton::clicked, this, &OpenCvImageProcessor::pasteFromClipboard);
    connect(m_processButton, &QPushButton::clicked, this, &OpenCvImageProcessor::processImage);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenCvImageProcessor::onProcessingModeChanged);

    // 设置视频定时器
    m_videoTimer = new QTimer(this);
    connect(m_videoTimer, &QTimer::timeout, this, &OpenCvImageProcessor::onFrameReady);

    // 设置工作线程
    m_worker = new ProcessingWorker;
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &ProcessingWorker::processImage);
    connect(m_worker, &ProcessingWorker::imageProcessed, this, [this](const QImage& result) {
        m_currentQImage = result;
        m_imageDisplay->setImage(result);
        m_isProcessing = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText(tr("处理完成"));
        onProcessingFinished();
    });
    connect(m_worker, &ProcessingWorker::processingError, this, [this](const QString& error) {
        showError(error);
        m_isProcessing = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText(tr("处理失败"));
    });
    connect(m_worker, &ProcessingWorker::progressChanged, m_progressBar, &QProgressBar::setValue);
}

void OpenCvImageProcessor::openImage()
{
    QString filter = tr("图像文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.webp *.gif);;所有文件 (*.*)");
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开图像"), QString(), filter);

    if (fileName.isEmpty()) {
        return;
    }

    QImage image(fileName);
    if (image.isNull()) {
        showError(tr("无法加载图像文件: %1").arg(fileName));
        return;
    }

    m_currentImagePath = fileName;
    m_currentQImage = image;

#ifdef WITH_OPENCV
    // 同时加载到OpenCV
    m_originalImage = cv::imread(fileName.toLocal8Bit().toStdString(), cv::IMREAD_COLOR);
    if (m_originalImage.empty()) {
        showError(tr("OpenCV无法加载图像文件: %1").arg(fileName));
        return;
    }
    m_processedImage = m_originalImage.clone();
#endif

    m_imageDisplay->setImage(image);

    // 启用控件
    m_saveButton->setEnabled(true);
    m_resetButton->setEnabled(true);
    m_copyButton->setEnabled(true);
    m_processButton->setEnabled(true);

    m_statusLabel->setText(tr("已加载: %1").arg(QFileInfo(fileName).fileName()));
    updateStatusInfo();
}

void OpenCvImageProcessor::saveImage()
{
    if (m_currentQImage.isNull()) {
        showError(tr("没有图像可保存"));
        return;
    }

    QString filter = tr("PNG文件 (*.png);;JPEG文件 (*.jpg);;BMP文件 (*.bmp);;TIFF文件 (*.tiff)");
    QString fileName = QFileDialog::getSaveFileName(this, tr("保存图像"), QString(), filter);

    if (fileName.isEmpty()) {
        return;
    }

    if (m_currentQImage.save(fileName)) {
        m_statusLabel->setText(tr("已保存: %1").arg(QFileInfo(fileName).fileName()));
    } else {
        showError(tr("保存图像失败: %1").arg(fileName));
    }
}

void OpenCvImageProcessor::resetImage()
{
#ifdef WITH_OPENCV
    if (!m_originalImage.empty()) {
        m_processedImage = m_originalImage.clone();
        m_currentQImage = cvMatToQImage(m_originalImage);
        m_imageDisplay->setImage(m_currentQImage);
        m_statusLabel->setText(tr("已重置到原始图像"));
    }
#endif
}

void OpenCvImageProcessor::copyToClipboard()
{
    if (m_currentQImage.isNull()) {
        showError(tr("没有图像可复制"));
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setImage(m_currentQImage);
    m_statusLabel->setText(tr("图像已复制到剪贴板"));
}

void OpenCvImageProcessor::pasteFromClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    if (mimeData->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            m_currentQImage = image;
            m_imageDisplay->setImage(image);

#ifdef WITH_OPENCV
            m_originalImage = qImageToCvMat(image);
            m_processedImage = m_originalImage.clone();
#endif

            // 启用控件
            m_saveButton->setEnabled(true);
            m_resetButton->setEnabled(true);
            m_copyButton->setEnabled(true);
            m_processButton->setEnabled(true);

            m_statusLabel->setText(tr("从剪贴板粘贴图像"));
            updateStatusInfo();
        }
    } else {
        showError(tr("剪贴板中没有图像数据"));
    }
}

void OpenCvImageProcessor::onProcessingModeChanged()
{
    m_currentMode = static_cast<ProcessingMode>(m_modeCombo->currentData().toInt());
    m_parameterPanel->updateForMode(m_currentMode);
}

void OpenCvImageProcessor::onParameterChanged()
{
    // 参数变化时自动处理（可选）
    if (m_currentMode != NONE && !m_isProcessing) {
        // processImage(); // 取消自动处理，改为手动点击
    }
}

void OpenCvImageProcessor::processImage()
{
#ifdef WITH_OPENCV
    if (m_originalImage.empty()) {
        showError(tr("请先加载图像"));
        return;
    }

    if (m_isProcessing) {
        return;
    }

    m_isProcessing = true;
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("处理中..."));

    // 准备参数
    QMap<QString, QVariant> parameters;

    // 根据模式获取参数
    switch (m_currentMode) {
    case BINARY_THRESHOLD:
        parameters["threshold"] = m_parameterPanel->getDoubleParameter("threshold");
        parameters["maxval"] = m_parameterPanel->getDoubleParameter("maxval");
        break;
    case CANNY_EDGE:
        parameters["low_threshold"] = m_parameterPanel->getDoubleParameter("low_threshold");
        parameters["high_threshold"] = m_parameterPanel->getDoubleParameter("high_threshold");
        break;
    case GAUSSIAN_BLUR:
    case MEDIAN_BLUR:
        parameters["kernel_size"] = m_parameterPanel->getIntParameter("kernel_size");
        break;
    case RESIZE:
        parameters["scale_x"] = m_parameterPanel->getDoubleParameter("scale_x");
        parameters["scale_y"] = m_parameterPanel->getDoubleParameter("scale_y");
        break;
    case ROTATE:
        parameters["angle"] = m_parameterPanel->getDoubleParameter("angle");
        break;
    default:
        break;
    }

    // 设置工作线程参数并开始处理
    m_worker->setInputImage(m_originalImage);
    m_worker->setProcessingMode(m_currentMode);
    m_worker->setParameters(parameters);

    if (!m_workerThread->isRunning()) {
        m_workerThread->start();
    } else {
        QMetaObject::invokeMethod(m_worker, "processImage", Qt::QueuedConnection);
    }

#else
    showError(tr("OpenCV支持未启用"));
#endif
}

void OpenCvImageProcessor::onProcessingFinished()
{
    m_saveButton->setEnabled(true);
    m_copyButton->setEnabled(true);
    updateStatusInfo();
}

void OpenCvImageProcessor::openVideo()
{
#ifdef WITH_OPENCV
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("打开视频"), QString(),
        tr("视频文件 (*.mp4 *.avi *.mov *.wmv *.mkv);;所有文件 (*.*)"));

    if (fileName.isEmpty()) {
        return;
    }

    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }

    m_videoCapture.open(fileName.toLocal8Bit().toStdString());

    if (!m_videoCapture.isOpened()) {
        showError(tr("无法打开视频文件: %1").arg(fileName));
        return;
    }

    m_isVideoMode = true;
    m_isCameraMode = false;
    m_videoTimer->start(30); // 约30fps

    m_statusLabel->setText(tr("视频模式: %1").arg(QFileInfo(fileName).fileName()));
#else
    showError(tr("OpenCV支持未启用"));
#endif
}

void OpenCvImageProcessor::openCamera()
{
#ifdef WITH_OPENCV
    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }

    m_videoCapture.open(0); // 默认摄像头

    if (!m_videoCapture.isOpened()) {
        showError(tr("无法打开摄像头"));
        return;
    }

    m_isVideoMode = true;
    m_isCameraMode = true;
    m_videoTimer->start(30); // 约30fps

    m_statusLabel->setText(tr("摄像头模式"));
#else
    showError(tr("OpenCV支持未启用"));
#endif
}

void OpenCvImageProcessor::stopVideo()
{
#ifdef WITH_OPENCV
    if (m_videoTimer->isActive()) {
        m_videoTimer->stop();
    }

    if (m_videoCapture.isOpened()) {
        m_videoCapture.release();
    }

    m_isVideoMode = false;
    m_isCameraMode = false;

    m_statusLabel->setText(tr("已停止视频/摄像头"));
#endif
}

void OpenCvImageProcessor::onFrameReady()
{
#ifdef WITH_OPENCV
    if (!m_videoCapture.isOpened()) {
        return;
    }

    cv::Mat frame;
    m_videoCapture >> frame;

    if (frame.empty()) {
        if (!m_isCameraMode) {
            // 视频结束
            stopVideo();
        }
        return;
    }

    // 更新原始图像
    m_originalImage = frame.clone();

    // 如果有处理模式，应用处理
    if (m_currentMode != NONE) {
        try {
            cv::Mat processed;
            switch (m_currentMode) {
            case GRAYSCALE:
                cv::cvtColor(frame, processed, cv::COLOR_BGR2GRAY);
                cv::cvtColor(processed, processed, cv::COLOR_GRAY2BGR);
                break;
            case CANNY_EDGE:
                {
                    cv::Mat gray, edges;
                    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
                    cv::Canny(gray, edges, 50, 150);
                    cv::cvtColor(edges, processed, cv::COLOR_GRAY2BGR);
                }
                break;
            default:
                processed = frame;
                break;
            }
            frame = processed;
        } catch (const cv::Exception& e) {
            qDebug() << "OpenCV处理错误:" << QString::fromStdString(e.what());
        }
    }

    m_currentQImage = cvMatToQImage(frame);
    m_imageDisplay->setImage(m_currentQImage);
#endif
}

#ifdef WITH_OPENCV
cv::Mat OpenCvImageProcessor::qImageToCvMat(const QImage& qimage)
{
    QImage swapped = qimage.rgbSwapped();
    return cv::Mat(swapped.height(), swapped.width(), CV_8UC3,
                   (void*)swapped.constBits(), swapped.bytesPerLine()).clone();
}

QImage OpenCvImageProcessor::cvMatToQImage(const cv::Mat& cvmat)
{
    switch (cvmat.type()) {
    case CV_8UC1:
        {
            cv::Mat rgb;
            cv::cvtColor(cvmat, rgb, cv::COLOR_GRAY2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        }
    case CV_8UC3:
        {
            cv::Mat rgb;
            cv::cvtColor(cvmat, rgb, cv::COLOR_BGR2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        }
    case CV_8UC4:
        {
            cv::Mat rgba;
            cv::cvtColor(cvmat, rgba, cv::COLOR_BGRA2RGBA);
            return QImage(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888);
        }
    }
    return QImage();
}
#endif

void OpenCvImageProcessor::updateStatusInfo()
{
#ifdef WITH_OPENCV
    if (!m_originalImage.empty()) {
        QString info = tr("尺寸: %1x%2, 通道: %3")
                       .arg(m_originalImage.cols)
                       .arg(m_originalImage.rows)
                       .arg(m_originalImage.channels());
        m_statusLabel->setText(info);
    }
#endif
}

void OpenCvImageProcessor::showError(const QString& message)
{
    QMessageBox::warning(this, tr("错误"), message);
    m_statusLabel->setText(tr("错误: %1").arg(message));
}

// ========== ImageDisplayWidget Implementation ==========

ImageDisplayWidget::ImageDisplayWidget(QWidget* parent)
    : QWidget(parent)
    , m_scaleFactor(1.0)
    , m_dragging(false)
{
    setMinimumSize(200, 200);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageDisplayWidget::setImage(const QImage& image)
{
    m_image = image;
    if (!m_image.isNull()) {
        updateTransform();
        fitToWindow();
    }
    update();
}

void ImageDisplayWidget::clearImage()
{
    m_image = QImage();
    m_scaledPixmap = QPixmap();
    update();
}

void ImageDisplayWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    painter.fillRect(rect, QColor(64, 64, 64));

    if (m_image.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect, Qt::AlignCenter, tr("请加载图像"));
        return;
    }

    // 绘制图像
    QRect imageRect = m_imageRect.translated(m_offset);
    painter.drawPixmap(imageRect, m_scaledPixmap);

    // 绘制边框
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    painter.drawRect(imageRect);
}

void ImageDisplayWidget::wheelEvent(QWheelEvent* event)
{
    if (m_image.isNull()) return;

    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        m_scaleFactor *= scaleFactor;
    } else {
        m_scaleFactor /= scaleFactor;
    }

    m_scaleFactor = qBound(0.1, m_scaleFactor, 10.0);
    updateTransform();
    update();
}

void ImageDisplayWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastPanPoint = event->pos();
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageDisplayWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_offset += delta;
        m_lastPanPoint = event->pos();
        update();
    }
}

void ImageDisplayWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageDisplayWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_image.isNull()) {
        updateTransform();
    }
}

void ImageDisplayWidget::updateTransform()
{
    if (m_image.isNull()) return;

    QSize scaledSize = m_image.size() * m_scaleFactor;
    m_scaledPixmap = QPixmap::fromImage(m_image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_imageRect = QRect(QPoint(0, 0), scaledSize);
}

void ImageDisplayWidget::fitToWindow()
{
    if (m_image.isNull()) return;

    QSize widgetSize = size();
    QSize imageSize = m_image.size();

    double xScale = double(widgetSize.width()) / imageSize.width();
    double yScale = double(widgetSize.height()) / imageSize.height();

    m_scaleFactor = qMin(xScale, yScale) * 0.9; // 留点边距
    updateTransform();

    // 居中显示
    m_offset = QPoint((widgetSize.width() - m_imageRect.width()) / 2,
                      (widgetSize.height() - m_imageRect.height()) / 2);
    update();
}

// ========== ParameterPanel Implementation ==========

ParameterPanel::ParameterPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentRow(0)
{
    m_layout = new QGridLayout(this);
    m_layout->setAlignment(Qt::AlignTop);
    setLayout(m_layout);
}

void ParameterPanel::updateForMode(OpenCvImageProcessor::ProcessingMode mode)
{
    clearParameters();

    switch (mode) {
    case OpenCvImageProcessor::BINARY_THRESHOLD:
        addDoubleParameter("threshold", tr("阈值"), 127.0, 0.0, 255.0, 1.0);
        addDoubleParameter("maxval", tr("最大值"), 255.0, 0.0, 255.0, 1.0);
        break;

    case OpenCvImageProcessor::CANNY_EDGE:
        addDoubleParameter("low_threshold", tr("低阈值"), 50.0, 0.0, 255.0, 1.0);
        addDoubleParameter("high_threshold", tr("高阈值"), 150.0, 0.0, 255.0, 1.0);
        break;

    case OpenCvImageProcessor::GAUSSIAN_BLUR:
        addIntParameter("kernel_size", tr("核大小"), 5, 1, 31, 2);
        break;

    case OpenCvImageProcessor::MEDIAN_BLUR:
        addIntParameter("kernel_size", tr("核大小"), 5, 1, 31, 2);
        break;

    case OpenCvImageProcessor::RESIZE:
        addDoubleParameter("scale_x", tr("X缩放"), 1.0, 0.1, 5.0, 0.1);
        addDoubleParameter("scale_y", tr("Y缩放"), 1.0, 0.1, 5.0, 0.1);
        break;

    case OpenCvImageProcessor::ROTATE:
        addDoubleParameter("angle", tr("角度"), 0.0, -180.0, 180.0, 1.0);
        break;

    default:
        {
            QLabel* noParamsLabel = new QLabel(tr("此模式无需参数"), this);
            m_layout->addWidget(noParamsLabel, 0, 0, 1, 2);
        }
        break;
    }
}

double ParameterPanel::getDoubleParameter(const QString& name) const
{
    QWidget* widget = m_parameters.value(name);
    if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
        return spinBox->value();
    }
    return 0.0;
}

int ParameterPanel::getIntParameter(const QString& name) const
{
    QWidget* widget = m_parameters.value(name);
    if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget)) {
        return spinBox->value();
    }
    return 0;
}

bool ParameterPanel::getBoolParameter(const QString& name) const
{
    QWidget* widget = m_parameters.value(name);
    if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
        return checkBox->isChecked();
    }
    return false;
}

QString ParameterPanel::getStringParameter(const QString& name) const
{
    QWidget* widget = m_parameters.value(name);
    if (QComboBox* comboBox = qobject_cast<QComboBox*>(widget)) {
        return comboBox->currentText();
    }
    return QString();
}

void ParameterPanel::onParameterChanged()
{
    emit parameterChanged();
}

void ParameterPanel::clearParameters()
{
    // 清除所有参数控件
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_parameters.clear();
    m_currentRow = 0;
}

void ParameterPanel::addDoubleParameter(const QString& name, const QString& label,
                                       double value, double min, double max, double step)
{
    QLabel* lblWidget = new QLabel(label, this);
    QDoubleSpinBox* spinBox = new QDoubleSpinBox(this);
    spinBox->setRange(min, max);
    spinBox->setSingleStep(step);
    spinBox->setValue(value);
    spinBox->setDecimals(1);

    connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParameterPanel::onParameterChanged);

    m_layout->addWidget(lblWidget, m_currentRow, 0);
    m_layout->addWidget(spinBox, m_currentRow, 1);
    m_parameters[name] = spinBox;
    m_currentRow++;
}

void ParameterPanel::addIntParameter(const QString& name, const QString& label,
                                    int value, int min, int max, int step)
{
    QLabel* lblWidget = new QLabel(label, this);
    QSpinBox* spinBox = new QSpinBox(this);
    spinBox->setRange(min, max);
    spinBox->setSingleStep(step);
    spinBox->setValue(value);

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ParameterPanel::onParameterChanged);

    m_layout->addWidget(lblWidget, m_currentRow, 0);
    m_layout->addWidget(spinBox, m_currentRow, 1);
    m_parameters[name] = spinBox;
    m_currentRow++;
}

void ParameterPanel::addBoolParameter(const QString& name, const QString& label, bool value)
{
    QCheckBox* checkBox = new QCheckBox(label, this);
    checkBox->setChecked(value);

    connect(checkBox, &QCheckBox::toggled, this, &ParameterPanel::onParameterChanged);

    m_layout->addWidget(checkBox, m_currentRow, 0, 1, 2);
    m_parameters[name] = checkBox;
    m_currentRow++;
}

void ParameterPanel::addComboParameter(const QString& name, const QString& label,
                                      const QStringList& items, int currentIndex)
{
    QLabel* lblWidget = new QLabel(label, this);
    QComboBox* comboBox = new QComboBox(this);
    comboBox->addItems(items);
    comboBox->setCurrentIndex(currentIndex);

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterPanel::onParameterChanged);

    m_layout->addWidget(lblWidget, m_currentRow, 0);
    m_layout->addWidget(comboBox, m_currentRow, 1);
    m_parameters[name] = comboBox;
    m_currentRow++;
}

// ========== ProcessingWorker Implementation ==========

ProcessingWorker::ProcessingWorker(QObject* parent)
    : QObject(parent)
#ifdef WITH_OPENCV
    , m_mode(OpenCvImageProcessor::NONE)
#endif
{
}

#ifdef WITH_OPENCV
void ProcessingWorker::setInputImage(const cv::Mat& image)
{
    m_inputImage = image.clone();
}

void ProcessingWorker::setProcessingMode(OpenCvImageProcessor::ProcessingMode mode)
{
    m_mode = mode;
}

void ProcessingWorker::setParameters(const QMap<QString, QVariant>& params)
{
    m_parameters = params;
}
#endif

void ProcessingWorker::processImage()
{
#ifdef WITH_OPENCV
    if (m_inputImage.empty()) {
        emit processingError(tr("输入图像为空"));
        return;
    }

    try {
        emit progressChanged(10);

        cv::Mat result;

        switch (m_mode) {
        case OpenCvImageProcessor::NONE:
            result = m_inputImage.clone();
            break;

        case OpenCvImageProcessor::GRAYSCALE:
            if (m_inputImage.channels() == 3) {
                cv::cvtColor(m_inputImage, result, cv::COLOR_BGR2GRAY);
            } else {
                result = m_inputImage.clone();
            }
            break;

        case OpenCvImageProcessor::BINARY_THRESHOLD:
            {
                cv::Mat gray;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                double threshold = m_parameters.value("threshold", 127.0).toDouble();
                double maxval = m_parameters.value("maxval", 255.0).toDouble();
                cv::threshold(gray, result, threshold, maxval, cv::THRESH_BINARY);
            }
            break;

        case OpenCvImageProcessor::CANNY_EDGE:
            {
                cv::Mat gray;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                double low = m_parameters.value("low_threshold", 50.0).toDouble();
                double high = m_parameters.value("high_threshold", 150.0).toDouble();
                cv::Canny(gray, result, low, high);
            }
            break;

        case OpenCvImageProcessor::GAUSSIAN_BLUR:
            {
                int kernelSize = m_parameters.value("kernel_size", 5).toInt();
                if (kernelSize % 2 == 0) kernelSize++; // 确保是奇数
                cv::GaussianBlur(m_inputImage, result, cv::Size(kernelSize, kernelSize), 0);
            }
            break;

        case OpenCvImageProcessor::MEDIAN_BLUR:
            {
                int kernelSize = m_parameters.value("kernel_size", 5).toInt();
                if (kernelSize % 2 == 0) kernelSize++; // 确保是奇数
                cv::medianBlur(m_inputImage, result, kernelSize);
            }
            break;

        case OpenCvImageProcessor::RESIZE:
            {
                double scaleX = m_parameters.value("scale_x", 1.0).toDouble();
                double scaleY = m_parameters.value("scale_y", 1.0).toDouble();
                cv::resize(m_inputImage, result, cv::Size(0, 0), scaleX, scaleY);
            }
            break;

        case OpenCvImageProcessor::ROTATE:
            {
                double angle = m_parameters.value("angle", 0.0).toDouble();
                cv::Point2f center(m_inputImage.cols / 2.0, m_inputImage.rows / 2.0);
                cv::Mat rotMat = cv::getRotationMatrix2D(center, angle, 1.0);
                cv::warpAffine(m_inputImage, result, rotMat, m_inputImage.size());
            }
            break;

        case OpenCvImageProcessor::ADAPTIVE_THRESHOLD:
            {
                cv::Mat gray;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                cv::adaptiveThreshold(gray, result, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 11, 2);
            }
            break;

        case OpenCvImageProcessor::BILATERAL_FILTER:
            {
                cv::bilateralFilter(m_inputImage, result, 9, 75, 75);
            }
            break;

        case OpenCvImageProcessor::SHARPEN:
            {
                cv::Mat kernel = (cv::Mat_<float>(3,3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
                cv::filter2D(m_inputImage, result, -1, kernel);
            }
            break;

        case OpenCvImageProcessor::HISTOGRAM_EQUALIZATION:
            {
                if (m_inputImage.channels() == 1) {
                    cv::equalizeHist(m_inputImage, result);
                } else {
                    cv::Mat ycrcb;
                    cv::cvtColor(m_inputImage, ycrcb, cv::COLOR_BGR2YCrCb);
                    std::vector<cv::Mat> channels;
                    cv::split(ycrcb, channels);
                    cv::equalizeHist(channels[0], channels[0]);
                    cv::merge(channels, ycrcb);
                    cv::cvtColor(ycrcb, result, cv::COLOR_YCrCb2BGR);
                }
            }
            break;

        case OpenCvImageProcessor::CLAHE:
            {
                cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8,8));
                if (m_inputImage.channels() == 1) {
                    clahe->apply(m_inputImage, result);
                } else {
                    cv::Mat lab;
                    cv::cvtColor(m_inputImage, lab, cv::COLOR_BGR2Lab);
                    std::vector<cv::Mat> channels;
                    cv::split(lab, channels);
                    clahe->apply(channels[0], channels[0]);
                    cv::merge(channels, lab);
                    cv::cvtColor(lab, result, cv::COLOR_Lab2BGR);
                }
            }
            break;

        case OpenCvImageProcessor::SOBEL_EDGE:
            {
                cv::Mat gray, grad_x, grad_y, abs_grad_x, abs_grad_y;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                cv::Sobel(gray, grad_x, CV_16S, 1, 0, 3);
                cv::Sobel(gray, grad_y, CV_16S, 0, 1, 3);
                cv::convertScaleAbs(grad_x, abs_grad_x);
                cv::convertScaleAbs(grad_y, abs_grad_y);
                cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, result);
            }
            break;

        case OpenCvImageProcessor::LAPLACIAN_EDGE:
            {
                cv::Mat gray, laplacian;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                cv::Laplacian(gray, laplacian, CV_16S, 3);
                cv::convertScaleAbs(laplacian, result);
            }
            break;

        case OpenCvImageProcessor::HARRIS_CORNERS:
            {
                cv::Mat gray, harris_corners, harris_norm;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                result = m_inputImage.clone();
                cv::cornerHarris(gray, harris_corners, 2, 3, 0.04);
                cv::normalize(harris_corners, harris_norm, 0, 255, cv::NORM_MINMAX, CV_32FC1, cv::Mat());

                for (int i = 0; i < harris_norm.rows; i++) {
                    for (int j = 0; j < harris_norm.cols; j++) {
                        if ((int)harris_norm.at<float>(i, j) > 100) {
                            cv::circle(result, cv::Point(j, i), 4, cv::Scalar(0, 0, 255), 2);
                        }
                    }
                }
            }
            break;

        case OpenCvImageProcessor::SHI_TOMASI_CORNERS:
            {
                cv::Mat gray;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                result = m_inputImage.clone();
                std::vector<cv::Point2f> corners;
                cv::goodFeaturesToTrack(gray, corners, 100, 0.01, 10);

                for (size_t i = 0; i < corners.size(); i++) {
                    cv::circle(result, corners[i], 4, cv::Scalar(0, 255, 0), 2);
                }
            }
            break;

        case OpenCvImageProcessor::AFFINE_TRANSFORM:
            {
                cv::Point2f srcTri[3] = {cv::Point2f(0, 0), cv::Point2f(m_inputImage.cols - 1, 0), cv::Point2f(0, m_inputImage.rows - 1)};
                cv::Point2f dstTri[3] = {cv::Point2f(0, m_inputImage.rows * 0.33f), cv::Point2f(m_inputImage.cols * 0.85f, m_inputImage.rows * 0.25f), cv::Point2f(m_inputImage.cols * 0.15f, m_inputImage.rows * 0.7f)};
                cv::Mat warp_mat = cv::getAffineTransform(srcTri, dstTri);
                cv::warpAffine(m_inputImage, result, warp_mat, m_inputImage.size());
            }
            break;

        case OpenCvImageProcessor::PERSPECTIVE_TRANSFORM:
            {
                cv::Point2f src[4] = {cv::Point2f(0, 0), cv::Point2f(m_inputImage.cols, 0), cv::Point2f(m_inputImage.cols, m_inputImage.rows), cv::Point2f(0, m_inputImage.rows)};
                cv::Point2f dst[4] = {cv::Point2f(0, 0), cv::Point2f(m_inputImage.cols * 0.8f, 0), cv::Point2f(m_inputImage.cols, m_inputImage.rows), cv::Point2f(m_inputImage.cols * 0.2f, m_inputImage.rows)};
                cv::Mat perspective_mat = cv::getPerspectiveTransform(src, dst);
                cv::warpPerspective(m_inputImage, result, perspective_mat, m_inputImage.size());
            }
            break;

        case OpenCvImageProcessor::MEAN_FILTER:
            {
                cv::blur(m_inputImage, result, cv::Size(5, 5));
            }
            break;

        case OpenCvImageProcessor::GAUSSIAN_FILTER:
            {
                cv::GaussianBlur(m_inputImage, result, cv::Size(5, 5), 0);
            }
            break;

        case OpenCvImageProcessor::SOBEL_FILTER:
            {
                cv::Mat gray, grad_x, grad_y, abs_grad_x, abs_grad_y;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                cv::Sobel(gray, grad_x, CV_16S, 1, 0, 3);
                cv::Sobel(gray, grad_y, CV_16S, 0, 1, 3);
                cv::convertScaleAbs(grad_x, abs_grad_x);
                cv::convertScaleAbs(grad_y, abs_grad_y);
                cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, result);
            }
            break;

        case OpenCvImageProcessor::LAPLACIAN_FILTER:
            {
                cv::Mat gray, laplacian;
                if (m_inputImage.channels() == 3) {
                    cv::cvtColor(m_inputImage, gray, cv::COLOR_BGR2GRAY);
                } else {
                    gray = m_inputImage.clone();
                }
                cv::Laplacian(gray, laplacian, CV_16S, 3);
                cv::convertScaleAbs(laplacian, result);
            }
            break;

        default:
            result = m_inputImage.clone();
            break;
        }

        emit progressChanged(90);

        // 转换为QImage
        QImage qResult;
        switch (result.type()) {
        case CV_8UC1:
            {
                cv::Mat rgb;
                cv::cvtColor(result, rgb, cv::COLOR_GRAY2RGB);
                qResult = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
            }
            break;
        case CV_8UC3:
            {
                cv::Mat rgb;
                cv::cvtColor(result, rgb, cv::COLOR_BGR2RGB);
                qResult = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
            }
            break;
        case CV_8UC4:
            {
                cv::Mat rgba;
                cv::cvtColor(result, rgba, cv::COLOR_BGRA2RGBA);
                qResult = QImage(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888).copy();
            }
            break;
        default:
            qResult = QImage();
            break;
        }

        emit progressChanged(100);
        emit imageProcessed(qResult);

    } catch (const cv::Exception& e) {
        emit processingError(tr("OpenCV处理错误: %1").arg(QString::fromStdString(e.what())));
    } catch (const std::exception& e) {
        emit processingError(tr("处理错误: %1").arg(QString::fromStdString(e.what())));
    }
#else
    emit processingError(tr("OpenCV支持未启用"));
#endif
}

// MOC include is handled automatically by CMake