
#include "WinPin.h"
#include "WinFull.h"
#include "Canvas.h"
#include "Shape/ShapeBase.h"
#include "App/Util.h"
#include "Tool/ToolMain.h"
#include "WinPinBtns.h"

// Windows API动态加载支持
#ifndef WINAPI
#define WINAPI __stdcall
#endif

// 声明基本的Windows API函数
extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
}

// DWM相关类型和常量
typedef struct _MARGINS {
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
} MARGINS;

#define DWMWA_NCRENDERING_POLICY 2
#define DWMWA_ALLOW_NCPAINT 4

// Windows API函数类型定义
typedef long (WINAPI *PFN_DwmExtendFrameIntoClientArea)(void* hWnd, const MARGINS* pMarInset);
typedef long (WINAPI *PFN_DwmSetWindowAttribute)(void* hwnd, unsigned long dwAttribute, const void* pvAttribute, unsigned long cbAttribute);

// 全局函数指针
static PFN_DwmExtendFrameIntoClientArea g_DwmExtendFrameIntoClientArea = nullptr;
static PFN_DwmSetWindowAttribute g_DwmSetWindowAttribute = nullptr;

// 动态加载DWM API函数
static bool loadDwmAPI() {
    static bool loaded = false;
    if (loaded) return true;
    
    // 加载dwmapi.dll
    void* hDwmapi = LoadLibraryA("dwmapi.dll");
    if (!hDwmapi) return false;
    
    // 获取函数地址
    g_DwmExtendFrameIntoClientArea = (PFN_DwmExtendFrameIntoClientArea)GetProcAddress(hDwmapi, "DwmExtendFrameIntoClientArea");
    g_DwmSetWindowAttribute = (PFN_DwmSetWindowAttribute)GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
    
    loaded = (g_DwmExtendFrameIntoClientArea && g_DwmSetWindowAttribute);
    return loaded;
}


WinPin::WinPin(const QPoint& pos, QImage& img, QWidget* parent) : WinBase(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowStaysOnTopHint);
    move(pos);
    initWindow();
    btns = new WinPinBtns(this);
    canvas = new Canvas(img, this);
    show();
    auto dpr = devicePixelRatio();
    if (dpr != img.devicePixelRatio()) {
        img.setDevicePixelRatio(dpr);
    }
    setFixedSize(img.width() / dpr, img.height() / dpr);
    initSize = size();
}

WinPin::~WinPin()
{
   
}

void WinPin::resetTool()
{
    if (toolMain) {
        toolMain->close();
        toolMain = nullptr;
        setCursor(Qt::SizeAllCursor);
        state = State::start;
    }
    else {
        if (scaleNum != 1.0) {
            scaleNum = 1.0;
            setFixedSize(initSize);
        }
        toolMain = new ToolMain(this);
        toolMain->move(pos().x(), pos().y() + height() + 4);
        toolMain->show();
    }
}

void WinPin::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    canvas->paint(p);
    if (scaleNum != 1.0)
    {
        auto text = QString("Scale:%1").arg(QString::number(scaleNum, 'f', 2));
        auto font = Util::getTextFont(10);
        QFontMetrics fm(*font);
        p.setFont(*font);
        int w = fm.horizontalAdvance(text);
        QRect rect(0, 0, w + 14, 30);
        p.setBrush(QColor(0, 0, 0, 120));
        p.setPen(Qt::NoPen);
        p.drawRect(rect);
        p.setPen(QPen(QBrush(Qt::white), 1));
        p.setBrush(Qt::NoBrush);
        p.drawText(rect, Qt::AlignCenter, text);
    }
}

void WinPin::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {        
        if (state >= State::rect) {
            canvas->mousePress(event);
        }
        else {
            posPress = event->globalPosition() - window()->pos();
        }
    }
}

void WinPin::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton) {
        if (state >= State::rect) {
            canvas->mouseDrag(event);
		}
		else {
            auto p = event->globalPosition() - posPress;
            move(p.toPoint());
		}
    }
    else
    {
        if (state >= State::rect) {
            canvas->mouseMove(event);
        }
    }
}

void WinPin::mouseReleaseEvent(QMouseEvent* event)
{
    if (state >= State::rect) {
        canvas->mouseRelease(event);
    }
}

void WinPin::wheelEvent(QWheelEvent* event)
{
    if (toolMain) {
        toolMain->close();
        toolMain = nullptr;
        state = State::start;
        setCursor(Qt::SizeAllCursor);
    }
    int delta = event->angleDelta().y();    
    if (delta > 0) {        
        scaleNum += 0.06;
    }
    else if (delta < 0) {        
        scaleNum -= 0.06;
    }
    QSize newSize = initSize * scaleNum;
    setFixedSize(newSize);
}

void WinPin::initWindow()
{
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setMouseTracking(true);
    setCursor(Qt::SizeAllCursor);
    
    // 尝试加载DWM API
    if (loadDwmAPI()) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        MARGINS margins = { 1, 1, 1, 1 };
        
        // 使用动态加载的函数
        g_DwmExtendFrameIntoClientArea(hwnd, &margins);
        
        int value = 2;
        g_DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &value, sizeof(value));
        g_DwmSetWindowAttribute(hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));
    }
    // 如果DWM API加载失败，窗口仍然可以正常工作，只是没有特殊效果
}

QImage WinPin::getTargetImg()
{
    QPainter p(&canvas->imgBg);
    p.drawImage(QPoint(0, 0), canvas->imgBoard);
    return canvas->imgBg;
}

void WinPin::moveEvent(QMoveEvent* event)
{
	if (!toolMain) return;
    auto pos = this->pos();
    toolMain->move(pos.x(), pos.y() + height() + 4);
}

void WinPin::resizeEvent(QResizeEvent* event)
{
    if (btns) {
        btns->move(width() - btns->width(), 0);
    }
}

void WinPin::leaveEvent(QEvent* event)
{
    if (btns->isVisible()) {
        btns->hide();
    }
}

void WinPin::enterEvent(QEnterEvent* event)
{
    if (!btns->isVisible()) {
        btns->show();
    }
}

void WinPin::closeEvent(QCloseEvent* event)
{
    deleteLater();
    // 移除qApp->exit()调用，Pin窗口关闭不应退出应用程序
}

