#include "Win/WinBase.h"
#include "Win/Canvas.h"
#include "ToolBase.h"

#include <ShellScalingApi.h>

ToolBase::ToolBase(QWidget* parent) : QWidget(parent) {
}

ToolBase::~ToolBase() {
}

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
    const auto win = static_cast<WinBase*>(parent());
    win->keyPressEvent(event);
}
