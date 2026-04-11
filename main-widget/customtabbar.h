#ifndef CUSTOMTABBAR_H
#define CUSTOMTABBAR_H

#include <QTabBar>
#include <QMouseEvent>
#include <QHash>

class CustomTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit CustomTabBar(QWidget *parent = nullptr);

protected:
    QSize tabSizeHint(int index) const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_hoveredTab;
    int m_hoveredCloseButton;
    int m_pressedCloseButton;
    QHash<int, QRect> m_closeRects;
};

#endif // CUSTOMTABBAR_H
