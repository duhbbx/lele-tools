#include "customtabbar.h"
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionTab>

CustomTabBar::CustomTabBar(QWidget *parent)
    : QTabBar(parent), m_hoveredTab(-1), m_hoveredCloseButton(-1), m_pressedCloseButton(-1)
{
    setMouseTracking(true);
    setElideMode(Qt::ElideRight);

    setStyleSheet(R"(
        QTabBar::tab {
            background-color: #f8f9fa;
            color: #495057;
            padding: 6px 28px 6px 12px;
            border: 1px solid #dee2e6;
            border-bottom: none;
            border-top: none;
            margin-right: -1px;
            min-width: 80px;
            max-width: 200px;
        }
        QTabBar::tab:first {
            padding-right: 12px;
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
            width: 0px;
            height: 0px;
        }
    )");
}

QSize CustomTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    // 首页 tab (index 0) 不需要关闭按钮的额外空间
    if (index > 0) {
        size.setWidth(qMax(size.width(), 90));
    }
    return size;
}

void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());
    int hoveredCloseButton = -1;

    for (auto it = m_closeRects.constBegin(); it != m_closeRects.constEnd(); ++it) {
        if (it.value().contains(event->pos())) {
            hoveredCloseButton = it.key();
            break;
        }
    }

    bool needUpdate = (index != m_hoveredTab) || (hoveredCloseButton != m_hoveredCloseButton);
    m_hoveredTab = index;
    m_hoveredCloseButton = hoveredCloseButton;

    if (needUpdate) update();
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
    painter.setRenderHint(QPainter::Antialiasing, true);
    QStyleOptionTab opt;

    for (int i = 0; i < count(); ++i) {
        initStyleOption(&opt, i);
        painter.drawControl(QStyle::CE_TabBarTabShape, opt);
        painter.drawControl(QStyle::CE_TabBarTabLabel, opt);

        // 首页 (index 0) 不绘制关闭按钮
        if (i == 0) continue;

        // 只在悬停时显示关闭按钮
        if (i == m_hoveredTab) {
            QRect tabRectCurrent = tabRect(i);

            // 关闭按钮：16x16 圆形热区，在 tab 右侧
            int btnSize = 16;
            int rightMargin = 6;
            QRect closeRect(
                tabRectCurrent.right() - btnSize - rightMargin,
                tabRectCurrent.y() + (tabRectCurrent.height() - btnSize) / 2,
                btnSize, btnSize
            );

            // 绘制圆形背景
            if (i == m_pressedCloseButton) {
                painter.setBrush(QColor(0, 0, 0, 40));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(closeRect);
            } else if (i == m_hoveredCloseButton) {
                painter.setBrush(QColor(0, 0, 0, 25));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(closeRect);
            }

            // 绘制 X 图标（基于中心点计算，确保完美居中）
            QColor xColor = (i == m_hoveredCloseButton || i == m_pressedCloseButton)
                            ? QColor("#495057") : QColor("#868e96");
            painter.setPen(QPen(xColor, 1.3, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            qreal cx = closeRect.x() + closeRect.width() / 2.0;
            qreal cy = closeRect.y() + closeRect.height() / 2.0;
            qreal half = 3.0; // X 半径
            painter.drawLine(QPointF(cx - half, cy - half), QPointF(cx + half, cy + half));
            painter.drawLine(QPointF(cx + half, cy - half), QPointF(cx - half, cy + half));

            m_closeRects[i] = closeRect;
        } else {
            m_closeRects.remove(i);
        }
    }
}

void CustomTabBar::mousePressEvent(QMouseEvent *event)
{
    for (auto it = m_closeRects.constBegin(); it != m_closeRects.constEnd(); ++it) {
        if (it.value().contains(event->pos())) {
            m_pressedCloseButton = it.key();
            update();
            return;
        }
    }
    QTabBar::mousePressEvent(event);
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_pressedCloseButton >= 0) {
        auto it = m_closeRects.find(m_pressedCloseButton);
        if (it != m_closeRects.end() && it.value().contains(event->pos())) {
            emit tabCloseRequested(m_pressedCloseButton);
        }
        m_pressedCloseButton = -1;
        update();
        return;
    }
    QTabBar::mouseReleaseEvent(event);
}
