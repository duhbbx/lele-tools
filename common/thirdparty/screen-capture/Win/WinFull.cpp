#include <windows.h>
#include <QApplication>

#include "App/App.h"
#include "App/Util.h"
#include "App/Logger.h"
#include "Shape/ShapeBase.h"
#include "Tool/ToolMain.h"
#include "Tool/ToolSub.h"
#include "WinFull.h"
#include "WinPin.h"
#include "CutMask.h"
#include "PixelInfo.h"
#include "Canvas.h"


WinFull::WinFull(QWidget* parent) : WinBase(parent)
{
    LOG_INFO(MODULE_WIN, "WinFull构造函数开始");
    
    x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    
    LOG_INFO(MODULE_WIN, QString("屏幕区域: x=%1, y=%2, w=%3, h=%4").arg(x).arg(y).arg(w).arg(h));
    
    LOG_DEBUG(MODULE_WIN, "创建CutMask对象");
    cutMask = new CutMask(this);
    
    LOG_DEBUG(MODULE_WIN, "开始截取屏幕");
    auto imgBg = Util::printScreen(x,y,w,h);
    imgBg.setDevicePixelRatio(devicePixelRatio());
    LOG_INFO(MODULE_WIN, QString("屏幕截取完成: 图像大小=%1x%2, DPR=%3")
             .arg(imgBg.width()).arg(imgBg.height()).arg(imgBg.devicePixelRatio()));
    
    LOG_DEBUG(MODULE_WIN, "创建Canvas对象");
    canvas = new Canvas(imgBg,this);
    
    LOG_DEBUG(MODULE_WIN, "初始化窗口");
    initWindow();
    
    LOG_DEBUG(MODULE_WIN, "创建PixelInfo对象");
    pixelInfo = new PixelInfo(this);
    pixelInfo->mouseMove(QCursor::pos());
    
    LOG_INFO(MODULE_WIN, "WinFull构造函数完成");
}
WinFull::~WinFull()
{   
}

void WinFull::paintEvent(QPaintEvent* event)
{
    LOG_DEBUG(MODULE_WIN, QString("=== 开始绘制事件 ==="));
    LOG_DEBUG(MODULE_WIN, QString("绘制事件: state=%1").arg((int)state));
    LOG_DEBUG(MODULE_WIN, QString("窗口大小: %1x%2").arg(width()).arg(height()));
    LOG_DEBUG(MODULE_WIN, QString("事件区域: (%1,%2,%3,%4)")
              .arg(event->rect().x()).arg(event->rect().y())
              .arg(event->rect().width()).arg(event->rect().height()));
    
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    
    LOG_DEBUG(MODULE_WIN, "开始绘制Canvas");
    if (canvas) {
        canvas->paint(p);
        LOG_DEBUG(MODULE_WIN, "Canvas绘制完成");
    } else {
        LOG_ERROR(MODULE_WIN, "Canvas对象为空！");
    }
    
    LOG_DEBUG(MODULE_WIN, "开始绘制CutMask");
    if (cutMask) {
        cutMask->paint(p);
        LOG_DEBUG(MODULE_WIN, "CutMask绘制完成");
    } else {
        LOG_ERROR(MODULE_WIN, "CutMask对象为空！");
    }
    
    LOG_DEBUG(MODULE_WIN, QString("=== 绘制事件完成 ==="));
}

void WinFull::mousePressEvent(QMouseEvent* event)
{
    LOG_DEBUG(MODULE_WIN, QString("鼠标按下: 位置=(%1,%2), 按钮=%3, state=%4")
              .arg(event->pos().x()).arg(event->pos().y())
              .arg(event->buttons()).arg((int)state));
              
    if (event->buttons() & Qt::LeftButton) {
        if (state < State::mask) {
            LOG_DEBUG(MODULE_WIN, "处理初始状态的鼠标按下");
            if(pixelInfo) pixelInfo->close();
            cutMask->mousePress(event);
        }
        else if (state <= State::tool) {
            LOG_DEBUG(MODULE_WIN, "处理工具状态的鼠标按下");
            if(toolMain && toolMain->isVisible()) {
                LOG_DEBUG(MODULE_WIN, "隐藏现有工具栏");
                toolMain->hide();
            }
            cutMask->mousePress(event);
        }
        else {
            LOG_DEBUG(MODULE_WIN, "处理画布状态的鼠标按下");
            canvas->mousePress(event);
        }
    }
    else {
        LOG_INFO(MODULE_WIN, "非左键按下，取消截图");
        close(); // 关闭窗口，但不退出应用程序
    }
}

void WinFull::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (state <= State::tool) {
            cutMask->mouseDrag(event);
        }
        else {
            canvas->mouseDrag(event);
        }        
    }
    else {
        if (state <= State::tool) {
            if (state == State::start) {
                pixelInfo->mouseMove(event->pos());
            }
            cutMask->mouseMove(event);
        }
        else {
            canvas->mouseMove(event);
        }
    }
}

void WinFull::mouseReleaseEvent(QMouseEvent* event)
{
    LOG_DEBUG(MODULE_WIN, QString("=== 鼠标释放事件开始 ==="));
    LOG_DEBUG(MODULE_WIN, QString("鼠标释放: 位置=(%1,%2), 按钮=%3, state=%4")
              .arg(event->pos().x()).arg(event->pos().y())
              .arg(event->buttons()).arg((int)state));
    
    if (state <= State::tool) {
        LOG_DEBUG(MODULE_WIN, "处理截图选择完成逻辑");
        
        cutMask->rectMask = cutMask->rectMask.normalized();
        LOG_DEBUG(MODULE_WIN, QString("标准化后的rectMask: (%1,%2,%3,%4)")
                  .arg(cutMask->rectMask.x()).arg(cutMask->rectMask.y())
                  .arg(cutMask->rectMask.width()).arg(cutMask->rectMask.height()));
        
        auto customCap = App::getCustomCap();
        LOG_INFO(MODULE_WIN, QString("获取customCap值: %1").arg(customCap));
        qDebug() << "WinFull: customCap =" << customCap;
        qDebug() << "WinFull: 开始判断截图后的行为";
        
        if (customCap == 0) {
            qDebug() << "WinFull: 进入保存到文件分支";
            LOG_INFO(MODULE_WIN, "customCap=0, 执行保存到文件操作");
            saveToFile();
        }
        else if (customCap == 1)
        {
            qDebug() << "WinFull: 进入保存到剪贴板分支";
            LOG_INFO(MODULE_WIN, "customCap=1, 执行保存到剪贴板操作");
            saveToClipboard();
        }
        else {
            qDebug() << "WinFull: 进入显示工具栏分支";
            LOG_INFO(MODULE_WIN, QString("customCap=%1, 显示工具栏").arg(customCap));
            state = State::tool;
            LOG_DEBUG(MODULE_WIN, QString("设置state为tool: %1").arg((int)state));
            
            if (!toolMain) {
                LOG_INFO(MODULE_WIN, "创建新的ToolMain工具栏");
                toolMain = new ToolMain(this);
                LOG_DEBUG(MODULE_WIN, QString("ToolMain创建完成，指针地址: 0x%1")
                          .arg(reinterpret_cast<quintptr>(toolMain), 0, 16));
            } else {
                LOG_DEBUG(MODULE_WIN, "ToolMain已存在，重用现有实例");
            }
            
            LOG_DEBUG(MODULE_WIN, "调用toolMain->confirmPos()确定位置");
            toolMain->confirmPos();
            
            LOG_DEBUG(MODULE_WIN, "调用toolMain->show()显示工具栏");
            toolMain->show();
            
            LOG_INFO(MODULE_WIN, "工具栏显示完成");
        }
    }
    else {
        LOG_DEBUG(MODULE_WIN, "state > State::tool, 传递给canvas处理");
        canvas->mouseRelease(event);
    }
    
    LOG_DEBUG(MODULE_WIN, QString("=== 鼠标释放事件完成 ==="));
}

void WinFull::pin()
{
    auto img = getTargetImg();
    auto pos = mapToGlobal(cutMask->rectMask.topLeft());
	new WinPin(pos, img);
    close();
}

void WinFull::saveToClipboard()
{
    LOG_INFO(MODULE_WIN, "保存截图到剪贴板");
    auto img = getTargetImg();
    Util::imgToClipboard(img);
    close(); // 关闭窗口，但不退出应用程序
}

void WinFull::saveToFile()
{
    LOG_INFO(MODULE_WIN, "保存截图到文件");
    auto img = getTargetImg();
    auto flag = Util::saveToFile(img);
    if (flag) {
        close(); // 关闭窗口，但不退出应用程序
    }
}

void WinFull::keyPressEvent(QKeyEvent* event)
{
    if (const auto key = event->key(); key == Qt::Key_Escape) {
        LOG_INFO(MODULE_WIN, "用户按下ESC键，取消截图");
        close(); // 关闭窗口，但不退出应用程序
    } else if (key == Qt::Key_Left) {
        moveCursor(QPoint(-1, 0));
    } else if (key == Qt::Key_Up) {
        moveCursor(QPoint(0, -1));
    } else if (key == Qt::Key_Right) {
        moveCursor(QPoint(1, 0));
    } else if (key == Qt::Key_Down) {
        moveCursor(QPoint(0, 1));
    } else if (key == Qt::Key_Delete) {
        canvas->removeShapeHover();
    } else if (key == Qt::Key_Backspace) {
        canvas->removeShapeHover();
    } else if (key == Qt::Key_Enter || key == Qt::Key_Return) {
        saveToClipboard();
    } else if (event->modifiers() & Qt::ControlModifier) {
        if (key == Qt::Key_Z) {
            canvas->undo();
        } else if (key == Qt::Key_Y) {
            canvas->redo();
        } else if (key == Qt::Key_S) {
            saveToFile();
        } else if (key == Qt::Key_C) {
            saveToClipboard();
        } else if (key == Qt::Key_R) {
            canvas->copyColor(0);
        } else if (key == Qt::Key_H) {
            canvas->copyColor(1);
        } else if (key == Qt::Key_K) {
            canvas->copyColor(2);
        } else if (key == Qt::Key_P) {
            canvas->copyColor(3);
        }
    }
}

void WinFull::closeEvent(QCloseEvent* event)
{
    LOG_INFO(MODULE_WIN, "WinFull窗口关闭，发射captureFinished信号");
    emit captureFinished();
    deleteLater();
}

void WinFull::initWindow()
{
    setFocusPolicy(Qt::StrongFocus);
#ifdef DEBUG
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
#else
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
#endif    
    show();
    setGeometry(x, y, w, h);
    setScreen(qApp->primaryScreen());
    auto hwnd = (HWND)winId();
    SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    SetFocus(hwnd);
}

QImage WinFull::getTargetImg()
{
    auto dpr = devicePixelRatio();
    auto tl = cutMask->rectMask.topLeft() * dpr;
    auto br = cutMask->rectMask.bottomRight() * dpr;
    QRect r;
    r.setCoords(tl.x(), tl.y(), br.x(), br.y());
    auto img = canvas->imgBg.copy(r);
    auto img2 = canvas->imgBoard.copy(r);
    QPainter p(&img);
    p.drawImage(QPoint(0, 0), img2);
	return img;
}
