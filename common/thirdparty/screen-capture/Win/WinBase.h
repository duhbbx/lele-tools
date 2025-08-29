#pragma once

#include <QMainWindow>
#include <QMouseEvent>
#include <QImage>

#include "../App/State.h"

class ToolMain;
class ToolSub;
class Canvas;
class WinBase  : public QMainWindow
{
	Q_OBJECT
public:
	explicit WinBase(QWidget* parent = nullptr);
	~WinBase() override;
	virtual void saveToClipboard();
	virtual void saveToFile();
	void keyPressEvent(QKeyEvent* event) override;
public:
	int x, y, w, h;
	State state;
	ToolMain* toolMain;
	ToolSub* toolSub;
	Canvas* canvas;
protected:
	virtual QImage getTargetImg()=0;
	static void moveCursor(const QPoint& pos);
	void mouseDoubleClickEvent(QMouseEvent* event) override;
private:

};
