#include "mainwindowtabwidget.h"

MainWindowTabWidget::MainWindowTabWidget(QWidget* parent)
    : QTabWidget(parent)
      , m_customTabBar(new CustomTabBar(this)) {
    // 设置自定义TabBar
    setTabBar(m_customTabBar);

    // 启用关闭按钮
    setTabsClosable(true);

    // 设置基本样式
    setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #ced4da;
            background-color: #ffffff;
            border-radius: 0px;
            margin: 0px;
            padding: 0px;
        }
    )");
}
