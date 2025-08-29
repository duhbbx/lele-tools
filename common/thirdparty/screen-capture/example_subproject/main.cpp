/**
 * @file example_subproject/main.cpp
 * @brief ScreenCapture子项目集成示例
 * 
 * 这个示例展示了如何将ScreenCapture作为CMake子项目集成到现有的Qt应用程序中。
 * 演示了F1快捷键截图、菜单触发截图、以及各种截图模式的使用。
 * 
 * @author ScreenCapture Team
 * @date 2025
 */

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>

#include "MainWindow.h"

// 定义日志分类
Q_LOGGING_CATEGORY(lcMain, "example.main")

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("ScreenCapture集成示例");
    app.setApplicationDisplayName("ScreenCapture Integration Example");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ScreenCapture Team");
    app.setOrganizationDomain("screencapture.example.com");
    
    // 设置应用程序图标
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));
    
    // 日志信息
    qCInfo(lcMain) << "=== ScreenCapture集成示例启动 ===";
    qCInfo(lcMain) << "应用程序名称:" << app.applicationName();
    qCInfo(lcMain) << "应用程序版本:" << app.applicationVersion();
    qCInfo(lcMain) << "Qt版本:" << QT_VERSION_STR;
    qCInfo(lcMain) << "工作目录:" << QDir::currentPath();
    
    // 确保截图保存目录存在
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString screenshotDir = picturesPath + "/ScreenCapture";
    QDir().mkpath(screenshotDir);
    qCInfo(lcMain) << "截图保存目录:" << screenshotDir;
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    qCInfo(lcMain) << "主窗口已显示，应用程序就绪";
    qCInfo(lcMain) << "按F1键开始截图，或使用菜单/按钮操作";
    
    // 运行事件循环
    int result = app.exec();
    
    qCInfo(lcMain) << "应用程序退出，返回码:" << result;
    qCInfo(lcMain) << "=== ScreenCapture集成示例结束 ===";
    
    return result;
}
