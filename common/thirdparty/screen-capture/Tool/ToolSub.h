#pragma once
#include <QWidget>
#include "ToolBase.h"

class StrokeCtrl;
class ColorCtrl;

class ToolSub final : public ToolBase {
    Q_OBJECT

public:
    explicit ToolSub(QWidget* parent = nullptr);
    ~ToolSub() override;
    bool getSelectState(const QString& btnName) const;
    QColor getColor() const;
    int getStrokeWidth() const;

public:
protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initPos();
    StrokeCtrl* strokeCtrl { };
    ColorCtrl* colorCtrl { };
    int triangleX { 0 };
};
