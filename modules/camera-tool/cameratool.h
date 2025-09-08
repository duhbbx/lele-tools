#ifndef CAMERATOOL_H
#define CAMERATOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QTextEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoWidget>
#include <QImageCapture>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QScreen>
#include <QPixmap>
#include <QFileDialog>
#include <QSettings>
#include <QClipboard>
#include <QGuiApplication>

#include "../../common/dynamicobjectbase.h"

class CameraTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit CameraTool(QWidget *parent = nullptr);
    ~CameraTool();

private slots:
    void onRefreshCamerasClicked();
    void onCameraSelectionChanged();
    void onStartCameraClicked();
    void onStopCameraClicked();
    void onTakePhotoClicked();
    void onStartRecordingClicked();
    void onStopRecordingClicked();
    void onTakeScreenshotClicked();
    void onSavePathBrowseClicked();
    void onResolutionChanged();
    void onAutoSaveToggled(bool enabled);
    
    // 摄像头状态回调
    void onCameraActiveChanged(bool active);
    void onCameraErrorOccurred(QCamera::Error error, const QString &errorString);
    void onImageCaptured(int requestId, const QImage &img);
    void onImageSaved(int id, const QString &fileName);
    void onImageCaptureError(int id, QImageCapture::Error error, const QString &errorString);

private:
    void setupUI();
    void setupCameraControls();
    void setupPreviewArea();
    void setupCaptureControls();
    void setupScreenshotControls();
    void setupSettingsArea();
    void setupStatusArea();
    
    void refreshCameraList();
    void updateCameraInfo();
    void updateUI();
    void logMessage(const QString &message);
    void saveSettings();
    void loadSettings();
    
    QString generateFileName(const QString &prefix = "camera") const;
    QPixmap takeScreenshot();
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    
    // 摄像头控制区域
    QGroupBox *m_cameraGroup;
    QComboBox *m_cameraCombo;
    QPushButton *m_refreshBtn;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QLabel *m_cameraStatusLabel;
    QComboBox *m_resolutionCombo;
    
    // 预览区域
    QGroupBox *m_previewGroup;
    QVideoWidget *m_videoWidget;
    QLabel *m_previewLabel;
    
    // 拍照控制区域
    QGroupBox *m_captureGroup;
    QPushButton *m_takePhotoBtn;
    QPushButton *m_startRecordBtn;
    QPushButton *m_stopRecordBtn;
    QLabel *m_recordingStatusLabel;
    
    // 截屏控制区域
    QGroupBox *m_screenshotGroup;
    QPushButton *m_screenshotBtn;
    QComboBox *m_screenCombo;
    
    // 设置区域
    QGroupBox *m_settingsGroup;
    QLabel *m_savePathLabel;
    QPushButton *m_savePathBtn;
    QCheckBox *m_autoSaveCheck;
    QSpinBox *m_qualitySpin;
    
    // 状态区域
    QGroupBox *m_statusGroup;
    QTextEdit *m_logText;
    QLabel *m_deviceCountLabel;
    
    // 摄像头相关对象
    QCamera *m_camera;
    QMediaCaptureSession *m_captureSession;
    QImageCapture *m_imageCapture;
    
    // 设置和状态
    QString m_savePath;
    bool m_cameraActive;
    bool m_recording;
    int m_photoCount;
    QTimer *m_updateTimer;
};

#endif // CAMERATOOL_H
