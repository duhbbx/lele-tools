/**
 * @file ScreenCaptureAPI.cpp
 * @brief ScreenCapture库的公共API实现
 * 
 * 实现了ScreenCaptureAPI类的所有方法，提供了一个简洁的接口
 * 来集成截图功能到其他Qt应用程序中。
 * 
 * @author ScreenCapture Team
 * @date 2025
 * @version 1.0
 */

#include "ScreenCaptureAPI.h"
#include "Win/WinFull.h"
#include "Win/WinPin.h"
#include "App/Util.h"
#include "App/Logger.h"

#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QClipboard>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// 获取虚拟屏幕区域的帮助函数
static QRect getVirtualScreenRect()
{
#ifdef Q_OS_WIN
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return QRect(x, y, w, h);
#else
    // 对于非Windows平台，使用主屏幕
    QScreen* screen = QApplication::primaryScreen();
    return screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
#endif
}

ScreenCaptureAPI::ScreenCaptureAPI(QObject* parent)
    : QObject(parent)
    , m_captureWindow(nullptr)
    , m_pinWindow(nullptr)
    , m_isCapturing(false)
    , m_timeoutTimer(nullptr)
{
    LOG_INFO("ScreenCaptureAPI", "=== ScreenCaptureAPI构造函数开始 ===");

    // 初始化默认配置
    m_config = CaptureConfig();

    // 初始化GDI缓存以提高截图性能
    Util::initializeGDICache();

    // 创建超时定时器
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        LOG_WARNING("ScreenCaptureAPI", "截图操作超时");
        cancelCapture();
        emit captureCompleted(CaptureResult::Timeout, QImage());
    });

    LOG_INFO("ScreenCaptureAPI", "=== ScreenCaptureAPI构造函数完成 ===");
}

ScreenCaptureAPI::~ScreenCaptureAPI()
{
    LOG_INFO("ScreenCaptureAPI", "=== ScreenCaptureAPI析构函数开始 ===");

    // 清理资源
    cleanupCapture();

    // 清理GDI缓存
    Util::cleanupGDICache();

    LOG_INFO("ScreenCaptureAPI", "=== ScreenCaptureAPI析构函数完成 ===");
}

bool ScreenCaptureAPI::startCapture(const CaptureConfig& config)
{
    LOG_INFO("ScreenCaptureAPI", "=== 开始截图操作 ===");
    
    if (m_isCapturing) {
        LOG_WARNING("ScreenCaptureAPI", "已经在截图中，忽略新的截图请求");
        return false;
    }
    
    // 更新配置
    m_config = config;
    
    // 根据模式处理
    switch (config.mode) {
    case CaptureMode::FullScreen:
        return startFullScreenCapture();
        
    case CaptureMode::SelectArea:
        return startSelectAreaCapture();
        
    case CaptureMode::FixedArea:
        return startFixedAreaCapture(config.fixedArea);
        
    default:
        LOG_ERROR("ScreenCaptureAPI", "未知的截图模式");
        return false;
    }
}

bool ScreenCaptureAPI::startCaptureAsync(std::function<void(CaptureResult, const QImage&)> callback, const CaptureConfig& config)
{
    m_callback = callback;
    return startCapture(config);
}

void ScreenCaptureAPI::cancelCapture()
{
    LOG_INFO("ScreenCaptureAPI", "取消截图操作");
    
    if (!m_isCapturing) {
        return;
    }
    
    cleanupCapture();
    
    emit captureCancelled();
    
    if (m_callback) {
        m_callback(CaptureResult::Cancelled, QImage());
        m_callback = nullptr;
    }
}

bool ScreenCaptureAPI::isCapturing() const
{
    return m_isCapturing;
}

QImage ScreenCaptureAPI::getLastCaptureImage() const
{
    return m_lastCaptureImage;
}

bool ScreenCaptureAPI::saveLastCapture(const QString& filePath) const
{
    if (m_lastCaptureImage.isNull()) {
        LOG_ERROR("ScreenCaptureAPI", "没有可保存的截图");
        return false;
    }
    
    // 确保目录存在
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR("ScreenCaptureAPI", QString("无法创建目录: %1").arg(dir.absolutePath()));
            return false;
        }
    }
    
    // 保存图片
    bool success = m_lastCaptureImage.save(filePath, m_config.format.toLocal8Bit().data(), m_config.quality);
    
    if (success) {
        LOG_INFO("ScreenCaptureAPI", QString("截图已保存到: %1").arg(filePath));
    } else {
        LOG_ERROR("ScreenCaptureAPI", QString("保存截图失败: %1").arg(filePath));
    }
    
    return success;
}

void ScreenCaptureAPI::setDefaultConfig(const CaptureConfig& config)
{
    m_config = config;
    LOG_INFO("ScreenCaptureAPI", "已更新默认配置");
}

ScreenCaptureAPI::CaptureConfig ScreenCaptureAPI::getCurrentConfig() const
{
    return m_config;
}

void ScreenCaptureAPI::quickCapture()
{
    CaptureConfig quickConfig;
    quickConfig.mode = CaptureMode::SelectArea;
    startCapture(quickConfig);
}

void ScreenCaptureAPI::captureToClipboard()
{
    // 设置回调，将截图复制到剪贴板
    auto callback = [](CaptureResult result, const QImage& image) {
        if (result == CaptureResult::Success && !image.isNull()) {
            QApplication::clipboard()->setImage(image);
            LOG_INFO("ScreenCaptureAPI", "截图已复制到剪贴板");
        }
    };
    
    startCaptureAsync(callback);
}

void ScreenCaptureAPI::onCaptureWindowClosed()
{
    LOG_INFO("ScreenCaptureAPI", "截图窗口已关闭");
    
    if (m_captureWindow) {
        // 获取截图结果
        QImage result = m_captureWindow->getTargetImg();
        
        if (!result.isNull()) {
            m_lastCaptureImage = result;
            handleCaptureCompleted(result);
        } else {
            // 用户取消了截图
            emit captureCancelled();
            if (m_callback) {
                m_callback(CaptureResult::Cancelled, QImage());
                m_callback = nullptr;
            }
        }
    }
    
    cleanupCapture();
}

void ScreenCaptureAPI::onPinWindowCreated()
{
    LOG_INFO("ScreenCaptureAPI", "Pin窗口已创建");
    // Pin窗口创建后，截图操作基本完成
    // 但用户可能还在编辑，所以不立即清理
}

void ScreenCaptureAPI::initializeCapture()
{
    LOG_DEBUG("ScreenCaptureAPI", "初始化截图环境");
    
    m_isCapturing = true;
    
    // 启动超时定时器
    if (m_config.timeoutMs > 0) {
        m_timeoutTimer->start(m_config.timeoutMs);
    }
    
    emit captureStarted();
}

void ScreenCaptureAPI::cleanupCapture()
{
    LOG_DEBUG("ScreenCaptureAPI", "清理截图资源");
    
    m_isCapturing = false;
    
    // 停止超时定时器
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
    
    // 清理截图窗口
    if (m_captureWindow) {
        m_captureWindow->close();
        m_captureWindow->deleteLater();
        m_captureWindow = nullptr;
    }
    
    // 注意：Pin窗口不在这里清理，因为用户可能还在使用
}

bool ScreenCaptureAPI::createCaptureWindow()
{
    LOG_DEBUG("ScreenCaptureAPI", "创建截图窗口");
    
    try {
        // 创建全屏截图窗口（WinFull构造函数会自动获取屏幕截图）
        m_captureWindow = new WinFull();
        
        // 连接信号
        connect(m_captureWindow, &WinFull::captureFinished, this, &ScreenCaptureAPI::onCaptureWindowClosed);
        
        // 显示窗口
        m_captureWindow->show();
        
        LOG_INFO("ScreenCaptureAPI", "截图窗口创建成功");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("ScreenCaptureAPI", QString("创建截图窗口时发生异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("ScreenCaptureAPI", "创建截图窗口时发生未知异常");
        return false;
    }
}

bool ScreenCaptureAPI::startFullScreenCapture()
{
    LOG_INFO("ScreenCaptureAPI", "开始全屏截图");
    
    // 全屏截图直接获取屏幕图像
    QRect screenRect = getVirtualScreenRect();
    QImage screenImage = Util::printScreen(screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
    if (screenImage.isNull()) {
        LOG_ERROR("ScreenCaptureAPI", "获取全屏截图失败");
        emit captureError("无法获取屏幕截图");
        return false;
    }
    
    m_lastCaptureImage = screenImage;
    handleCaptureCompleted(screenImage);
    return true;
}

bool ScreenCaptureAPI::startSelectAreaCapture()
{
    LOG_INFO("ScreenCaptureAPI", "开始选择区域截图");
    
    initializeCapture();
    
    if (!createCaptureWindow()) {
        cleanupCapture();
        emit captureError("无法创建截图窗口");
        return false;
    }
    
    return true;
}

bool ScreenCaptureAPI::startFixedAreaCapture(const QRect& area)
{
    LOG_INFO("ScreenCaptureAPI", QString("开始固定区域截图: (%1,%2,%3,%4)")
             .arg(area.x()).arg(area.y()).arg(area.width()).arg(area.height()));
    
    // 获取屏幕截图
    QRect screenRect = getVirtualScreenRect();
    QImage screenImage = Util::printScreen(screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
    if (screenImage.isNull()) {
        LOG_ERROR("ScreenCaptureAPI", "获取屏幕截图失败");
        emit captureError("无法获取屏幕截图");
        return false;
    }
    
    // 裁剪指定区域
    QRect validArea = area.intersected(screenImage.rect());
    if (validArea.isEmpty()) {
        LOG_ERROR("ScreenCaptureAPI", "指定区域无效");
        emit captureError("指定的截图区域无效");
        return false;
    }
    
    QImage croppedImage = screenImage.copy(validArea);
    m_lastCaptureImage = croppedImage;
    handleCaptureCompleted(croppedImage);
    return true;
}

void ScreenCaptureAPI::handleCaptureCompleted(const QImage& image)
{
    LOG_INFO("ScreenCaptureAPI", "截图完成");
    
    emit captureCompleted(CaptureResult::Success, image);
    
    if (m_callback) {
        m_callback(CaptureResult::Success, image);
        m_callback = nullptr;
    }
    
    cleanupCapture();
}

// =============================================================================
// 便利函数实现
// =============================================================================

namespace ScreenCaptureUtils {

QImage quickFullScreenCapture()
{
    LOG_DEBUG("ScreenCaptureUtils", "快速全屏截图");
    QRect screenRect = getVirtualScreenRect();
    return Util::printScreen(screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
}

QImage captureWindow(WId windowHandle)
{
    LOG_DEBUG("ScreenCaptureUtils", QString("截图窗口: %1").arg(windowHandle));
    
    // 这里需要实现窗口截图逻辑
    // 可以使用Windows API或Qt的方法
    // 暂时返回全屏截图
    QRect screenRect = getVirtualScreenRect();
    return Util::printScreen(screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
}

QImage captureArea(const QRect& rect)
{
    LOG_DEBUG("ScreenCaptureUtils", QString("截图区域: (%1,%2,%3,%4)")
              .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    
    QRect screenRect = getVirtualScreenRect();
    QImage screenImage = Util::printScreen(screenRect.x(), screenRect.y(), screenRect.width(), screenRect.height());
    if (screenImage.isNull()) {
        return QImage();
    }
    
    QRect validArea = rect.intersected(screenImage.rect());
    if (validArea.isEmpty()) {
        return QImage();
    }
    
    return screenImage.copy(validArea);
}

} // namespace ScreenCaptureUtils
