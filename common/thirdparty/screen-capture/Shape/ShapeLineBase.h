#pragma once

#include <QMouseEvent>
#include <QPainter>
#include <QObject>
#include <QPainterPath>
#include "ShapeBase.h"

class ShapeLineBase:public ShapeBase
{
    Q_OBJECT
public:
    ShapeLineBase(QObject* parent = nullptr);
    virtual ~ShapeLineBase();
    virtual void paintDragger(QPainter* painter) override;
    virtual bool mouseMove(QMouseEvent* event) override;
    virtual void mouseDrag(QMouseEvent* event) override;
    virtual bool mousePress(QMouseEvent* event) override;
    virtual bool mouseRelease(QMouseEvent* event) override;
    
protected:
    // 平滑绘制辅助方法
    void addSmoothPoint(const QPointF& point);
    void createSmoothPath();
    QPointF getMidPoint(const QPointF& p1, const QPointF& p2) const;
    qreal getDistance(const QPointF& p1, const QPointF& p2) const;
public:
    qreal strokeWidth;
protected:
    QPainterPath path;
    QPainterPath pathStart;
    QPainterPath pathStroker;  //描边路径
    QPainterPathStroker stroker; 
    QPointF pressPos;
    bool isStraight{ false };
    
    // 平滑绘制相关
    QVector<QPointF> smoothPoints;  // 存储平滑点
    QPointF lastPoint;              // 上一个点
    QPointF lastControlPoint;       // 上一个控制点
    static constexpr qreal SMOOTH_THRESHOLD = 3.0;  // 平滑阈值
private:
};
