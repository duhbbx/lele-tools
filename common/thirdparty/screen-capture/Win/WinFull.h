#pragma once
#include "WinBase.h"


class CutMask;
class PixelInfo;
class WinFull : public WinBase
{
	Q_OBJECT
public:
	WinFull(QWidget* parent = nullptr);
	~WinFull();
	void pin();
	QImage getTargetImg() override;  // 移到public区域
	
	// 重写保存方法，避免退出应用程序
	void saveToClipboard() override;
	void saveToFile() override;
	void keyPressEvent(QKeyEvent* event) override;

signals:
	void captureFinished();  // 截图完成信号

public:
	PixelInfo* pixelInfo;
	CutMask* cutMask;
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void closeEvent(QCloseEvent* event) override;
private:
	void initWindow();
private:
};

