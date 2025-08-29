#include <QPainter>

#include "../App/Util.h"
#include "../App/Logger.h"
#include "ToolBase.h"
#include "BtnCheck.h"

BtnCheck::BtnCheck(const QString& name, const QChar& icon, QWidget* parent, State state, bool isChecked) : BtnBase(name, icon, parent),
state{state}, isChecked{ isChecked }
{
    qDebug() << "BtnCheck构造函数: 名称=" << name << "this=" << this << "parent=" << parent;
}

BtnCheck::~BtnCheck()
{

}

void BtnCheck::paintEvent(QPaintEvent* event)
{
    qDebug() << "BtnCheck::paintEvent - 按钮:" << name << "this:" << this << "parent:" << parent();
    
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    auto r = rect().adjusted(2, 2, -2, -2);
    p.setPen(Qt::NoPen);
    auto font = Util::getIconFont(15);
    p.setFont(*font);
    if (isChecked)
    {
        p.setBrush(QColor(228, 238, 255));
        p.drawRoundedRect(r, 6, 6);
        p.setPen(QColor(9, 88, 217));
        p.setBrush(Qt::NoBrush);
        p.drawText(r, Qt::AlignCenter, icon);
    }
    else if (isHover) {
        p.setBrush(QColor(228, 238, 255));
        p.drawRoundedRect(r, 6, 6);
        p.setPen(QColor(33, 33, 33));
        p.setBrush(Qt::NoBrush);
        p.drawText(r, Qt::AlignCenter, icon);
    }
    else
    {
        p.setBrush(Qt::white);
        p.drawRoundedRect(r, 6, 6);
        p.setPen(QColor(33, 33, 33));
        p.setBrush(Qt::NoBrush);
        p.drawText(r, Qt::AlignCenter, icon);
    }
    
    qDebug() << "BtnCheck::paintEvent 完成 - 按钮:" << name;
}

void BtnCheck::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "=== BtnCheck::mousePressEvent 开始 ===";
    qDebug() << "BtnCheck: 按钮名称:" << name;
    qDebug() << "BtnCheck: this指针:" << this;
    qDebug() << "BtnCheck: parent()指针:" << parent();
    
    LOG_DEBUG("BtnCheck", QString("=== BtnCheck鼠标按下事件 ==="));
    LOG_DEBUG("BtnCheck", QString("按钮名称: %1, 当前状态: %2")
              .arg(name).arg(isChecked ? "选中" : "未选中"));
    LOG_DEBUG("BtnCheck", QString("鼠标按钮: %1").arg(event->buttons()));
    
    if (event->buttons() & Qt::LeftButton) {
        qDebug() << "BtnCheck: 左键点击，准备切换状态";
        LOG_DEBUG("BtnCheck", "左键点击，切换按钮状态");
        
        isChecked = !isChecked;
        qDebug() << "BtnCheck: 状态已切换为:" << (isChecked ? "选中" : "未选中");
        LOG_DEBUG("BtnCheck", QString("新状态: %1").arg(isChecked ? "选中" : "未选中"));
        
        qDebug() << "BtnCheck: 调用update()";
        update();
        qDebug() << "BtnCheck: update()完成";
        LOG_DEBUG("BtnCheck", "按钮界面更新完成");
        
        qDebug() << "BtnCheck: 准备获取父对象";
        ToolBase* toolBase = dynamic_cast<ToolBase*>(parent());
        qDebug() << "BtnCheck: dynamic_cast结果:" << toolBase;
        
        if (toolBase) {
            LOG_DEBUG("BtnCheck", QString("调用父工具栏的btnCheckChange，父对象: 0x%1")
                      .arg(reinterpret_cast<quintptr>(toolBase), 0, 16));
            
            qDebug() << "BtnCheck: 即将调用btnCheckChange，按钮:" << name << "状态:" << isChecked;
            qDebug() << "BtnCheck: toolBase指针有效，开始调用";
            
            try {
                qDebug() << "BtnCheck: 进入btnCheckChange调用";
                toolBase->btnCheckChange(this);
                qDebug() << "BtnCheck: btnCheckChange调用返回";
                LOG_DEBUG("BtnCheck", "btnCheckChange调用完成");
                qDebug() << "BtnCheck: btnCheckChange调用成功完成";
            } catch (const std::exception& e) {
                LOG_ERROR("BtnCheck", QString("btnCheckChange调用异常: %1").arg(e.what()));
                qDebug() << "BtnCheck: btnCheckChange异常:" << e.what();
            } catch (...) {
                LOG_ERROR("BtnCheck", "btnCheckChange调用发生未知异常");
                qDebug() << "BtnCheck: btnCheckChange发生未知异常";
            }
        } else {
            LOG_ERROR("BtnCheck", "父对象不是ToolBase类型");
            qDebug() << "BtnCheck: 父对象不是ToolBase类型，parent()类型:" << (parent() ? parent()->metaObject()->className() : "nullptr");
        }
    }
    else {
        qDebug() << "BtnCheck: 非左键点击，忽略";
        LOG_DEBUG("BtnCheck", "非左键点击，忽略");
        // 移除qApp->exit()调用
    }
    
    qDebug() << "=== BtnCheck::mousePressEvent 完成 ===";
    LOG_DEBUG("BtnCheck", "=== BtnCheck鼠标按下事件完成 ===");
}
