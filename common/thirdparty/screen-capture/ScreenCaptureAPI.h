/**
 * @file ScreenCaptureAPI.h
 * @brief ScreenCapture库的公共API接口
 * 
 * 这个头文件定义了ScreenCapture库的公共接口，允许其他Qt应用程序
 * 轻松集成截图功能。
 * 
 * 主要功能：
 * 1. 触发截图操作（全屏或指定区域）
 * 2. 获取截图结果（QImage或保存到文件）
 * 3. 配置截图参数（质量、格式等）
 * 4. 监听截图完成事件
 * 
 * 使用方式：
 * ```cpp
 * #include "ScreenCaptureAPI.h"
 * 
 * // 创建截图实例
 * ScreenCaptureAPI* capture = new ScreenCaptureAPI(this);
 * 
 * // 连接完成信号
 * connect(capture, &ScreenCaptureAPI::captureCompleted,
 *         this, &MyClass::onCaptureCompleted);
 * 
 * // 触发截图
 * capture->startCapture();
 * ```
 * 
 * @author ScreenCapture Team
 * @date 2025
 * @version 1.0
 */

#ifndef SCREENCAPTUREAPI_H
#define SCREENCAPTUREAPI_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QRect>
#include <QPixmap>

class WinFull;
class WinPin;

/**
 * @brief ScreenCapture库的主要API类
 * 
 * 这个类提供了一个简洁的接口来集成截图功能到其他Qt应用程序中。
 * 它封装了复杂的窗口管理和用户交互逻辑，对外提供简单的API。
 */
class ScreenCaptureAPI : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 截图模式枚举
     */
    enum class CaptureMode {
        FullScreen,    ///< 全屏截图模式
        SelectArea,    ///< 选择区域模式（默认）
        FixedArea      ///< 固定区域模式
    };

    /**
     * @brief 截图结果枚举
     */
    enum class CaptureResult {
        Success,       ///< 截图成功
        Cancelled,     ///< 用户取消
        Error,         ///< 发生错误
        Timeout        ///< 超时
    };

    /**
     * @brief 截图配置结构体
     */
    struct CaptureConfig {
        CaptureMode mode = CaptureMode::SelectArea;  ///< 截图模式
        QRect fixedArea;                             ///< 固定区域（仅在FixedArea模式下有效）
        bool includeDecorations = true;             ///< 是否包含窗口装饰
        bool hideCursor = false;                     ///< 是否隐藏鼠标光标
        int quality = 100;                           ///< 图片质量（1-100）
        QString format = "PNG";                      ///< 图片格式
    };

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit ScreenCaptureAPI(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ScreenCaptureAPI();

    /**
     * @brief 开始截图操作
     * @param config 截图配置（可选）
     * @return 是否成功启动截图
     * 
     * 这个方法会显示截图界面，用户可以选择区域并进行编辑。
     * 完成后会发射captureCompleted信号。
     */
    bool startCapture(const CaptureConfig& config = CaptureConfig());

    /**
     * @brief 开始截图操作（异步，带回调）
     * @param callback 完成回调函数
     * @param config 截图配置（可选）
     * @return 是否成功启动截图
     */
    bool startCaptureAsync(std::function<void(CaptureResult, const QImage&)> callback,
                          const CaptureConfig& config = CaptureConfig());

    /**
     * @brief 取消当前截图操作
     */
    void cancelCapture();

    /**
     * @brief 检查是否正在截图
     * @return 是否正在截图
     */
    bool isCapturing() const;

    /**
     * @brief 获取最后一次截图的结果
     * @return 截图图像
     */
    QImage getLastCaptureImage() const;

    /**
     * @brief 保存最后一次截图到文件
     * @param filePath 文件路径
     * @return 是否保存成功
     */
    bool saveLastCapture(const QString& filePath) const;

    /**
     * @brief 设置默认配置
     * @param config 默认配置
     */
    void setDefaultConfig(const CaptureConfig& config);

    /**
     * @brief 获取当前配置
     * @return 当前配置
     */
    CaptureConfig getCurrentConfig() const;

public slots:
    /**
     * @brief 快速截图（使用默认配置）
     */
    void quickCapture();

    /**
     * @brief 截图到剪贴板
     */
    void captureToClipboard();

signals:
    /**
     * @brief 截图完成信号
     * @param result 截图结果
     * @param image 截图图像（成功时有效）
     */
    void captureCompleted(CaptureResult result, const QImage& image);

    /**
     * @brief 截图开始信号
     */
    void captureStarted();

    /**
     * @brief 截图取消信号
     */
    void captureCancelled();

    /**
     * @brief 截图错误信号
     * @param errorMessage 错误信息
     */
    void captureError(const QString& errorMessage);

private slots:
    /**
     * @brief 处理截图窗口关闭
     */
    void onCaptureWindowClosed();

    /**
     * @brief 处理Pin窗口创建
     */
    void onPinWindowCreated();

private:
    /**
     * @brief 初始化截图环境
     */
    void initializeCapture();

    /**
     * @brief 清理截图资源
     */
    void cleanupCapture();

    /**
     * @brief 创建截图窗口
     * @return 是否创建成功
     */
    bool createCaptureWindow();

    /**
     * @brief 开始全屏截图
     * @return 是否成功启动
     */
    bool startFullScreenCapture();

    /**
     * @brief 开始选择区域截图
     * @return 是否成功启动
     */
    bool startSelectAreaCapture();

    /**
     * @brief 开始固定区域截图
     * @param area 截图区域
     * @return 是否成功启动
     */
    bool startFixedAreaCapture(const QRect& area);

    /**
     * @brief 处理截图完成
     * @param image 截图图像
     */
    void handleCaptureCompleted(const QImage& image);

private:
    // 私有成员变量
    WinFull* m_captureWindow;                                           ///< 截图窗口
    WinPin* m_pinWindow;                                               ///< Pin窗口
    CaptureConfig m_config;                                            ///< 当前配置
    QImage m_lastCaptureImage;                                         ///< 最后一次截图
    bool m_isCapturing;                                                ///< 是否正在截图
    std::function<void(CaptureResult, const QImage&)> m_callback;      ///< 异步回调

    // 禁用拷贝构造和赋值
    ScreenCaptureAPI(const ScreenCaptureAPI&) = delete;
    ScreenCaptureAPI& operator=(const ScreenCaptureAPI&) = delete;
};

// 便利函数，用于简单的截图操作
namespace ScreenCaptureUtils {
    /**
     * @brief 快速全屏截图
     * @return 截图图像
     */
    QImage quickFullScreenCapture();

    /**
     * @brief 截图指定窗口
     * @param windowHandle 窗口句柄
     * @return 截图图像
     */
    QImage captureWindow(WId windowHandle);

    /**
     * @brief 截图指定区域
     * @param rect 截图区域
     * @return 截图图像
     */
    QImage captureArea(const QRect& rect);
}

#endif // SCREENCAPTUREAPI_H
