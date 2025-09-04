#pragma once

#include <QWidget>
#include <QHBoxLayout>

#include "ToolBase.h"


class BtnBase;
class BtnCheck;

class ToolMain final : public ToolBase {
    Q_OBJECT

public:
    explicit ToolMain(QWidget* parent = nullptr);
    ~ToolMain() override;
    void setBtnEnable(const QString& name, bool flag) const;
    void confirmPos();
    void btnCheckChange(BtnCheck* btn) override;
    void btnClick(Btn* btn) override;

    int btnCheckedCenterX { };

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* getTool(const QString& toolName);
    void initDefaultTool(QHBoxLayout* layout);

    bool topFlag { false };
    std::vector<BtnBase> btns;
    QList<int> splitters;
    int posState { -1 };
};
