#ifndef CUSTOMTABBAR_H
#define CUSTOMTABBAR_H

#include <QTabBar>
#include <QMouseEvent>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QApplication>
#include <QHash>
#include <QPixmap>

class CustomTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit CustomTabBar(QWidget *parent = nullptr);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_hoveredTab;
    int m_hoveredCloseButton; // 当前悬停的关闭按钮索引
    int m_pressedCloseButton; // 当前按下的关闭按钮索引
    QHash<int, QRect> m_closeRects; // 每个 tab 对应的关闭按钮区域
    QPixmap m_closeNormalIcon;
    QPixmap m_closeHoverIcon;
};

#endif // CUSTOMTABBAR_H
