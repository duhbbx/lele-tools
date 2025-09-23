#include "cameratool.h"
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QSplitter>
#include <QGuiApplication>
#include <QClipboard>

REGISTER_DYNAMICOBJECT(CameraTool);

CameraTool::CameraTool(QWidget *parent)
    : QWidget(parent), DynamicObjectBase()
#ifdef WITH_QT_MULTIMEDIA
    , m_camera(nullptr)
    , m_captureSession(nullptr)
    , m_imageCapture(nullptr)
#endif
    , m_cameraActive(false)
    , m_recording(false)
    , m_photoCount(0)
{
    setupUI();
    loadSettings();
    
#ifdef WITH_QT_MULTIMEDIA
    refreshCameraList();
    
    // 创建定时器用于更新状态
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &CameraTool::updateUI);
    m_updateTimer->start(1000); // 每秒更新一次
    
    // 初始化媒体捕获会话
    m_captureSession = new QMediaCaptureSession(this);
    m_captureSession->setVideoOutput(m_videoWidget);
    
    logMessage(tr("摄像头工具初始化完成"));
#else
    logMessage(tr("Qt Multimedia模块不可用，摄像头功能已禁用"));
    logMessage(tr("请安装Qt Multimedia模块以启用摄像头功能"));
    
    // 禁用所有摄像头相关控件
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_takePhotoBtn->setEnabled(false);
    m_startRecordBtn->setEnabled(false);
    m_stopRecordBtn->setEnabled(false);
    m_refreshBtn->setEnabled(false);
    m_cameraCombo->setEnabled(false);
    m_resolutionCombo->setEnabled(false);
    
    m_cameraStatusLabel->setText(tr("Qt Multimedia不可用"));
    m_cameraStatusLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
#endif
}

CameraTool::~CameraTool()
{
#ifdef WITH_QT_MULTIMEDIA
    if (m_camera && m_camera->isActive()) {
        m_camera->stop();
    }
#endif
    saveSettings();
}

void CameraTool::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // 创建主分割器
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧控制面板
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(10);
    
    setupCameraControls();
    setupCaptureControls();
    setupScreenshotControls();
    setupSettingsArea();
    
    leftLayout->addWidget(m_cameraGroup);
    leftLayout->addWidget(m_captureGroup);
    leftLayout->addWidget(m_screenshotGroup);
    leftLayout->addWidget(m_settingsGroup);
    leftLayout->addStretch();
    
    // 右侧预览和状态面板
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(10);
    
    setupPreviewArea();
    setupStatusArea();
    
    rightLayout->addWidget(m_previewGroup);
    rightLayout->addWidget(m_statusGroup);
    
    // 设置分割器比例
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({350, 500});
    m_mainLayout->addWidget(mainSplitter);
}

void CameraTool::setupCameraControls()
{
    m_cameraGroup = new QGroupBox(tr("摄像头控制"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_cameraGroup);
    layout->setSpacing(8);
    
    // 摄像头选择
    QHBoxLayout *cameraLayout = new QHBoxLayout();
    cameraLayout->addWidget(new QLabel(tr("摄像头:")));
    m_cameraCombo = new QComboBox();
    m_cameraCombo->setMinimumWidth(200);
    m_refreshBtn = new QPushButton(tr("刷新"));
    m_refreshBtn->setMaximumWidth(60);
    
    cameraLayout->addWidget(m_cameraCombo);
    cameraLayout->addWidget(m_refreshBtn);
    layout->addLayout(cameraLayout);
    
    // 分辨率选择
    QHBoxLayout *resLayout = new QHBoxLayout();
    resLayout->addWidget(new QLabel(tr("分辨率:")));
    m_resolutionCombo = new QComboBox();
    m_resolutionCombo->addItems({"640x480", "800x600", "1024x768", "1280x720", "1920x1080"});
    m_resolutionCombo->setCurrentText("1280x720");
    resLayout->addWidget(m_resolutionCombo);
    layout->addLayout(resLayout);
    
    // 控制按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("开启摄像头"));
    m_startBtn->setObjectName("startBtn");
    m_stopBtn = new QPushButton(tr("关闭摄像头"));
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setEnabled(false);
    
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    layout->addLayout(btnLayout);
    
    // 状态标签
    m_cameraStatusLabel = new QLabel(tr("摄像头未启动"));
    m_cameraStatusLabel->setStyleSheet("color: #6c757d; font-weight: bold;");
    layout->addWidget(m_cameraStatusLabel);
    
    // 连接信号
    connect(m_refreshBtn, &QPushButton::clicked, this, &CameraTool::onRefreshCamerasClicked);
    connect(m_cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CameraTool::onCameraSelectionChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &CameraTool::onStartCameraClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &CameraTool::onStopCameraClicked);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTool::onResolutionChanged);
}

void CameraTool::setupPreviewArea()
{
    m_previewGroup = new QGroupBox(tr("摄像头预览"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_previewGroup);
    
#ifdef WITH_QT_MULTIMEDIA
    m_videoWidget = new QVideoWidget();
    m_videoWidget->setMinimumSize(400, 300);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_videoWidget);
    
    // 创建预览标签（当没有摄像头时显示）
    m_previewLabel = new QLabel(tr("摄像头预览区域\n请选择并启动摄像头"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("color: #6c757d; font-size: 14px; background-color: #f8f9fa;");
    m_previewLabel->setMinimumSize(400, 300);
    layout->addWidget(m_previewLabel);
    
    // 初始时隐藏视频widget
    m_videoWidget->hide();
#else
    // 创建提示标签
    m_previewLabel = new QLabel(tr("Qt Multimedia模块不可用\n摄像头功能已禁用\n\n请安装Qt Multimedia模块以启用摄像头功能"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("color: #ff6b6b; font-size: 14px; background-color: #f8f9fa; font-weight: bold;");
    m_previewLabel->setMinimumSize(400, 300);
    layout->addWidget(m_previewLabel);
#endif
}

void CameraTool::setupCaptureControls()
{
    m_captureGroup = new QGroupBox(tr("拍照录制"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_captureGroup);
    layout->setSpacing(8);
    
    // 拍照按钮
    m_takePhotoBtn = new QPushButton(tr("拍照"));
    m_takePhotoBtn->setObjectName("takePhotoBtn");
    m_takePhotoBtn->setEnabled(false);
    layout->addWidget(m_takePhotoBtn);
    
    // 录制按钮
    QHBoxLayout *recordLayout = new QHBoxLayout();
    m_startRecordBtn = new QPushButton(tr("开始录制"));
    m_startRecordBtn->setEnabled(false);
    m_stopRecordBtn = new QPushButton(tr("停止录制"));
    m_stopRecordBtn->setEnabled(false);
    
    recordLayout->addWidget(m_startRecordBtn);
    recordLayout->addWidget(m_stopRecordBtn);
    layout->addLayout(recordLayout);
    
    // 录制状态
    m_recordingStatusLabel = new QLabel(tr("未录制"));
    m_recordingStatusLabel->setStyleSheet("color: #6c757d;");
    layout->addWidget(m_recordingStatusLabel);
    
    // 连接信号
    connect(m_takePhotoBtn, &QPushButton::clicked, this, &CameraTool::onTakePhotoClicked);
    connect(m_startRecordBtn, &QPushButton::clicked, this, &CameraTool::onStartRecordingClicked);
    connect(m_stopRecordBtn, &QPushButton::clicked, this, &CameraTool::onStopRecordingClicked);
}

void CameraTool::setupScreenshotControls()
{
    m_screenshotGroup = new QGroupBox(tr("屏幕截图"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_screenshotGroup);
    layout->setSpacing(8);
    
    // 屏幕选择
    QHBoxLayout *screenLayout = new QHBoxLayout();
    screenLayout->addWidget(new QLabel(tr("屏幕:")));
    m_screenCombo = new QComboBox();
    
    // 枚举所有屏幕
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens[i];
        QString screenInfo = QString("屏幕 %1 (%2x%3)")
                           .arg(i + 1)
                           .arg(screen->size().width())
                           .arg(screen->size().height());
        m_screenCombo->addItem(screenInfo, i);
    }
    
    screenLayout->addWidget(m_screenCombo);
    layout->addLayout(screenLayout);
    
    // 截屏按钮
    m_screenshotBtn = new QPushButton(tr("截取屏幕"));
    layout->addWidget(m_screenshotBtn);
    
    connect(m_screenshotBtn, &QPushButton::clicked, this, &CameraTool::onTakeScreenshotClicked);
}

void CameraTool::setupSettingsArea()
{
    m_settingsGroup = new QGroupBox(tr("设置"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_settingsGroup);
    layout->setSpacing(8);
    
    // 保存路径
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("保存路径:")));
    m_savePathLabel = new QLabel();
    m_savePathLabel->setStyleSheet("color: #007bff; text-decoration: underline;");
    m_savePathLabel->setWordWrap(true);
    m_savePathBtn = new QPushButton(tr("浏览"));
    m_savePathBtn->setMaximumWidth(60);
    
    pathLayout->addWidget(m_savePathLabel);
    pathLayout->addWidget(m_savePathBtn);
    layout->addLayout(pathLayout);
    
    // 自动保存
    m_autoSaveCheck = new QCheckBox("自动保存到文件");
    m_autoSaveCheck->setChecked(true);
    layout->addWidget(m_autoSaveCheck);
    
    // 图片质量
    QHBoxLayout *qualityLayout = new QHBoxLayout();
    qualityLayout->addWidget(new QLabel(tr("图片质量:")));
    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(1, 100);
    m_qualitySpin->setValue(90);
    m_qualitySpin->setSuffix("%");
    qualityLayout->addWidget(m_qualitySpin);
    layout->addLayout(qualityLayout);
    
    // 连接信号
    connect(m_savePathBtn, &QPushButton::clicked, this, &CameraTool::onSavePathBrowseClicked);
    connect(m_autoSaveCheck, &QCheckBox::toggled, this, &CameraTool::onAutoSaveToggled);
}

void CameraTool::setupStatusArea()
{
    m_statusGroup = new QGroupBox(tr("状态信息"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_statusGroup);
    
    // 设备统计
    m_deviceCountLabel = new QLabel();
    m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #007bff;");
    layout->addWidget(m_deviceCountLabel);
    
    // 日志区域
    m_logText = new QTextEdit();
    m_logText->setMaximumHeight(150);
    m_logText->setReadOnly(true);
    layout->addWidget(m_logText);
}

void CameraTool::refreshCameraList()
{
#ifdef WITH_QT_MULTIMEDIA
    m_cameraCombo->clear();
    
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    
    if (cameras.isEmpty()) {
        m_cameraCombo->addItem("未找到摄像头设备");
        m_startBtn->setEnabled(false);
        logMessage(tr("未检测到摄像头设备"));
    } else {
        for (const QCameraDevice &cameraDevice : cameras) {
            QString displayName = cameraDevice.description();
            if (displayName.isEmpty()) {
                displayName = QString("摄像头 %1").arg(m_cameraCombo->count() + 1);
            }
            m_cameraCombo->addItem(displayName, QVariant::fromValue(cameraDevice));
        }
        m_startBtn->setEnabled(true);
        logMessage(QString(tr("检测到 %1 个摄像头设备")).arg(cameras.size()));
    }
    
    updateCameraInfo();
#else
    m_cameraCombo->clear();
    m_cameraCombo->addItem(tr("Qt Multimedia不可用"));
    logMessage(tr("Qt Multimedia模块不可用，无法枚举摄像头设备"));
#endif
}

void CameraTool::updateCameraInfo()
{
#ifdef WITH_QT_MULTIMEDIA
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    m_deviceCountLabel->setText(QString("检测到摄像头设备: %1 个").arg(cameras.size()));
#else
    m_deviceCountLabel->setText(tr("Qt Multimedia不可用"));
#endif
}

void CameraTool::updateUI()
{
    // 更新按钮状态
    bool hasCamera = m_cameraCombo->count() > 0 && 
                    m_cameraCombo->currentText() != "未找到摄像头设备";
    
    m_startBtn->setEnabled(hasCamera && !m_cameraActive);
    m_stopBtn->setEnabled(m_cameraActive);
    m_takePhotoBtn->setEnabled(m_cameraActive);
    m_startRecordBtn->setEnabled(m_cameraActive && !m_recording);
    m_stopRecordBtn->setEnabled(m_recording);
    
    // 更新状态标签
    if (m_cameraActive) {
        m_cameraStatusLabel->setText(tr("摄像头运行中"));
        m_cameraStatusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    } else {
        m_cameraStatusLabel->setText(tr("摄像头未启动"));
        m_cameraStatusLabel->setStyleSheet("color: #6c757d; font-weight: bold;");
    }
    
    if (m_recording) {
        m_recordingStatusLabel->setText(tr("正在录制..."));
        m_recordingStatusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
    } else {
        m_recordingStatusLabel->setText(tr("未录制"));
        m_recordingStatusLabel->setStyleSheet("color: #6c757d;");
    }
}

void CameraTool::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);
    m_logText->append(logEntry);
    
    // 限制日志行数
    if (m_logText->document()->lineCount() > 100) {
        QTextCursor cursor = m_logText->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 10);
        cursor.removeSelectedText();
    }
}

QString CameraTool::generateFileName(const QString &prefix) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return QString("%1_%2.jpg").arg(prefix, timestamp);
}

QPixmap CameraTool::takeScreenshot()
{
    int screenIndex = m_screenCombo->currentData().toInt();
    QList<QScreen*> screens = QGuiApplication::screens();
    
    if (screenIndex >= 0 && screenIndex < screens.size()) {
        QScreen *screen = screens[screenIndex];
        return screen->grabWindow(0);
    }
    
    // 如果没有指定屏幕，截取主屏幕
    return QGuiApplication::primaryScreen()->grabWindow(0);
}

void CameraTool::saveSettings()
{
    QSettings settings;
    settings.setValue("CameraTool/savePath", m_savePath);
    settings.setValue("CameraTool/autoSave", m_autoSaveCheck->isChecked());
    settings.setValue("CameraTool/quality", m_qualitySpin->value());
    settings.setValue("CameraTool/resolution", m_resolutionCombo->currentText());
}

void CameraTool::loadSettings()
{
    QSettings settings;
    m_savePath = settings.value("CameraTool/savePath", 
                               QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    
    // 确保保存目录存在
    QDir saveDir(m_savePath);
    if (!saveDir.exists()) {
        saveDir.mkpath(".");
    }
    
    if (m_savePathLabel) {
        m_savePathLabel->setText(QDir::toNativeSeparators(m_savePath));
    }
    
    if (m_autoSaveCheck) {
        m_autoSaveCheck->setChecked(settings.value("CameraTool/autoSave", true).toBool());
    }
    
    if (m_qualitySpin) {
        m_qualitySpin->setValue(settings.value("CameraTool/quality", 90).toInt());
    }
    
    if (m_resolutionCombo) {
        QString resolution = settings.value("CameraTool/resolution", "1280x720").toString();
        int index = m_resolutionCombo->findText(resolution);
        if (index >= 0) {
            m_resolutionCombo->setCurrentIndex(index);
        }
    }
}

// 槽函数实现
void CameraTool::onRefreshCamerasClicked()
{
    logMessage(tr("刷新摄像头设备列表..."));
    refreshCameraList();
}

void CameraTool::onCameraSelectionChanged()
{
    if (m_cameraActive) {
        QMessageBox::information(this, tr("提示"), tr("请先关闭当前摄像头后再切换"));
        return;
    }
    
    int index = m_cameraCombo->currentIndex();
    if (index >= 0) {
        QVariant cameraData = m_cameraCombo->itemData(index);
        if (cameraData.canConvert<QCameraDevice>()) {
            QCameraDevice device = cameraData.value<QCameraDevice>();
            logMessage(QString(tr("选择摄像头: %1")).arg(device.description()));
        }
    }
}

void CameraTool::onStartCameraClicked()
{
    int index = m_cameraCombo->currentIndex();
    if (index < 0) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个摄像头设备"));
        return;
    }
    
    QVariant cameraData = m_cameraCombo->itemData(index);
    if (!cameraData.canConvert<QCameraDevice>()) {
        QMessageBox::warning(this, tr("错误"), tr("无效的摄像头设备"));
        return;
    }
    
    try {
        QCameraDevice device = cameraData.value<QCameraDevice>();
        
        // 创建摄像头
        if (m_camera) {
            delete m_camera;
        }
        
        m_camera = new QCamera(device, this);
        
        // 连接信号
        connect(m_camera, &QCamera::activeChanged, this, &CameraTool::onCameraActiveChanged);
        connect(m_camera, &QCamera::errorOccurred, this, &CameraTool::onCameraErrorOccurred);
        
        // 设置到捕获会话
        m_captureSession->setCamera(m_camera);
        
        // 创建图像捕获
        if (m_imageCapture) {
            delete m_imageCapture;
        }
        
        m_imageCapture = new QImageCapture(this);
        m_captureSession->setImageCapture(m_imageCapture);
        
        // 连接图像捕获信号
        connect(m_imageCapture, &QImageCapture::imageCaptured, this, &CameraTool::onImageCaptured);
        connect(m_imageCapture, &QImageCapture::imageSaved, this, &CameraTool::onImageSaved);
        connect(m_imageCapture, &QImageCapture::errorOccurred, this, &CameraTool::onImageCaptureError);
        
        // 启动摄像头
        m_camera->start();
        
        logMessage(QString(tr("正在启动摄像头: %1")).arg(device.description()));
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("错误"), QString(tr("启动摄像头失败: %1")).arg(e.what()));
        logMessage(QString(tr("启动摄像头失败: %1")).arg(e.what()));
    }
}

void CameraTool::onStopCameraClicked()
{
    if (m_camera && m_camera->isActive()) {
        m_camera->stop();
        logMessage(tr("摄像头已关闭"));
    }
}

void CameraTool::onTakePhotoClicked()
{
    if (!m_imageCapture || !m_camera || !m_camera->isActive()) {
        QMessageBox::warning(this, tr("错误"), tr("摄像头未启动或图像捕获未准备就绪"));
        return;
    }
    
    try {
        if (m_autoSaveCheck->isChecked()) {
            QString fileName = generateFileName("photo");
            QString fullPath = QDir(m_savePath).absoluteFilePath(fileName);
            
            QImageCapture::FileFormat format = QImageCapture::JPEG;
            m_imageCapture->captureToFile(fullPath);
            
            logMessage(QString("拍照保存到: %1").arg(fullPath));
        } else {
            // 只捕获到内存
            m_imageCapture->capture();
            logMessage("拍照完成（未保存到文件）");
        }
        
        m_photoCount++;
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("拍照失败: %1").arg(e.what()));
        logMessage(QString("拍照失败: %1").arg(e.what()));
    }
}

void CameraTool::onStartRecordingClicked()
{
    // 注意：这里需要QMediaRecorder，但为了简化，我们暂时只提供占位实现
    logMessage("录制功能暂未实现（需要QMediaRecorder）");
    QMessageBox::information(this, tr("提示"), tr("录制功能将在后续版本中实现"));
}

void CameraTool::onStopRecordingClicked()
{
    logMessage("停止录制");
}

void CameraTool::onTakeScreenshotClicked()
{
    try {
        QPixmap screenshot = takeScreenshot();
        
        if (m_autoSaveCheck->isChecked()) {
            QString fileName = generateFileName("screenshot");
            QString fullPath = QDir(m_savePath).absoluteFilePath(fileName);
            
            if (screenshot.save(fullPath, "JPEG", m_qualitySpin->value())) {
                logMessage(QString("屏幕截图保存到: %1").arg(fullPath));
                QMessageBox::information(this, "成功", QString("截图已保存到:\n%1").arg(fullPath));
            } else {
                logMessage("保存截图失败");
                QMessageBox::warning(this, tr("错误"), tr("保存截图失败"));
            }
        } else {
            // 复制到剪贴板
            QGuiApplication::clipboard()->setPixmap(screenshot);
            logMessage("屏幕截图已复制到剪贴板");
            QMessageBox::information(this, tr("成功"), tr("截图已复制到剪贴板"));
        }
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("截图失败: %1").arg(e.what()));
        logMessage(QString("截图失败: %1").arg(e.what()));
    }
}

void CameraTool::onSavePathBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择保存目录", m_savePath);
    if (!dir.isEmpty()) {
        m_savePath = dir;
        m_savePathLabel->setText(QDir::toNativeSeparators(m_savePath));
        logMessage(QString("保存路径已更改为: %1").arg(m_savePath));
    }
}

void CameraTool::onResolutionChanged()
{
    // 如果摄像头正在运行，提示需要重启
    if (m_cameraActive) {
        QMessageBox::information(this, tr("提示"), tr("分辨率更改将在下次启动摄像头时生效"));
    }
}

void CameraTool::onAutoSaveToggled(bool enabled)
{
    logMessage(enabled ? "已启用自动保存到文件" : "已禁用自动保存，将复制到剪贴板");
}

// 摄像头状态回调
void CameraTool::onCameraActiveChanged(bool active)
{
    m_cameraActive = active;
    
    if (active) {
        m_videoWidget->show();
        m_previewLabel->hide();
        logMessage("摄像头已启动");
    } else {
        m_videoWidget->hide();
        m_previewLabel->show();
        logMessage("摄像头已关闭");
    }
    
    updateUI();
}

void CameraTool::onCameraErrorOccurred(QCamera::Error error, const QString &errorString)
{
    QString errorMsg = QString("摄像头错误 [%1]: %2").arg(static_cast<int>(error)).arg(errorString);
    logMessage(errorMsg);
    QMessageBox::critical(this, "摄像头错误", errorMsg);
}

void CameraTool::onImageCaptured(int requestId, const QImage &img)
{
    Q_UNUSED(requestId)
    logMessage(QString("图像捕获完成，尺寸: %1x%2").arg(img.width()).arg(img.height()));
}

void CameraTool::onImageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id)
    logMessage(QString("图像已保存: %1").arg(fileName));
}

void CameraTool::onImageCaptureError(int id, QImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id)
    QString errorMsg = QString("图像捕获错误 [%1]: %2").arg(static_cast<int>(error)).arg(errorString);
    logMessage(errorMsg);
    QMessageBox::warning(this, "捕获错误", errorMsg);
}
