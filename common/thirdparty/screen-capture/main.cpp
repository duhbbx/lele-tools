#include <QtWidgets/QApplication>
#include "App/App.h"
#include "App/Logger.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    
    LOG_INFO(MODULE_MAIN, "========== ScreenCapture 应用程序启动 ==========");
    LOG_INFO(MODULE_MAIN, QString("命令行参数数量: %1").arg(argc));
    for (int i = 0; i < argc; i++) {
        LOG_DEBUG(MODULE_MAIN, QString("参数[%1]: %2").arg(i).arg(argv[i]));
    }
    
    LOG_INFO(MODULE_MAIN, "初始化应用程序");
    App::init();
    
    LOG_INFO(MODULE_MAIN, "开始事件循环");
    const auto resut = a.exec();
    
    LOG_INFO(MODULE_MAIN, QString("事件循环结束，返回值: %1").arg(resut));
    App::dispose();
    
    LOG_INFO(MODULE_MAIN, "========== ScreenCapture 应用程序结束 ==========");
    return resut;
}
