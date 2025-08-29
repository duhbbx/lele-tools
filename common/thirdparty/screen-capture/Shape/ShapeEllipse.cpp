#include <qpainter.h>
#include <QPen>

#include "ShapeEllipse.h"
#include "../App/App.h"
#include "../App/Logger.h"
#include "../Win/WinBase.h"
#include "../Tool/ToolSub.h"

ShapeEllipse::ShapeEllipse(QObject* parent) : ShapeRectBase(parent)
{
    LOG_DEBUG("ShapeEllipse", "=== ShapeEllipse构造函数开始 ===");
    
    auto win = (WinBase*)parent;
    if (!win) {
        LOG_ERROR("ShapeEllipse", "父窗口为空");
        return;
    }
    
    if (!win->toolSub) {
        LOG_ERROR("ShapeEllipse", "toolSub为空，使用默认设置");
        isFill = false;
        color = Qt::red;
        strokeWidth = 2;
        return;
    }
    
    try {
        LOG_DEBUG("ShapeEllipse", "获取工具设置");
        isFill = win->toolSub->getSelectState("ellipseFill");
        color = win->toolSub->getColor();
        strokeWidth = win->toolSub->getStrokeWidth();
        
        LOG_DEBUG("ShapeEllipse", QString("设置: 填充=%1, 颜色=RGB(%2,%3,%4), 线宽=%5")
                  .arg(isFill ? "是" : "否")
                  .arg(color.red()).arg(color.green()).arg(color.blue())
                  .arg(strokeWidth));
                  
    } catch (const std::exception& e) {
        LOG_ERROR("ShapeEllipse", QString("获取工具设置异常: %1").arg(e.what()));
        isFill = false;
        color = Qt::red;
        strokeWidth = 2;
    } catch (...) {
        LOG_ERROR("ShapeEllipse", "获取工具设置发生未知异常");
        isFill = false;
        color = Qt::red;
        strokeWidth = 2;
    }
    
    LOG_DEBUG("ShapeEllipse", "=== ShapeEllipse构造函数完成 ===");
}

ShapeEllipse::~ShapeEllipse()
{
}

void ShapeEllipse::paint(QPainter* painter)
{
    if (isFill) {
        painter->setBrush(QBrush(color));
        painter->setPen(Qt::NoPen);
    }
    else {
        QPen pen(color);
        pen.setWidthF(strokeWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
    }
	painter->drawEllipse(shape);
}
void ShapeEllipse::mouseOnShape(QMouseEvent* event)
{
    auto pos = event->position();
    auto center = shape.center();
    float half{ strokeWidth / 2.f };
    QRectF outerRect = shape.adjusted(-half, -half, half, half);
    qreal spanX{ pos.x() - center.x() }, spanY{ pos.y() - center.y() };
    float normalizedX = spanX / static_cast<double>(outerRect.width() / 2);
    float normalizedY = spanY / static_cast<double>(outerRect.height() / 2);
    auto flag = (normalizedX * normalizedX + normalizedY * normalizedY <= 1.0);
    if (flag) {
        QRectF innerRect = shape.adjusted(half, half, -half, -half);
        normalizedX = spanX / static_cast<double>(innerRect.width() / 2);
        normalizedY = spanY / static_cast<double>(innerRect.height() / 2);
        flag = (normalizedX * normalizedX + normalizedY * normalizedY <= 1.0);
        if (!flag) {
            hoverDraggerIndex = 8;
            auto win = (WinBase*)parent();
            win->setCursor(Qt::SizeAllCursor);
        }
    }
}
