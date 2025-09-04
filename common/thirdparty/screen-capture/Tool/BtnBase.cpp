#include <QToolTip>
#include <utility>
#include "App/Lang.h"
#include "BtnBase.h"

BtnBase::BtnBase(QString name, const QChar& icon, QWidget* parent) : QWidget(parent),
                                                                     icon { icon }, name { std::move(name) } {
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(32, 28);
}

BtnBase::~BtnBase() = default;

void BtnBase::enterEvent(QEnterEvent* event) {
    if (!isHover) {
        const auto tip = Lang::get(name);
        QToolTip::showText(QCursor::pos(), tip, this);
        isHover = true;
        update();
    }
    QWidget::enterEvent(event);
}

void BtnBase::leaveEvent(QEvent* event) {
    if (isHover) {
        QToolTip::hideText();
        isHover = false;
        update();
    }
    QWidget::leaveEvent(event);
}
