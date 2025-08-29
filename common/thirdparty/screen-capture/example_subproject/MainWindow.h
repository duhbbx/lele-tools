/**
 * @file example_subproject/MainWindow.h
 * @brief 主窗口类定义
 * 
 * 主窗口类，演示ScreenCapture库的各种使用方式：
 * - F1快捷键截图
 * - 菜单栏截图选项
 * - 按钮触发截图
 * - 不同的截图模式
 * - 截图结果处理
 * 
 * @author ScreenCapture Team
 * @date 2025
 */

#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QShortcut>
#include <QTimer>
#include <QLoggingCategory>

// 包含ScreenCapture API
#include "ScreenCaptureAPI.h"

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QPushButton;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QSlider;
QT_END_NAMESPACE

// 声明日志分类
Q_DECLARE_LOGGING_CATEGORY(lcMainWindow)

/**
 * @brief 主窗口类
 * 
 * 演示ScreenCapture库集成的主窗口，提供了完整的用户界面
 * 来测试和展示截图功能的各个方面。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // ScreenCapture API 信号处理
    void onCaptureCompleted(ScreenCaptureAPI::CaptureResult result, const QImage& image);
    void onCaptureStarted();
    void onCaptureCancelled();
    void onCaptureError(const QString& errorMessage);
    
    // 菜单和工具栏动作
    void onQuickCapture();
    void onFullScreenCapture();
    void onSelectAreaCapture();
    void onFixedAreaCapture();
    void onCaptureToClipboard();
    void onCancelCapture();
    
    // 设置相关
    void onSettingsChanged();
    void onSaveSettings();
    void onLoadSettings();
    void onResetSettings();
    
    // 文件操作
    void onSaveLastCapture();
    void onOpenScreenshotFolder();
    
    // 帮助和关于
    void onShowHelp();
    void onAbout();
    void onAboutQt();
    
    // UI更新
    void updateUI();
    void updateStatusBar();
    void clearLog();

private:
    // UI初始化方法
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupShortcuts();
    void connectSignals();
    
    // ScreenCapture设置
    void setupScreenCapture();
    void applyScreenCaptureConfig();
    ScreenCaptureAPI::CaptureConfig getCurrentConfig() const;
    
    // 工具方法
    void addLogMessage(const QString& message, const QString& category = "INFO");
    void displayImage(const QImage& image);
    void saveImageToFile(const QImage& image);
    QString generateFileName() const;
    void showImageInfo(const QImage& image);
    
    // 私有成员变量
    
    // ScreenCapture相关
    ScreenCaptureAPI* m_captureAPI;
    QImage m_lastCaptureImage;
    
    // 快捷键
    QShortcut* m_f1Shortcut;
    QShortcut* m_f2Shortcut;
    QShortcut* m_escShortcut;
    
    // 菜单和工具栏
    QMenuBar* m_menuBar;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    
    // 菜单动作
    QAction* m_quickCaptureAction;
    QAction* m_fullScreenAction;
    QAction* m_selectAreaAction;
    QAction* m_fixedAreaAction;
    QAction* m_clipboardAction;
    QAction* m_cancelAction;
    QAction* m_saveAction;
    QAction* m_openFolderAction;
    QAction* m_exitAction;
    
    // 设置动作
    QAction* m_saveSettingsAction;
    QAction* m_loadSettingsAction;
    QAction* m_resetSettingsAction;
    
    // 帮助动作
    QAction* m_helpAction;
    QAction* m_aboutAction;
    QAction* m_aboutQtAction;
    
    // 中央部件
    QWidget* m_centralWidget;
    
    // 左侧：图片显示区域
    QLabel* m_imageLabel;
    QLabel* m_imageInfoLabel;
    
    // 右侧：控制面板
    QWidget* m_controlPanel;
    
    // 设置控件
    QComboBox* m_modeComboBox;
    QSpinBox* m_qualitySpinBox;
    QComboBox* m_formatComboBox;
    QSpinBox* m_timeoutSpinBox;
    QCheckBox* m_hideCursorCheckBox;
    QCheckBox* m_includeDecorationsCheckBox;
    
    // 固定区域设置
    QSpinBox* m_fixedXSpinBox;
    QSpinBox* m_fixedYSpinBox;
    QSpinBox* m_fixedWidthSpinBox;
    QSpinBox* m_fixedHeightSpinBox;
    
    // 操作按钮
    QPushButton* m_quickCaptureButton;
    QPushButton* m_fullScreenButton;
    QPushButton* m_selectAreaButton;
    QPushButton* m_fixedAreaButton;
    QPushButton* m_clipboardButton;
    QPushButton* m_cancelButton;
    QPushButton* m_saveButton;
    QPushButton* m_clearLogButton;
    
    // 底部：日志区域
    QTextEdit* m_logTextEdit;
    
    // 状态栏控件
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_captureCountLabel;
    
    // 统计信息
    int m_captureCount;
    int m_successCount;
    int m_cancelledCount;
    int m_errorCount;
    
    // 定时器
    QTimer* m_statusUpdateTimer;
};

#endif // MAINWINDOW_H
