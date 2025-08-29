#include <QPainter>
#include "Btn.h"
#include "ToolBase.h"
#include "../App/Util.h"


Btn::Btn(const QString& name, const QChar& icon, bool isEnable, QWidget *parent) : BtnBase(name,icon,parent),isEnable{isEnable}
{
    qDebug() << "Btn构造函数: 名称=" << name << "this=" << this << "parent=" << parent << "isEnable=" << isEnable;
    
	if (isEnable)
	{
		setCursor(Qt::PointingHandCursor);
	}
	else
	{
		setCursor(Qt::ArrowCursor);
	}
}

Btn::~Btn()
{

}

void Btn::setEnable(bool flag)
{
	if (flag == isEnable) return;
	isEnable = flag;
	if (flag)
	{
		setCursor(Qt::PointingHandCursor);
	}
	else
	{
		setCursor(Qt::ArrowCursor);
	}
    update();
}

void Btn::paintEvent(QPaintEvent* event)
{
    qDebug() << "Btn::paintEvent - 按钮:" << name << "this:" << this << "parent:" << parent();
    
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    auto r = rect().adjusted(2, 2, -2, -2);
    p.setPen(Qt::NoPen);
    auto font = Util::getIconFont(15);
    p.setFont(*font);
	if (isEnable)
	{
		p.setBrush(isHover ? QColor(228, 238, 255) : Qt::white);
		p.drawRoundedRect(r, 6, 6);
		p.setPen(QColor(33, 33, 33));
	}
	else
	{
		p.setBrush(Qt::white);
		p.drawRoundedRect(r, 6, 6);
		p.setPen(QColor(180, 180, 180));
	}
    p.setBrush(Qt::NoBrush);
    p.drawText(r, Qt::AlignCenter, icon);
    
    qDebug() << "Btn::paintEvent 完成 - 按钮:" << name;
}

void Btn::mousePressEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton) {
		ToolBase* toolBase = dynamic_cast<ToolBase*>(parent());
		if (toolBase) {
			qDebug() << "Btn: 调用btnClick，按钮:" << name;
			toolBase->btnClick(this);
			qDebug() << "Btn: btnClick调用完成";
		} else {
			qDebug() << "Btn: 父对象不是ToolBase类型，无法处理点击";
		}
	}
	else {
		// 移除qApp->exit()调用，非左键点击忽略
	}
}
