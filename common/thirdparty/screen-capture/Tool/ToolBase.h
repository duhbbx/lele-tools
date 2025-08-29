#pragma once

#include <QWidget>

class BtnCheck;
class Btn;

class ToolBase : public QWidget {
    Q_OBJECT

public:
    explicit ToolBase(QWidget* parent = nullptr);
    ~ToolBase() override;
    virtual void btnCheckChange(BtnCheck* btn);
    virtual void btnClick(Btn* btn);

    int selectIndex { -1 };

protected:
    void initWindow();
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    int hoverIndex { -1 };
    qreal btnW { 32 };
    qreal border { 0.8 };
};
