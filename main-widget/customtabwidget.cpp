#include "customtabwidget.h"

MainWindowTabWidget::MainWindowTabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_customTabBar(new CustomTabBar(this))
{
    // 在构造函数中设置自定义的 TabBar
    setTabBar(m_customTabBar);
    
    // 设置基本样式
    setStyleSheet(
        "QTabWidget::pane {"
        "    border: 1px solid #ced4da;"
        "    background-color: #ffffff;"
        "    border-radius: 0px;"
        "    margin: 0px;"
        "    padding: 0px;"
        "}"
    );
}
