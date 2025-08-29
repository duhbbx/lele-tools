#include <qpainter.h>
#include <QTransform>

#include "ShapeArrow.h"
#include "../App/App.h"
#include "../App/Logger.h"
#include "../Tool/ToolSub.h"
#include "../Win/WinBase.h"
#include "../Win/Canvas.h"

ShapeArrow::ShapeArrow(QObject* parent) : ShapeBase(parent)
{
    LOG_DEBUG("ShapeArrow", "=== ShapeArrow构造函数开始 ===");
    
    auto win = (WinBase*)parent;
    if (!win) {
        LOG_ERROR("ShapeArrow", "父窗口为空");
        return;
    }
    
    if (!win->toolSub) {
        LOG_ERROR("ShapeArrow", "toolSub为空，使用默认设置");
        isFill = false;
        color = Qt::red;
        arrowSize = 20;
        return;
    }
    
    try {
        LOG_DEBUG("ShapeArrow", "获取工具设置");
        isFill = win->toolSub->getSelectState("arrowFill");
        color = win->toolSub->getColor();
        arrowSize = win->toolSub->getStrokeWidth();
        
        LOG_DEBUG("ShapeArrow", QString("设置: 填充=%1, 颜色=RGB(%2,%3,%4), 箭头大小=%5")
                  .arg(isFill ? "是" : "否")
                  .arg(color.red()).arg(color.green()).arg(color.blue())
                  .arg(arrowSize));
                  
    } catch (const std::exception& e) {
        LOG_ERROR("ShapeArrow", QString("获取工具设置异常: %1").arg(e.what()));
        isFill = false;
        color = Qt::red;
        arrowSize = 20;
    } catch (...) {
        LOG_ERROR("ShapeArrow", "获取工具设置发生未知异常");
        isFill = false;
        color = Qt::red;
        arrowSize = 20;
    }
    
    LOG_DEBUG("ShapeArrow", "=== ShapeArrow构造函数完成 ===");
}

ShapeArrow::~ShapeArrow()
{
}

void ShapeArrow::resetDragger()
{
    prepareDraggers(2);
    auto half{ draggerSize / 2 };    
    draggers[0].setRect(shape[0].x() - half, shape[0].y() - half, draggerSize, draggerSize);
    draggers[1].setRect(shape[3].x() - half, shape[3].y() - half, draggerSize, draggerSize);
}

void ShapeArrow::resetShape()
{
    QLineF line(startPos, endPos);
    qreal length = line.length();
    auto angle = line.angle();
    qreal v1{ (qreal)arrowSize / 4.f };
    qreal v2{ (qreal)arrowSize / 3.f*2.f };
    shape.clear();
    shape.append(QPointF(0, 0));
    shape.append(QPointF(length - arrowSize, -v1));
    shape.append(QPointF(length - arrowSize - v1, -v2));
    shape.append(QPointF(length, 0));
    shape.append(QPointF(length - arrowSize - v1, v2));
    shape.append(QPointF(length - arrowSize, v1));
    shape.append(QPointF(0, 0));
    QTransform transform;
    transform.translate(startPos.x(), startPos.y());
    transform.rotate(-angle);
    shape = transform.map(shape);
}

void ShapeArrow::paint(QPainter* painter)
{
    if (isFill) {
        painter->setBrush(QBrush(color));
        painter->setPen(Qt::NoPen);
    }
    else {
        painter->setPen(QPen(QBrush(color), 1));
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawPolygon(shape);
}
void ShapeArrow::paintDragger(QPainter* painter)
{
    if (state != ShapeState::ready) return;
    painter->setPen(QPen(QBrush(QColor(0, 0, 0)), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(draggers[0]);
    painter->drawRect(draggers[1]);
}
bool ShapeArrow::mouseMove(QMouseEvent* event)
{
    if (state != ShapeState::ready) return false;
    auto pos = event->pos();
    hoverDraggerIndex = -1;
    if (draggers[0].contains(pos)) {
        hoverDraggerIndex = 0;
    }
    else if (draggers[1].contains(pos)) {
        hoverDraggerIndex = 1;
    } 
    else if (shape.containsPoint(event->pos(), Qt::WindingFill)) {
        hoverDraggerIndex = 8;
    }
    auto win = (WinBase*)parent();
    if (hoverDraggerIndex > -1) {
        win->canvas->setHoverShape(this);
        win->setCursor(Qt::SizeAllCursor);
        return true;
    }
    else {
        return false;
    }
}
bool ShapeArrow::mousePress(QMouseEvent* event)
{
    if (state == ShapeState::temp) {
        startPos = event->position();
        state = (ShapeState)((int)ShapeState::sizing0 + 1);
        return true;
    }
    else if(hoverDraggerIndex >= 0) {
        if (state == ShapeState::ready) {
            auto win = (WinBase*)parent();
            win->canvas->removeShapeFromBoard(this);
        }
        pressPos = event->position();
        state = (ShapeState)((int)ShapeState::sizing0 + hoverDraggerIndex);
        return true;
    }
    return false;
}
bool ShapeArrow::mouseRelease(QMouseEvent* event)
{
    if (shape.isEmpty()) { //鼠标按下，没有拖拽，随即释放
        return false;
    }
    if (state >= ShapeState::sizing0) {
        resetDragger();
        state = ShapeState::ready;
        auto win = (WinBase*)parent();
        win->canvas->setHoverShape(this);
        return true;
    }
    return false;
}
void ShapeArrow::mouseDrag(QMouseEvent* event)
{
    if (state == ShapeState::ready) return;
    if (state == ShapeState::sizing0) {
        startPos = event->pos();
        resetShape();
    }
    else if (state == ShapeState::sizing1) {
        endPos = event->pos();
        resetShape();
    }
    else if (state == ShapeState::moving) {
        auto pos = event->pos();
        auto span = pos - pressPos;
        shape.translate(span);
        startPos = shape[0];
        endPos = shape[3];
        pressPos = pos;
    }
    //if (event->modifiers() & Qt::ShiftModifier) {
    //    if (shape.width() > shape.height()) {
    //        shape.setHeight(shape.width());
    //    }
    //    else {
    //        shape.setWidth(shape.height());
    //    }
    //}
    auto win = (WinBase*)parent();
    win->update();
}
