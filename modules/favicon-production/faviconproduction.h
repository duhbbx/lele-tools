#ifndef FAVICONPRODUCTION_H
#define FAVICONPRODUCTION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QListWidget>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "../../common/dynamicobjectbase.h"

class FaviconProduction : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit FaviconProduction();
    ~FaviconProduction();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onSelectImage();
    void onClear();
    void onGenerate();
    void onOpenOutputDir();
    void onSelectOutputDir();

private:
    void setupUI();
    void loadImage(const QString& filePath);
    QPixmap generateIcon(const QPixmap& source, int targetSize);
    void addResultItem(const QString& filePath, int size);

    // Toolbar
    QPushButton* m_selectBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_generateBtn;
    QPushButton* m_openDirBtn;

    // Source preview
    QLabel* m_previewLabel;
    QLabel* m_fileNameLabel;
    QLabel* m_fileSizeLabel;

    // Settings
    QLineEdit* m_outputPathEdit;
    QPushButton* m_selectOutputBtn;
    QCheckBox* m_webCheck;
    QCheckBox* m_androidCheck;
    QCheckBox* m_iosCheck;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Result grid
    QListWidget* m_resultList;

    // Data
    QPixmap m_sourcePixmap;
    QString m_sourcePath;
    QString m_outputDir;
};

#endif // FAVICONPRODUCTION_H
