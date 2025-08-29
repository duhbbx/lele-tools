#pragma once

#include <QObject>
#include <QPainter>

// 前向声明
class QMouseEvent;

// 使用Qt提供的Windows类型定义
#ifdef _WIN32
#include <QtGui/qwindowdefs_win.h>
#endif

class CutMask  : public QObject
{
	Q_OBJECT
public:
	CutMask(QObject* parent = nullptr);
	~CutMask();
	void mousePress(QMouseEvent* event);
	void mouseDrag(QMouseEvent* event);
	void mouseMove(QMouseEvent* event);
	void paint(QPainter& p);
	HWND getHwndByPos(const QPoint& pos);
public:
	QRect rectMask;
protected:
private:
	void changeRectMask(const QPoint& pos);
	void changeMouseState(const int& x, const int& y);
	void moveMaskRect(const QPoint& pos);
	void initWinRect();
private:
	QList<QRect> rectWins;
	QList<HWND> hwnds;
	uint mouseState{ 0 };
	QPoint posPress;
	float maskStroke{ 1.5 };
};
