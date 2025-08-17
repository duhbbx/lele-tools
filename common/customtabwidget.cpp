#include "customtabwidget.h"
#include <QIcon>
#include <QDebug>

// HoverTabBar 实现
HoverTabBar::HoverTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_hoveredTab(-1)
    , m_updateTimer(new QTimer(this))
{
    setMouseTracking(true);
    setTabsClosable(true);
    
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(50);
    connect(m_updateTimer, &QTimer::timeout, this, &HoverTabBar::updateCloseButtons);
}

void HoverTabBar::enterEvent(QEnterEvent *event)
{
    QTabBar::enterEvent(event);
    m_updateTimer->start();
}

void HoverTabBar::leaveEvent(QEvent *event)
{
    QTabBar::leaveEvent(event);
    m_hoveredTab = -1;
    m_updateTimer->start();
}

void HoverTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    
    int newHoveredTab = tabAt(event->pos());
    if (newHoveredTab != m_hoveredTab) {
        m_hoveredTab = newHoveredTab;
        m_updateTimer->start();
    }
}

void HoverTabBar::tabLayoutChange()
{
    QTabBar::tabLayoutChange();
    m_updateTimer->start();
}

void HoverTabBar::updateCloseButtons()
{
    hideAllCloseButtons();
    
    if (m_hoveredTab >= 0 && m_hoveredTab < count()) {
        showCloseButtonForTab(m_hoveredTab);
    }
}

void HoverTabBar::showCloseButtonForTab(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    
    // 查找对应的关闭按钮
    QList<QToolButton*> buttons = findChildren<QToolButton*>();
    for (QToolButton *button : buttons) {
        if (button->parent() == this) {
            QRect buttonGeometry = button->geometry();
            QRect tabGeometry = tabRect(index);
            
            // 检查按钮是否在对应的标签页区域内
            if (tabGeometry.intersects(buttonGeometry)) {
                button->setIcon(QIcon(":/resources/close.svg"));
                break;
            }
        }
    }
}

void HoverTabBar::hideAllCloseButtons()
{
    QList<QToolButton*> buttons = findChildren<QToolButton*>();
    for (QToolButton *button : buttons) {
        if (button->parent() == this) {
            button->setIcon(QIcon());
        }
    }
}

int HoverTabBar::tabAt(const QPoint &pos) const
{
    for (int i = 0; i < count(); ++i) {
        if (tabRect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

// CustomTabWidget 实现
CustomTabWidget::CustomTabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_customTabBar(new HoverTabBar(this))
{
    setTabBar(m_customTabBar);
    
    // 设置基本样式
    setStyleSheet(R"(
        QTabWidget::pane {
            border: 2px solid #dee2e6;
            border-radius: 8px;
        }
        QTabBar::tab {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            margin: 2px;
            border-radius: 4px;
            background-color: #f8f9fa;
        }
        QTabBar::tab:selected {
            background-color: #007bff;
            color: white;
        }
        QTabBar::tab:hover {
            background-color: #e9ecef;
        }
        QTabBar::close-button {
            subcontrol-position: right;
            border: none;
            width: 16px;
            height: 16px;
            margin: 1px;
            background-color: transparent;
            border-radius: 8px;
            image: none;
        }
        QTabBar::close-button:hover {
            background-color: rgba(220, 53, 69, 0.1);
            border: 1px solid #dc3545;
        }
        QTabBar::close-button:pressed {
            background-color: rgba(200, 35, 51, 0.2);
            border: 1px solid #c82333;
        }
    )");
}
