#ifndef TINYIMAGECOMPRESS_H
#define TINYIMAGECOMPRESS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QStringList>

#include "../../common/dynamicobjectbase.h"

struct CompressItem {
    QString sourcePath;
    QString fileName;
    qint64 originalSize = 0;
    qint64 compressedSize = 0;
    int width = 0;
    int height = 0;
    bool done = false;
    bool replaced = false;
    QByteArray compressedData;
};

class TinyImageCompress : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit TinyImageCompress();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddFiles();
    void onAddFolder();
    void onRemoveSelected();
    void onClearAll();
    void onCompress();
    void onReplaceSelected();
    void onReplaceAll();
    void onQualityChanged(int value);

private:
    void setupUI();
    void addFiles(const QStringList& files);
    void compressImage(int index);
    void updateTable();
    void updateStatus();
    QString formatSize(qint64 bytes) const;

    // UI
    QPushButton* m_addFilesBtn = nullptr;
    QPushButton* m_addFolderBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_compressBtn = nullptr;
    QPushButton* m_replaceSelectedBtn = nullptr;
    QPushButton* m_replaceAllBtn = nullptr;
    QSlider* m_qualitySlider = nullptr;
    QSpinBox* m_qualitySpin = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QCheckBox* m_keepMetadataCheck = nullptr;
    QTableWidget* m_table = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_qualityLabel = nullptr;

    // Data
    QList<CompressItem> m_items;
};

#endif // TINYIMAGECOMPRESS_H
