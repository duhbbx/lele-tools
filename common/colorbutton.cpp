#include "colorbutton.h"

ColorButton::ColorButton(const QColor &color, QWidget *parent)
    : QPushButton(parent), m_color(color)
{
    setFixedSize(30, 30);
    setColor(color);
}

void ColorButton::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void ColorButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect().adjusted(2, 2, -2, -2);
    painter.fillRect(rect, m_color);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(rect);
}

void ColorButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QColor color = QColorDialog::getColor(m_color, this, "选择颜色");
        if (color.isValid()) {
            setColor(color);
            emit colorChanged(color);
        }
    }
    QPushButton::mousePressEvent(event);
}

#include "colorbutton.moc"
