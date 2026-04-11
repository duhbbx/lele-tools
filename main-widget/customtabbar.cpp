#include "customtabbar.h"
#include <QDebug>

CustomTabBar::CustomTabBar(QWidget *parent)
    : QTabBar(parent), m_hoveredTab(-1), m_hoveredCloseButton(-1), m_pressedCloseButton(-1)
{
    setMouseTracking(true);  // 必须开启鼠标追踪
    setElideMode(Qt::ElideRight); // 标签名过长时右侧省略
    
    // 加载关闭按钮图标
    m_closeNormalIcon = QPixmap(":/resources/close.svg");
    m_closeHoverIcon = QPixmap(":/resources/close.svg");
    
    // 设置基础样式（不包含关闭按钮，因为我们自己绘制）
    setStyleSheet(R"(
        QTabBar::tab {
            background-color: #f8f9fa;
            color: #495057;
            padding: 8px 16px;
            border: 1px solid #dee2e6;
            border-bottom: none;
            border-top: none;
            margin-right: -1px;
            min-width: 80px;
            max-width: 200px;
        }

        QTabBar::tab:last {
            margin-right: 0px;
        }

        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #212529;
        }
        QTabBar::tab:hover {
            background-color: #e9ecef;
        }
        QTabBar::close-button {
            image: none;
        }
    )");
}

void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());
    int hoveredCloseButton = -1;
    
    // 检查是否悬停在关闭按钮上
    for (auto it = m_closeRects.constBegin(); it != m_closeRects.constEnd(); ++it) {
        if (it.value().contains(event->pos())) {
            hoveredCloseButton = it.key();
            break;
        }
    }
    
    bool needUpdate = false;
    
    if (index != m_hoveredTab) {
        m_hoveredTab = index;
        needUpdate = true;
    }
    
    if (hoveredCloseButton != m_hoveredCloseButton) {
        m_hoveredCloseButton = hoveredCloseButton;
        needUpdate = true;
    }
    
    if (needUpdate) {
        update(); // 刷新绘制
    }
    
    QTabBar::mouseMoveEvent(event);
}

void CustomTabBar::leaveEvent(QEvent *event)
{
    m_hoveredTab = -1;
    m_hoveredCloseButton = -1;
    m_pressedCloseButton = -1;
    update();
    QTabBar::leaveEvent(event);
}

void CustomTabBar::paintEvent(QPaintEvent *event)
{
    QStylePainter painter(this);
    QStyleOptionTab opt;

    for (int i = 0; i < count(); ++i) {
        initStyleOption(&opt, i);
        painter.drawControl(QStyle::CE_TabBarTabShape, opt);
        painter.drawControl(QStyle::CE_TabBarTabLabel, opt);

        // 绘制关闭按钮
        if (i == m_hoveredTab) {
            QRect tabRectCurrent = tabRect(i);
            
            // 计算关闭按钮位置：距离右边8px，上下居中
            int buttonSize = 14;
            int rightMargin = 8;
            
            QRect closeRect(
                tabRectCurrent.right() - buttonSize - rightMargin,  // x坐标
                tabRectCurrent.y() + (tabRectCurrent.height() - buttonSize) / 2,  // y坐标，居中
                buttonSize,  // 宽度
                buttonSize   // 高度
            );
            
            // 确保图标已加载
            if (m_closeHoverIcon.isNull()) {
                m_closeHoverIcon = QPixmap(":/resources/close.svg");
            }
            
            // 绘制关闭按钮背景（根据状态）
            if (i == m_pressedCloseButton) {
                // 按下状态：深红色背景
                painter.fillRect(closeRect, QColor(200, 35, 51, 51)); // rgba(200, 35, 51, 0.2)
                painter.setPen(QPen(QColor("#c82333"), 1));
                painter.drawRoundedRect(closeRect, 4, 4);
            } else if (i == m_hoveredCloseButton) {
                // 悬停状态：浅红色背景
                painter.fillRect(closeRect, QColor(220, 53, 69, 25)); // rgba(220, 53, 69, 0.1)
                painter.setPen(QPen(QColor("#dc3545"), 1));
                painter.drawRoundedRect(closeRect, 4, 4);
            }
            
            // 绘制关闭图标
            if (!m_closeHoverIcon.isNull()) {
                // 缩放图标到合适大小
                QPixmap scaledIcon = m_closeHoverIcon.scaled(buttonSize, buttonSize, Qt::KeepAspectRatio);
                painter.drawPixmap(closeRect, scaledIcon);
            } else {
                // 如果图标加载失败，绘制一个简单的X
                painter.setPen(QPen(QColor("#dc3545"), 2));
                int margin = 3;
                painter.drawLine(closeRect.left() + margin, closeRect.top() + margin, 
                               closeRect.right() - margin, closeRect.bottom() - margin);
                painter.drawLine(closeRect.right() - margin, closeRect.top() + margin, 
                               closeRect.left() + margin, closeRect.bottom() - margin);
            }
            
            m_closeRects[i] = closeRect;
        } else {
            // 移除非悬停标签页的关闭区域
            m_closeRects.remove(i);
        }
    }
}

void CustomTabBar::mousePressEvent(QMouseEvent *event)
{
    // 检查是否点击了关闭按钮
    for (auto it = m_closeRects.constBegin(); it != m_closeRects.constEnd(); ++it) {
        if (it.value().contains(event->pos())) {
            m_pressedCloseButton = it.key();
            update(); // 刷新显示按下效果
            return;
        }
    }
    QTabBar::mousePressEvent(event);
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_pressedCloseButton >= 0) {
        // 检查是否在同一个关闭按钮上释放
        auto it = m_closeRects.find(m_pressedCloseButton);
        if (it != m_closeRects.end() && it.value().contains(event->pos())) {
            emit tabCloseRequested(m_pressedCloseButton);
        }
        
        m_pressedCloseButton = -1;
        update(); // 刷新移除按下效果
        return;
    }
    QTabBar::mouseReleaseEvent(event);
}