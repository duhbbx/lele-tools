#include <tuple>
#include <QToolTip>
#include <QPainterPath>

#include "../App/App.h"
#include "../App/Logger.h"
#include "../Win/WinBase.h"
#include "ToolSub.h"
#include "ToolMain.h"
#include "StrokeCtrl.h"
#include "ColorCtrl.h"
#include "BtnBase.h"
#include "BtnCheck.h"

ToolSub::ToolSub(QWidget* parent) : ToolBase(parent) {
    qDebug() << "=== ToolSub构造函数开始 ===";
    qDebug() << "ToolSub: this指针:" << this;
    qDebug() << "ToolSub: parent指针:" << parent;
    
    LOG_DEBUG("ToolSub", "=== ToolSub构造函数开始 ===");
    LOG_DEBUG("ToolSub", QString("ToolSub指针: 0x%1").arg(reinterpret_cast<quintptr>(this), 0, 16));
    LOG_DEBUG("ToolSub", QString("父窗口指针: 0x%1").arg(reinterpret_cast<quintptr>(parent), 0, 16));

    qDebug() << "ToolSub: 准备转换父窗口指针";
    const auto win = dynamic_cast<WinBase*>(parent);
    qDebug() << "ToolSub: dynamic_cast结果:" << win;
    
    if (win) {
        qDebug() << "ToolSub: 父窗口转换成功，状态:" << (int)win->state;
        LOG_DEBUG("ToolSub", QString("父窗口状态: %1").arg((int)win->state));
        LOG_DEBUG("ToolSub", QString("父窗口当前toolSub: 0x%1")
                  .arg(reinterpret_cast<quintptr>(win->toolSub), 0, 16));
    } else {
        qDebug() << "ToolSub: 父窗口转换失败！parent类型:" << (parent ? parent->metaObject()->className() : "nullptr");
        LOG_ERROR("ToolSub", "父窗口转换失败");
        return;
    }

    LOG_DEBUG("ToolSub", "调用initWindow()");
    initWindow();

    LOG_DEBUG("ToolSub", "创建布局");
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(4, 4, 4, 2);

    LOG_DEBUG("ToolSub", QString("根据窗口状态 %1 创建相应的工具控件").arg((int)win->state));
    if (win->state == State::rect) {
        LOG_DEBUG("ToolSub", "创建矩形工具控件");
        layout->addWidget(new BtnCheck("rectFill", QChar(0xe602), this));
        layout->addWidget(new StrokeCtrl(1, 20, 2, this));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW + 84 + 26 * 8, 40);
        auto x = win->toolMain->x();
        auto y = win->toolMain->y() + win->toolMain->height() + 2;
        move(x, y);
        initPos();
    } else if (win->state == State::ellipse) {
        layout->addWidget(new BtnCheck("ellipseFill", QChar(0xe600), this));
        layout->addWidget(new StrokeCtrl(1, 20, 2, this));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW + 84 + 26 * 8, 40);
        initPos();
    } else if (win->state == State::arrow) {
        layout->addWidget(new BtnCheck("arrowFill", QChar(0xe604), this, State::noState, true));
        layout->addWidget(new StrokeCtrl(12, 60, 18, this));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW + 84 + 26 * 8, 40);
        initPos();
    } else if (win->state == State::number) {
        layout->addWidget(new BtnCheck("numberFill", QChar(0xe605), this, State::noState, true));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW + 26 * 8, 40);
        initPos();
    } else if (win->state == State::line) {
        layout->addWidget(new BtnCheck("lineTransparent", QChar(0xe607), this));
        layout->addWidget(new StrokeCtrl(1, 160, 6, this));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW + 84 + 26 * 8, 40);
        initPos();
    } else if (win->state == State::text) {
        layout->addWidget(new BtnCheck("bold", QChar(0xe634), this));
        layout->addWidget(new BtnCheck("italic", QChar(0xe682), this));
        layout->addWidget(new StrokeCtrl(12, 160, 22, this));
        layout->addWidget(new ColorCtrl(0, this));
        setFixedSize(8 + btnW * 2 + 84 + 26 * 8, 40);
        initPos();
    } else if (win->state == State::mosaic) {
        layout->addWidget(new BtnCheck("mosaicFill", QChar(0xe602), this));
        layout->addWidget(new StrokeCtrl(16, 80, 26, this));
        auto w { 8 + btnW + 84 };
        setFixedSize(w, 40);
        initPos();
    } else if (win->state == State::eraser) {
        layout->addWidget(new BtnCheck("eraserFill", QChar(0xe602), this));
        layout->addWidget(new StrokeCtrl(12, 60, 16, this));
        auto w { 8 + btnW + 84 };
        setFixedSize(w, 40);
        initPos();
    }
    setLayout(layout);
    show();

    LOG_DEBUG("ToolSub", "=== ToolSub构造函数完成 ===");
}

void ToolSub::initPos() {
    const auto win = dynamic_cast<WinBase*>(parent());
    int x { win->toolMain->x() }, y { win->toolMain->y() + win->toolMain->height() + 2 };
    if (win->toolMain->btnCheckedCenterX > width()) { //向上箭头不足以出现在子工具条上方时，调整子工具条的位置
        x = win->toolMain->x() + win->toolMain->btnCheckedCenterX - width() / 2;
        triangleX = width() / 2;
    } else {
        triangleX = win->toolMain->btnCheckedCenterX;
    }
    move(x, y);
}

ToolSub::~ToolSub() = default;


bool ToolSub::getSelectState(const QString& btnName) const {
    for (auto btns = findChildren<BtnCheck*>(); auto& btn : btns) {
        if (btn->name == btnName) {
            return btn->isChecked;
        }
    }
    return false;
}

QColor ToolSub::getColor() const {
    auto colorCtrl = findChild<ColorCtrl*>();
    return colorCtrl->getColor();
}

int ToolSub::getStrokeWidth() const {
    auto strokeCtrl = findChild<StrokeCtrl*>();
    return strokeCtrl->value();
}

void ToolSub::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.moveTo(border, 5);
    path.lineTo(triangleX - 4, 5);
    path.lineTo(triangleX, border);
    path.lineTo(triangleX + 4, 5);
    path.lineTo(width() - border, 5);
    path.lineTo(width() - border, height() - 4);
    path.lineTo(border, height() - 4);
    path.lineTo(border, 5);
    p.setPen(QColor(22, 118, 255));
    p.setBrush(Qt::white);
    p.drawPath(path); //有个小三角形指向主工具条的按钮
}

void ToolSub::closeEvent(QCloseEvent* event) {
    LOG_DEBUG("ToolSub", "=== ToolSub::closeEvent 开始 ===");
    LOG_DEBUG("ToolSub", QString("ToolSub指针: 0x%1").arg(reinterpret_cast<quintptr>(this), 0, 16));

    if (auto win = dynamic_cast<WinBase*>(parent())) {
        LOG_DEBUG("ToolSub", QString("父窗口指针: 0x%1").arg(reinterpret_cast<quintptr>(win), 0, 16));
        LOG_DEBUG("ToolMain", QString("当前win->toolSub: 0x%1").arg(reinterpret_cast<quintptr>(win->toolSub), 0, 16));

        if (win->toolSub == this) {
            LOG_DEBUG("ToolSub", "清空父窗口的toolSub指针");
            win->toolSub = nullptr;
        } else {
            LOG_WARNING("ToolSub", "父窗口的toolSub指针与当前对象不匹配，不清空");
        }
    } else {
        LOG_ERROR("ToolSub", "父窗口指针为空");
    }

    LOG_DEBUG("ToolSub", "调用deleteLater()");
    deleteLater();

    LOG_DEBUG("ToolSub", "=== ToolSub::closeEvent 完成 ===");
}
