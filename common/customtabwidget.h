#ifndef CUSTOMTABWIDGET_H
#define CUSTOMTABWIDGET_H

#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTimer>
#include <QHoverEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>

class HoverTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit HoverTabBar(QWidget *parent = nullptr);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void tabLayoutChange() override;

private slots:
    void updateCloseButtons();
    
private:
    void showCloseButtonForTab(int index);
    void hideAllCloseButtons();
    int tabAt(const QPoint &pos) const;
    
    int m_hoveredTab;
    QTimer *m_updateTimer;
};

class CustomTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit CustomTabWidget(QWidget *parent = nullptr);
    
private:
    HoverTabBar *m_customTabBar;
};

#endif // CUSTOMTABWIDGET_H
