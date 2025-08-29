#include <qpainter.h>
#include <QTransform>
#include <numbers>

#include "ShapeNumber.h"
#include "../App/App.h"
#include "../App/Logger.h"
#include "../Tool/ToolSub.h"
#include "../Win/WinBase.h"
#include "../Win/Canvas.h"

namespace {
    static int numVal{ 0 };
    static qreal defalutR{ 18.f };
}


ShapeNumber::ShapeNumber(QObject* parent) : ShapeBase(parent)
{
    LOG_DEBUG("ShapeNumber", "=== ShapeNumber构造函数开始 ===");
    
    auto win = (WinBase*)parent;
    prepareDraggers(3);
    
    if (!win) {
        LOG_ERROR("ShapeNumber", "父窗口为空");
        return;
    }
    
    if (!win->toolSub) {
        LOG_ERROR("ShapeNumber", "toolSub为空，使用默认设置");
        isFill = true;
        color = Qt::red;
        val = ++numVal;
        r = defalutR;
        return;
    }
    
    try {
        LOG_DEBUG("ShapeNumber", "获取工具设置");
        isFill = win->toolSub->getSelectState("numberFill");
        color = win->toolSub->getColor();
        val = ++numVal;
        r = defalutR;
        
        LOG_DEBUG("ShapeNumber", QString("设置: 填充=%1, 颜色=RGB(%2,%3,%4), 数字=%5, 半径=%6")
                  .arg(isFill ? "是" : "否")
                  .arg(color.red()).arg(color.green()).arg(color.blue())
                  .arg(val).arg(r));
                  
    } catch (const std::exception& e) {
        LOG_ERROR("ShapeNumber", QString("获取工具设置异常: %1").arg(e.what()));
        isFill = true;
        color = Qt::red;
        val = ++numVal;
        r = defalutR;
    } catch (...) {
        LOG_ERROR("ShapeNumber", "获取工具设置发生未知异常");
        isFill = true;
        color = Qt::red;
        val = ++numVal;
        r = defalutR;
    }
    
    LOG_DEBUG("ShapeNumber", "=== ShapeNumber构造函数完成 ===");
}

ShapeNumber::~ShapeNumber()
{

}

void ShapeNumber::resetValBy(const int& num)
{
    numVal += num;
}

void ShapeNumber::resetShape()
{
    shape.clear();
    auto x1{ r * qCos(qDegreesToRadians(12.0)) }, y1{ -r * qSin(qDegreesToRadians(12.0)) };
    shape.moveTo(x1,y1);
    QRectF rect(-r, -r, 2 * r, 2 * r);
    shape.arcTo(rect, 12, 338);
    shape.lineTo(r+r/3, 0); //三角形箭头的高度为r/3
    shape.lineTo(x1, y1);
    QTransform transform;
    transform.translate(centerPos.x(), centerPos.y());
    transform.rotate(-angle);
    shape = transform.map(shape);
    sizePos = transform.map(QPointF(-r,0));
    arrowPos = transform.map(QPointF(r + r / 3, 0));
}

void ShapeNumber::resetDraggers()
{
    auto half{ draggerSize / 2 };
    draggers[0].setRect(centerPos.x() - half, centerPos.y() - half, draggerSize, draggerSize);
    draggers[1].setRect(sizePos.x() - half, sizePos.y() - half, draggerSize, draggerSize);
    draggers[2].setRect(arrowPos.x() - half, arrowPos.y() - half, draggerSize, draggerSize);
}

void ShapeNumber::paint(QPainter* painter)
{
    if (isFill) {
        painter->setBrush(QBrush(color));
        painter->setPen(Qt::NoPen);
        painter->drawPath(shape);
        painter->setPen(QColor(255, 255, 255));
    }
    else {
        painter->setPen(QPen(QBrush(color), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(shape);
        painter->setPen(color);
    }
    QRectF rect(centerPos.x() - r, centerPos.y() - r, 2 * r, 2 * r);
    auto font = painter->font();
    font.setPointSizeF(r / 4 * 3);
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, QString::number(val));
}
void ShapeNumber::paintDragger(QPainter* painter)
{
    if (state != ShapeState::ready) return;
    painter->setPen(QPen(QBrush(QColor(0, 0, 0)), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(draggers[0]);
    painter->drawRect(draggers[1]);
    painter->drawRect(draggers[2]);
}
bool ShapeNumber::mouseMove(QMouseEvent* event)
{
    if (state != ShapeState::ready) return false;
    auto pos = event->pos();
    hoverDraggerIndex = -1;
    if (draggers[0].contains(pos)) {
        hoverDraggerIndex = 8; //移动
	}
	else if (draggers[1].contains(pos)) {
		hoverDraggerIndex = 0; //改变大小
	}
    else if(draggers[2].contains(pos)) {
        hoverDraggerIndex = 1; 
    }
    auto win = (WinBase*)parent();
    if (hoverDraggerIndex > -1) {
        win->canvas->setHoverShape(this);
        win->setCursor(Qt::SizeAllCursor);
		return true;
    }
    return false;
}
bool ShapeNumber::mousePress(QMouseEvent* event)
{
    LOG_DEBUG("ShapeNumber", QString("=== ShapeNumber::mousePress 开始 ==="));
    LOG_DEBUG("ShapeNumber", QString("当前状态: %1, 数字值: %2").arg((int)state).arg(val));
    LOG_DEBUG("ShapeNumber", QString("鼠标位置: (%1,%2)").arg(event->position().x()).arg(event->position().y()));
    
    if (state == ShapeState::temp) {
        LOG_DEBUG("ShapeNumber", "处理临时状态 - 设置中心位置");
        centerPos = event->position();
        pressPos = centerPos;
        state = ShapeState::moving;
        LOG_DEBUG("ShapeNumber", QString("设置中心位置: (%1,%2), 状态改为moving")
                  .arg(centerPos.x()).arg(centerPos.y()));
        resetShape();
        auto win = (WinBase*)parent();
        if (win) {
            LOG_DEBUG("ShapeNumber", "调用win->update()");
            win->update();
        }
        LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mousePress 完成 (temp->moving) ===");
        return true;
    }
    else if (hoverDraggerIndex >= 0) {
        LOG_DEBUG("ShapeNumber", QString("处理拖拽器悬停: 索引=%1").arg(hoverDraggerIndex));
        if (state == ShapeState::ready) {
            LOG_DEBUG("ShapeNumber", "从画板移除当前形状");
            auto win = (WinBase*)parent();
            if (win && win->canvas) {
                win->canvas->removeShapeFromBoard(this);
            }
        }
        pressPos = event->position();
        state = (ShapeState)((int)ShapeState::sizing0 + hoverDraggerIndex);
        LOG_DEBUG("ShapeNumber", QString("状态改为: %1").arg((int)state));
        LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mousePress 完成 (dragger) ===");
        return true;
    }
    
    LOG_DEBUG("ShapeNumber", "没有处理鼠标按下事件");
    LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mousePress 完成 (未处理) ===");
    return false;
}
bool ShapeNumber::mouseRelease(QMouseEvent* event)
{
    LOG_DEBUG("ShapeNumber", QString("=== ShapeNumber::mouseRelease 开始 ==="));
    LOG_DEBUG("ShapeNumber", QString("当前状态: %1, 数字值: %2").arg((int)state).arg(val));
    LOG_DEBUG("ShapeNumber", QString("形状是否为空: %1").arg(shape.isEmpty() ? "是" : "否"));
    
    if (shape.isEmpty()) { //鼠标按下，没有拖拽，随即释放
        LOG_DEBUG("ShapeNumber", "形状为空，返回false");
        LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mouseRelease 完成 (形状为空) ===");
        return false;
    }
    
    if (state >= ShapeState::sizing0) {
        LOG_DEBUG("ShapeNumber", "处理调整大小状态完成");
        resetDraggers();
        state = ShapeState::ready;
        LOG_DEBUG("ShapeNumber", "状态设为ready，重置拖拽器");
        
        auto win = (WinBase*)parent();
        if (win && win->canvas) {
            LOG_DEBUG("ShapeNumber", "设置为悬停形状");
            win->canvas->setHoverShape(this);
        }
        LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mouseRelease 完成 (sizing->ready) ===");
        return true;
    }
    
    LOG_DEBUG("ShapeNumber", QString("当前状态 %1 不需要特殊处理").arg((int)state));
    LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mouseRelease 完成 (无处理) ===");
    return false;
}

void ShapeNumber::mouseDrag(QMouseEvent* event)
{
    LOG_DEBUG("ShapeNumber", QString("=== ShapeNumber::mouseDrag 开始 ==="));
    LOG_DEBUG("ShapeNumber", QString("当前状态: %1, 数字值: %2").arg((int)state).arg(val));
    
    if (state == ShapeState::ready) {
        LOG_DEBUG("ShapeNumber", "状态为ready，不处理拖拽");
        return;
    }
    
    if (state == ShapeState::sizing0) {
        LOG_DEBUG("ShapeNumber", "处理大小调整");
        QLineF line(centerPos, event->pos());
        r = line.length();
		defalutR = r;
        LOG_DEBUG("ShapeNumber", QString("新半径: %1").arg(r));
        resetShape();
    }
    else if (state == ShapeState::sizing1) {
        LOG_DEBUG("ShapeNumber", "处理角度调整");
        QLineF line(centerPos, event->pos());
        angle = line.angle();
        LOG_DEBUG("ShapeNumber", QString("新角度: %1").arg(angle));
        resetShape();
    }
    else if (state == ShapeState::moving) {
        LOG_DEBUG("ShapeNumber", "处理移动");
        auto pos = event->pos();
        auto span = pos - pressPos;
        LOG_DEBUG("ShapeNumber", QString("移动偏移: (%1,%2)").arg(span.x()).arg(span.y()));
        shape.translate(span);
        centerPos += span;
        arrowPos += span;
        sizePos += span;
        pressPos = pos;
        LOG_DEBUG("ShapeNumber", QString("新中心位置: (%1,%2)").arg(centerPos.x()).arg(centerPos.y()));
    }
    
    auto win = (WinBase*)parent();
    if (win) {
        LOG_DEBUG("ShapeNumber", "调用win->update()刷新显示");
        win->update();
    }
    
    LOG_DEBUG("ShapeNumber", "=== ShapeNumber::mouseDrag 完成 ===");
}