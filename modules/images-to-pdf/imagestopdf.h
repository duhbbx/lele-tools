#ifndef IMAGESTOPDF_H
#define IMAGESTOPDF_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QStringList>

#include "../../common/dynamicobjectbase.h"

class ImagesToPdf : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit ImagesToPdf();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddImages();
    void onRemoveSelected();
    void onClearAll();
    void onMoveUp();
    void onMoveDown();
    void onExportPdf();

private:
    void setupUI();
    void updateStatus();
    void addImageFiles(const QStringList& files);

    QListWidget* m_listWidget = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};

#endif // IMAGESTOPDF_H
