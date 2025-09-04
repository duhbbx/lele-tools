#include "Win/WinBase.h"
#include "Win/Canvas.h"
#include "ToolBase.h"

#include <ShellScalingApi.h>

ToolBase::ToolBase(QWidget* parent) : QWidget(parent) {
}

ToolBase::~ToolBase() = default;

void ToolBase::btnCheckChange(BtnCheck* btn) {
}

void ToolBase::btnClick(Btn* btn) {
}

void ToolBase::initWindow() {
    setMouseTracking(true);
    // 不要设置为不可见，让工具栏能正常显示
    // setVisible(false);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    // 这个设置启用了 Qt 窗口部件的鼠标悬停检测功能
    // 告诉 Qt 系统这个窗口部件需要接收鼠标悬停事件
    // 使窗口部件能够检测到鼠标进入和离开的状态

    // 设置了 Qt::WA_Hover 后，当鼠标移动时会触发：
    // enterEvent() - 鼠标进入窗口部件区域时
    // leaveEvent() - 鼠标离开窗口部件区域时
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    
    qDebug() << "ToolBase: initWindow完成，鼠标跟踪已启用";
}

void ToolBase::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    //用qt的setFocus始终无法使ToolSub聚焦
    const auto wid = reinterpret_cast<HWND>(winId());
    SetFocus(wid);
}

void ToolBase::keyPressEvent(QKeyEvent* event) {
    const auto win = dynamic_cast<WinBase*>(parent());
    win->keyPressEvent(event);
}
