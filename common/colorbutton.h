#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QPushButton>
#include <QColor>
#include <QPainter>
#include <QColorDialog>
#include <QPaintEvent>
#include <QMouseEvent>

// 颜色选择按钮
class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(const QColor &color = Qt::black, QWidget *parent = nullptr);
    
    QColor color() const { return m_color; }
    void setColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QColor m_color;
};

#endif // COLORBUTTON_H
