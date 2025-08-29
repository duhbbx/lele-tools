#include "CutMask.h"
#include "WinFull.h"
#include <QPainterPath>
#include <QMouseEvent>
#include "../App/Util.h"
#include "../App/Logger.h"
#include "../Tool/ToolMain.h"

// 基本Windows类型定义
#ifndef WINAPI
#define WINAPI __stdcall
#endif

// 使用Qt已经定义的Windows类型，避免重定义
// 声明基本的Windows API函数
extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryA(const char* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
}

// Windows API函数类型定义
typedef int (WINAPI *EnumWindowsProc)(void* hwnd, long long lParam);
typedef int (WINAPI *PFN_EnumWindows)(EnumWindowsProc lpEnumFunc, long long lParam);
typedef int (WINAPI *PFN_IsWindowVisible)(void* hWnd);
typedef int (WINAPI *PFN_IsIconic)(void* hWnd);
typedef int (WINAPI *PFN_GetWindowTextLength)(void* hWnd);
typedef long (WINAPI *PFN_DwmGetWindowAttribute)(void* hwnd, unsigned long dwAttribute, void* pvAttribute, unsigned long cbAttribute);

// 全局函数指针
static PFN_EnumWindows g_EnumWindows = nullptr;
static PFN_IsWindowVisible g_IsWindowVisible = nullptr;
static PFN_IsIconic g_IsIconic = nullptr;
static PFN_GetWindowTextLength g_GetWindowTextLength = nullptr;
static PFN_DwmGetWindowAttribute g_DwmGetWindowAttribute = nullptr;

// 常量定义
#define DWMWA_EXTENDED_FRAME_BOUNDS 9

// 使用统一的日志系统
#define MODULE_CUTMASK "CutMask"

// 动态加载Windows API函数
static bool loadWindowsAPI() {
    static bool loaded = false;
    if (loaded) {
        LOG_DEBUG(MODULE_CUTMASK, "Windows API already loaded");
        return true;
    }
    
    LOG_INFO(MODULE_CUTMASK, "开始加载Windows API");
    
    // 加载user32.dll
    void* hUser32 = LoadLibraryA("user32.dll");
    if (!hUser32) {
        LOG_ERROR(MODULE_CUTMASK, "无法加载user32.dll");
        return false;
    }
    LOG_DEBUG(MODULE_CUTMASK, "成功加载user32.dll");
    
    // 加载dwmapi.dll
    void* hDwmapi = LoadLibraryA("dwmapi.dll");
    if (!hDwmapi) {
        LOG_ERROR(MODULE_CUTMASK, "无法加载dwmapi.dll");
        return false;
    }
    LOG_DEBUG(MODULE_CUTMASK, "成功加载dwmapi.dll");
    
    // 获取函数地址
    g_EnumWindows = (PFN_EnumWindows)GetProcAddress(hUser32, "EnumWindows");
    g_IsWindowVisible = (PFN_IsWindowVisible)GetProcAddress(hUser32, "IsWindowVisible");
    g_IsIconic = (PFN_IsIconic)GetProcAddress(hUser32, "IsIconic");
    g_GetWindowTextLength = (PFN_GetWindowTextLength)GetProcAddress(hUser32, "GetWindowTextLengthW");
    g_DwmGetWindowAttribute = (PFN_DwmGetWindowAttribute)GetProcAddress(hDwmapi, "DwmGetWindowAttribute");
    
    // 检查函数是否成功获取
    if (!g_EnumWindows) LOG_ERROR(MODULE_CUTMASK, "无法获取EnumWindows函数地址");
    if (!g_IsWindowVisible) LOG_ERROR(MODULE_CUTMASK, "无法获取IsWindowVisible函数地址");
    if (!g_IsIconic) LOG_ERROR(MODULE_CUTMASK, "无法获取IsIconic函数地址");
    if (!g_GetWindowTextLength) LOG_ERROR(MODULE_CUTMASK, "无法获取GetWindowTextLength函数地址");
    if (!g_DwmGetWindowAttribute) LOG_ERROR(MODULE_CUTMASK, "无法获取DwmGetWindowAttribute函数地址");
    
    loaded = (g_EnumWindows && g_IsWindowVisible && g_IsIconic && 
              g_GetWindowTextLength && g_DwmGetWindowAttribute);
    
    if (loaded) {
        LOG_INFO(MODULE_CUTMASK, "Windows API加载成功");
    } else {
        LOG_ERROR(MODULE_CUTMASK, "Windows API加载失败");
    }
    
    return loaded;
}

CutMask::CutMask(QObject* parent) : QObject(parent)
{
    LOG_INFO(MODULE_CUTMASK, "CutMask构造函数开始");
    initWinRect();
    LOG_INFO(MODULE_CUTMASK, "CutMask构造函数完成");
}
CutMask::~CutMask()
{
    LOG_INFO(MODULE_CUTMASK, "CutMask析构函数调用");
}
void CutMask::initWinRect()
{
    LOG_INFO(MODULE_CUTMASK, "开始初始化窗口矩形");
    rectWins.clear();
    hwnds.clear();
    
    // 尝试加载Windows API
    if (!loadWindowsAPI()) {
        LOG_ERROR(MODULE_CUTMASK, "Windows API加载失败，使用默认全屏矩形");
        // 如果加载失败，使用默认的全屏矩形
        auto win = (WinFull*)parent();
        if (win) {
            QRect fullScreen(0, 0, win->width(), win->height());
            rectWins.push_back(fullScreen);
            hwnds.push_back(nullptr);
            LOG_INFO(MODULE_CUTMASK, QString("添加默认全屏矩形: %1x%2").arg(win->width()).arg(win->height()));
        } else {
            LOG_ERROR(MODULE_CUTMASK, "无法获取父窗口，无法创建默认矩形");
        }
        return;
    }
    
    LOG_INFO(MODULE_CUTMASK, "开始枚举系统窗口");
    int windowCount = 0;
    
    // 使用Windows API枚举窗口
    g_EnumWindows([](void* hwnd, long long lparam) -> int {
        static int* pWindowCount = nullptr;
        if (!pWindowCount) {
            // 第一次调用时，从lparam获取计数器指针
            auto self = (CutMask*)lparam;
            // 这里我们需要一个更好的方式来传递计数器
        }
        
        if (!hwnd) return 1;
        if (!g_IsWindowVisible(hwnd)) return 1;
        if (g_IsIconic(hwnd)) return 1;
        if (g_GetWindowTextLength(hwnd) < 1) return 1;
        
        // 定义RECT结构
        struct { long left, top, right, bottom; } rect;
        if (g_DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect)) != 0) {
            return 1;  // 获取窗口属性失败
        }
        
        if (rect.right - rect.left <= 6 || rect.bottom - rect.top <= 6) {
            return 1;
        }
        
        auto self = (CutMask*)lparam;
        auto win = (WinFull*)self->parent();
        if (!win) return 1;
        
        if (rect.left < win->x) rect.left = win->x;
        if (rect.top < win->y) rect.top = win->y;
        if (rect.right > win->x + win->w) rect.right = win->x + win->w;
        if (rect.bottom > win->y + win->h) rect.bottom = win->y + win->h;
        
        auto dpr = win->devicePixelRatio();
        QRect r;
        r.setCoords((rect.left - win->x) / dpr, (rect.top - win->y) / dpr, 
                   (rect.right - win->x) / dpr, (rect.bottom - win->y) / dpr);
        self->rectWins.push_back(r);
        self->hwnds.push_back((HWND)hwnd);
        
        // 记录找到的窗口
        LOG_DEBUG(MODULE_CUTMASK, QString("找到窗口 HWND=0x%1, 矩形=(%2,%3,%4,%5)")
                .arg((quintptr)hwnd, 0, 16)
                .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
        
        return 1;
    }, (long long)this);
    
    LOG_INFO(MODULE_CUTMASK, QString("窗口枚举完成，共找到 %1 个有效窗口").arg(rectWins.size()));
}

void CutMask::mousePress(QMouseEvent* event)
{
    auto win = (WinFull*)parent();
    posPress = event->pos();
    LOG_DEBUG(MODULE_CUTMASK, QString("鼠标按下: 位置=(%1,%2)").arg(posPress.x()).arg(posPress.y()));
    changeRectMask(posPress);
}

void CutMask::mouseDrag(QMouseEvent* event)
{
    auto pos = event->pos();
    auto win = (WinFull*)parent();
    if (win->state == State::start) {
        if (pos == posPress) return;
        rectMask.setCoords(posPress.x(), posPress.y(), pos.x(), pos.y());
        rectMask = rectMask.normalized();
        win->update();
    }
    else if (win->state == State::tool) {
        if (mouseState == 0)
        {
            moveMaskRect(pos);
        }
        else {
            changeRectMask(pos);
        }
        win->update();
    }
}
void CutMask::mouseMove(QMouseEvent* event)
{
    auto pos = event->pos();
    auto win = (WinFull*)parent();
    if (win->state == State::start) {
        for (const auto& rect : rectWins)
        {
            if (rect.contains(pos)) {
                if (rectMask == rect) return;
                rectMask = rect;
                LOG_DEBUG(MODULE_CUTMASK, QString("鼠标移动: 选中窗口矩形=(%1,%2,%3,%4)")
                         .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
                win->update();
                return;
            }
        }
    }
    else if (win->state <= State::tool) {
        changeMouseState(pos.x(), pos.y());
    }
    else {
       
    }
}
void CutMask::moveMaskRect(const QPoint& pos)
{
    auto span = pos - posPress;
    posPress = pos;
    auto target = rectMask.topLeft() + span;
    auto win = (WinFull*)parent();
    auto flag1 = target.x() < 0;
    auto flag2 = target.y() < 0;
    if (flag1 || flag2) {
        if (flag1)target.setX(0);
        if (flag2)target.setY(0);
        rectMask.moveTo(target);
        return;
    }
    flag1 = target.x() + rectMask.width() > win->width();
    flag2 = target.y() + rectMask.height() > win->height();
    if (flag1 || flag2) {
        if(flag1)target.setX(win->width() - rectMask.width());
        if(flag2)target.setY(win->height() - rectMask.height());
        rectMask.moveTo(target);
        return;
    }
    rectMask.moveTo(target);
}


void CutMask::paint(QPainter& p)
{
    LOG_DEBUG(MODULE_CUTMASK, "=== CutMask::paint 开始 ===");
    
    auto win = (WinFull*)parent();
    if (!win) {
        LOG_ERROR(MODULE_CUTMASK, "父窗口为空，无法绘制");
        return;
    }
    
    LOG_DEBUG(MODULE_CUTMASK, QString("父窗口大小: %1x%2").arg(win->w).arg(win->h));
    LOG_DEBUG(MODULE_CUTMASK, QString("当前rectMask: (%1,%2,%3,%4)")
              .arg(rectMask.x()).arg(rectMask.y())
              .arg(rectMask.width()).arg(rectMask.height()));
    LOG_DEBUG(MODULE_CUTMASK, QString("rectMask是否为空: %1").arg(rectMask.isEmpty() ? "是" : "否"));
    
    // 设置画刷和画笔
    p.setBrush(QColor(0, 0, 0, 120));
    QColor borderColor(22, 118, 255);
    p.setPen(QPen(QBrush(borderColor), maskStroke));
    LOG_DEBUG(MODULE_CUTMASK, QString("设置画笔颜色: RGB(%1,%2,%3), 线宽: %4")
              .arg(borderColor.red()).arg(borderColor.green())
              .arg(borderColor.blue()).arg(maskStroke));
    
    // 创建路径
    QPainterPath path;
    QRect outerRect(-1, -1, win->w + 1, win->h + 1);
    path.addRect(outerRect);
    path.addRect(rectMask);
    
    LOG_DEBUG(MODULE_CUTMASK, QString("外部矩形: (%1,%2,%3,%4)")
              .arg(outerRect.x()).arg(outerRect.y())
              .arg(outerRect.width()).arg(outerRect.height()));
    LOG_DEBUG(MODULE_CUTMASK, QString("绘制路径包含 %1 个矩形").arg(2));
    
    // 绘制路径
    p.drawPath(path);
    LOG_DEBUG(MODULE_CUTMASK, "主路径绘制完成");
    
    // 绘制截图区域位置和大小文本
    auto text = QString("X:%1 Y:%2 R:%3 B:%4 W:%5 H:%6")
        .arg(rectMask.x()).arg(rectMask.y())
        .arg(rectMask.right()).arg(rectMask.bottom())
        .arg(rectMask.width()).arg(rectMask.height());
    
    LOG_DEBUG(MODULE_CUTMASK, QString("信息文本: %1").arg(text));
    
    auto font = Util::getTextFont(10);
    if (!font) {
        LOG_ERROR(MODULE_CUTMASK, "无法获取字体");
        return;
    }
    
    QFontMetrics fm(*font);
    p.setFont(*font);
    int w = fm.horizontalAdvance(text);
    
    QRect textRect;
    if (rectMask.y() - 25 >= 0) {
        textRect = QRect(rectMask.x(), rectMask.y() - 25, w + 14, 22);
    }
    else {
        textRect = QRect(rectMask.x()+2, rectMask.y() + 2, w + 14, 22);
    }
    
    LOG_DEBUG(MODULE_CUTMASK, QString("文本矩形: (%1,%2,%3,%4)")
              .arg(textRect.x()).arg(textRect.y())
              .arg(textRect.width()).arg(textRect.height()));
    
    // 绘制文本背景
    p.setBrush(QColor(0, 0, 0, 120));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(textRect, 3, 3);
    
    // 绘制文本
    p.setPen(QPen(QBrush(Qt::white), 1));
    p.setBrush(Qt::NoBrush);
    p.drawText(textRect, Qt::AlignCenter, text);
    
    LOG_DEBUG(MODULE_CUTMASK, "文本绘制完成");
    LOG_DEBUG(MODULE_CUTMASK, "=== CutMask::paint 完成 ===");
}
void CutMask::changeRectMask(const QPoint& pos)
{
    if (mouseState == 1)
    {
        rectMask.setTopLeft(pos);
    }
    else if (mouseState == 2)
    {
        rectMask.setTop(pos.y());
    }
    else if (mouseState == 3)
    {
        rectMask.setTopRight(pos);
    }
    else if (mouseState == 4)
    {
        rectMask.setRight(pos.x());
    }
    else if (mouseState == 5)
    {
        rectMask.setBottomRight(pos);
    }
    else if (mouseState == 6)
    {
        rectMask.setBottom(pos.y());
    }
    else if (mouseState == 7)
    {
        rectMask.setBottomLeft(pos);
    }
    else if (mouseState == 8)
    {
        rectMask.setLeft(pos.x());
    }
    auto win = (WinFull*)parent();
    win->update();
}
void CutMask::changeMouseState(const int& x, const int& y)
{
    auto leftX = rectMask.left(); auto topY = rectMask.top();
    auto rightX = rectMask.right(); auto bottomY = rectMask.bottom();
    auto win = (WinFull*)parent();
    if (x > leftX + 1 && y > topY + 1 && x < rightX - 1 && y < bottomY - 1) //不然截图区域上顶天，下顶地的时候没法改变高度
    {
        win->setCursor(Qt::SizeAllCursor);
        mouseState = 0;
    }
    else if (x <= leftX && y <= topY)
    {
        win->setCursor(Qt::SizeFDiagCursor);
        mouseState = 1;
    }
    else if (x >= leftX && x <= rightX && y <= topY)
    {
        win->setCursor(Qt::SizeVerCursor);
        mouseState = 2;
    }
    else if (x >= rightX && y <= topY)
    {
        win->setCursor(Qt::SizeBDiagCursor);
        mouseState = 3;
    }
    else if (x >= rightX && y >= topY && y <= bottomY)
    {
        win->setCursor(Qt::SizeHorCursor);
        mouseState = 4;
    }
    else if (x >= rightX && y >= bottomY)
    {
        win->setCursor(Qt::SizeFDiagCursor);
        mouseState = 5;
    }
    else if (x >= leftX && x <= rightX && y >= bottomY)
    {
        win->setCursor(Qt::SizeVerCursor);
        mouseState = 6;
    }
    else if (x <= leftX && y >= bottomY)
    {
        win->setCursor(Qt::SizeBDiagCursor);
        mouseState = 7;
    }
    else if (x <= leftX && y <= bottomY && y >= topY)
    {
        win->setCursor(Qt::SizeHorCursor);
        mouseState = 8;
    }
}

HWND CutMask::getHwndByPos(const QPoint& pos)
{
    LOG_DEBUG(MODULE_CUTMASK, QString("根据位置查找窗口句柄: 位置=(%1,%2)").arg(pos.x()).arg(pos.y()));
    
    for (int i = 0; i < rectWins.size(); i++)
    {
        if (rectWins[i].contains(pos)) {
            HWND hwnd = hwnds[i];
            LOG_DEBUG(MODULE_CUTMASK, QString("找到匹配窗口: HWND=0x%1, 矩形=(%2,%3,%4,%5)")
                     .arg((quintptr)hwnd, 0, 16)
                     .arg(rectWins[i].x()).arg(rectWins[i].y())
                     .arg(rectWins[i].width()).arg(rectWins[i].height()));
            return hwnd;
        }
    }
    
    LOG_DEBUG(MODULE_CUTMASK, "未找到匹配的窗口");
    return NULL;
}