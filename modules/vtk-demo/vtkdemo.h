#ifndef VTKDEMO_H
#define VTKDEMO_H

#include <QWidget>

#include "../../common/dynamicobjectbase.h"

class QPushButton;
class QLabel;
class QPlainTextEdit;

class VtkDemo : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit VtkDemo();
    ~VtkDemo() override;

private slots:
    void showPointCloud();
    void showSurface();
    void showHeatmap();
    void showSlice();
    void showModelCompare();

private:
    void setupUI();
    void setTitle(const QString& title, const QString& note);

    QLabel* m_titleLabel = nullptr;
    QLabel* m_noteLabel  = nullptr;
    QPushButton* m_btnPointCloud = nullptr;
    QPushButton* m_btnSurface    = nullptr;
    QPushButton* m_btnHeatmap    = nullptr;
    QPushButton* m_btnSlice      = nullptr;
    QPushButton* m_btnCompare    = nullptr;

    // 用 PIMPL 把 VTK 头隔离在 cpp 里
    struct Impl;
    Impl* m_impl = nullptr;
};

#endif // VTKDEMO_H
