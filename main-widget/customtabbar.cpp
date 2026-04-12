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
            padding: 6px 12px;
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
            width: 0px;
            height: 0px;
        }
    )");
}

QSize CustomTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    // 非首页 tab 右侧留出关闭按钮空间（16px 按钮 + 6px 间距）
    if (index > 0) {
        size.setWidth(size.width() + 22);
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

        if (i == 0) {
            // 首页：正常绘制文字，不绘制关闭按钮
            painter.drawControl(QStyle::CE_TabBarTabLabel, opt);
            continue;
        }

        // 非首页：手动绘制文字，右侧留出关闭按钮空间
        {
            QRect textRect = opt.rect;
            textRect.setRight(textRect.right() - 24); // 留 24px 给关闭按钮
            painter.save();
            painter.setPen(opt.state & QStyle::State_Selected ? QColor("#212529") : QColor("#495057"));
            QFont f = painter.font();
            f.setPointSize(9);
            painter.setFont(f);
            QString elidedText = painter.fontMetrics().elidedText(opt.text, Qt::ElideRight, textRect.width() - 16);
            painter.drawText(textRect, Qt::AlignCenter, elidedText);
            painter.restore();
        }

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
