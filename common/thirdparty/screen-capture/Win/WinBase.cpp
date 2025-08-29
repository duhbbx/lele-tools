#include "WinBase.h"
#include "App/Logger.h"

#include "Shape/ShapeBase.h"
#include "Tool/ToolMain.h"

#include "App/Util.h"
#include "Canvas.h"

WinBase::WinBase(QWidget* parent) : QMainWindow(parent), x(0), y(0), w(0), h(0), toolSub(nullptr), canvas(nullptr) {
    // 初始化状态为开始状态
    state = State::start;

    // 重要：初始化工具栏指针为nullptr，避免野指针
    toolMain = nullptr;

    LOG_DEBUG(MODULE_WIN, QString("WinBase构造函数: 初始化state=%1").arg((int)state));
    LOG_DEBUG(MODULE_WIN, "toolMain已初始化为nullptr");
}

WinBase::~WinBase() {
    LOG_DEBUG(MODULE_WIN, "=== WinBase析构函数开始 ===");

    // 安全清理工具栏
    if (toolMain) {
        LOG_DEBUG(MODULE_WIN, "清理toolMain指针");
        toolMain->close();
        toolMain = nullptr;
    }

    LOG_DEBUG(MODULE_WIN, "=== WinBase析构函数完成 ===");
}

void WinBase::saveToClipboard() {
    auto img = getTargetImg();
    Util::imgToClipboard(img);
    close();
    // 移除qApp->exit()调用，避免退出整个应用程序
}

void WinBase::saveToFile() {
    auto img = getTargetImg();
    auto flag = Util::saveToFile(img);
    if (flag) {
        close();
        // 移除qApp->exit()调用，避免退出整个应用程序
    }
}

void WinBase::keyPressEvent(QKeyEvent* event) {
    if (const auto key = event->key(); key == Qt::Key_Escape) {
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

void WinBase::moveCursor(const QPoint& pos) {
    const auto pos1 = QCursor::pos() + pos;
    QCursor::setPos(pos1);
}

void WinBase::mouseDoubleClickEvent(QMouseEvent* event) {
    saveToClipboard();
}
