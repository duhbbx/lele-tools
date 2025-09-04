#include <qpainter.h>
#include <QTransform>
#include <numbers>
#include <cmath>

#include "ShapeLineBase.h"
#include "../App/App.h"
#include "../Tool/ToolSub.h"
#include "../Win/WinBase.h"
#include "../Win/Canvas.h"


ShapeLineBase::ShapeLineBase(QObject* parent) : ShapeBase(parent)
{
    auto win = (WinBase*)(parent);
    strokeWidth = win->toolSub->getStrokeWidth();
    stroker.setWidth(strokeWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    prepareDraggers(2);
}

ShapeLineBase::~ShapeLineBase()
{
}

void ShapeLineBase::paintDragger(QPainter* painter)
{
    if (state != ShapeState::ready) return;
    QPen pen;
    pen.setColor(Qt::black);
    pen.setWidth(1);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(pen);
    painter->drawRect(draggers[0]);
    painter->drawRect(draggers[1]);
}
bool ShapeLineBase::mouseMove(QMouseEvent* event)
{
    if (state != ShapeState::ready) return false; //!isStraight || 
    auto pos = event->pos();
    hoverDraggerIndex = -1;
    if (draggers[0].contains(pos)) {
        hoverDraggerIndex = 0;
    }
    else if (draggers[1].contains(pos)) {
        hoverDraggerIndex = 1;
    }
    else if(pathStroker.contains(pos)){
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
bool ShapeLineBase::mousePress(QMouseEvent* event)
{
    if (state == ShapeState::temp) {
        pressPos = event->position();
        isStraight = event->modifiers() & Qt::ShiftModifier;
        state = (ShapeState)((int)ShapeState::sizing0 + 1);
        
        // 初始化平滑绘制
        if (!isStraight) {
            smoothPoints.clear();
            smoothPoints.append(pressPos);
            lastPoint = pressPos;
            lastControlPoint = pressPos;
        }
        
        path.moveTo(pressPos);
        return true;
    }
    else if (hoverDraggerIndex >= 0) {
        if (state == ShapeState::ready) {
            auto win = (WinBase*)parent();
            win->canvas->removeShapeFromBoard(this);
        }
        pressPos = event->position();
        state = (ShapeState)((int)ShapeState::sizing0 + hoverDraggerIndex);
        if (hoverDraggerIndex == 0) {
            pathStart.moveTo(pressPos);
        }
        return true;
    }
    return false;
}
bool ShapeLineBase::mouseRelease(QMouseEvent* event)
{
    if (state < ShapeState::sizing0) return false;
    if (path.isEmpty()) {
        return false;
    }
    
    // 如果不是直线模式，创建平滑路径
    if (!isStraight && !smoothPoints.isEmpty()) {
        createSmoothPath();
    }
    
    if (state == ShapeState::sizing0) {
        QPainterPath tempPath;
        auto ele = pathStart.elementAt(pathStart.elementCount() - 1);
        tempPath.moveTo(ele.x, ele.y);
        for (int i = pathStart.elementCount() - 2; i >= 0; --i)
        {
            auto ele = pathStart.elementAt(i);
            tempPath.lineTo(ele.x, ele.y);
        }
        pathStart.clear();
        for (int i = 0; i < path.elementCount(); i++)
        {
            auto ele = path.elementAt(i);
            tempPath.lineTo(ele.x, ele.y);
        }
        path = tempPath;
    }
    auto startPos = path.elementAt(0);
    auto endPos = path.elementAt(path.elementCount()-1);
    auto half{ draggerSize / 2 };
    draggers[0].setRect(startPos.x - half, startPos.y - half, draggerSize, draggerSize);
    draggers[1].setRect(endPos.x - half, endPos.y - half, draggerSize, draggerSize);
    pathStroker = stroker.createStroke(path);
    state = ShapeState::ready;
    auto win = (WinBase*)parent();
    win->canvas->setHoverShape(this);
    return true;
}
void ShapeLineBase::mouseDrag(QMouseEvent* event)
{
    if (state == ShapeState::ready) return;
    auto pos = event->position();
    if (isStraight) {
        if (state == ShapeState::sizing0) {
            path.setElementPositionAt(0, pos.x(), pos.y());
        }
        else if (state == ShapeState::sizing1) {
            if (path.elementCount() < 2) {
                path.lineTo(pos);
            }
            else {
                path.setElementPositionAt(1, pos.x(), pos.y());
            }            
        }
        else if (state == ShapeState::moving) {
            auto span = pos - pressPos;
            auto startPos = path.elementAt(0);
            auto endPos = path.elementAt(1);
            path.setElementPositionAt(0, startPos.x+span.x(), startPos.y+span.y());
            path.setElementPositionAt(1, endPos.x+span.x(), endPos.y+span.y());
            pressPos = pos;
        }
    }
    else {
        if (state == ShapeState::sizing0) {
            // 使用平滑绘制
            addSmoothPoint(pos);
        }
        else if (state == ShapeState::sizing1) {
            addSmoothPoint(pos);
        }
        else {
            auto span = pos - pressPos;
            path.translate(span);
            pressPos = pos;
        }
    }
    auto win = (WinBase*)parent();
    win->update();
}

// 平滑绘制实现方法
void ShapeLineBase::addSmoothPoint(const QPointF& point) {
    // 只有当点之间的距离大于阈值时才添加
    if (getDistance(lastPoint, point) > SMOOTH_THRESHOLD) {
        smoothPoints.append(point);
        lastPoint = point;
        
        // 实时更新路径用于预览
        if (smoothPoints.size() >= 3) {
            // 使用二次贝塞尔曲线连接最后三个点
            int lastIndex = smoothPoints.size() - 1;
            QPointF p1 = smoothPoints[lastIndex - 2];
            QPointF p2 = smoothPoints[lastIndex - 1];
            QPointF p3 = smoothPoints[lastIndex];
            
            QPointF mid1 = getMidPoint(p1, p2);
            QPointF mid2 = getMidPoint(p2, p3);
            
            if (state == ShapeState::sizing0) {
                pathStart.quadTo(p2, mid2);
            } else {
                path.quadTo(p2, mid2);
            }
        } else {
            // 前几个点直接连线
            if (state == ShapeState::sizing0) {
                pathStart.lineTo(point);
            } else {
                path.lineTo(point);
            }
        }
    }
}

void ShapeLineBase::createSmoothPath() {
    if (smoothPoints.size() < 2) return;
    
    QPainterPath smoothPath;
    smoothPath.moveTo(smoothPoints.first());
    
    if (smoothPoints.size() == 2) {
        // 只有两个点，直接连线
        smoothPath.lineTo(smoothPoints.last());
    } else if (smoothPoints.size() == 3) {
        // 三个点，使用二次贝塞尔曲线
        smoothPath.quadTo(smoothPoints[1], smoothPoints[2]);
    } else {
        // 多个点，使用平滑的二次贝塞尔曲线
        for (int i = 1; i < smoothPoints.size() - 1; i++) {
            QPointF p1 = smoothPoints[i - 1];
            QPointF p2 = smoothPoints[i];
            QPointF p3 = smoothPoints[i + 1];
            
            // 计算控制点
            QPointF mid1 = getMidPoint(p1, p2);
            QPointF mid2 = getMidPoint(p2, p3);
            
            if (i == 1) {
                // 第一段，从起点到第一个中点
                smoothPath.quadTo(p2, mid2);
            } else if (i == smoothPoints.size() - 2) {
                // 最后一段，从上一个中点到终点
                smoothPath.quadTo(p2, smoothPoints.last());
            } else {
                // 中间段，使用二次贝塞尔曲线
                smoothPath.quadTo(p2, mid2);
            }
        }
    }
    
    // 更新主路径
    if (state == ShapeState::sizing0) {
        pathStart = smoothPath;
    } else {
        path = smoothPath;
    }
}

QPointF ShapeLineBase::getMidPoint(const QPointF& p1, const QPointF& p2) const {
    return QPointF((p1.x() + p2.x()) / 2.0, (p1.y() + p2.y()) / 2.0);
}

qreal ShapeLineBase::getDistance(const QPointF& p1, const QPointF& p2) const {
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return sqrt(dx * dx + dy * dy);
}
