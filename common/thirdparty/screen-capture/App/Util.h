#pragma once

#include <QMouseEvent>
#include <QImage>
#include <QScreen>

class Util
{
public:
	static QFont* getIconFont(const int& fontSize);
	static QFont* getTextFont(const int& fontSize);
	static QImage printScreen(const int& x,const int& y,const int& w,const int& h);
	static bool posInScreen(const int& x, const int& y);
	static QScreen* getScreen(const int& x, const int& y);
	static void setClipboardText(const std::wstring& text);
	static void copyColor(const int& key);
	static bool saveToFile(const QImage& img);
	static void imgToClipboard(const QImage& image);
	static bool isImagePath(const QString& path);

	// GDI资源缓存相关（公共接口）
	static void initializeGDICache();
	static void cleanupGDICache();

private:
	// 屏幕缓存相关
	static QList<QScreen*> getCachedScreens();
	static void invalidateScreenCache();
	static void setupScreenChangeMonitoring();
};

