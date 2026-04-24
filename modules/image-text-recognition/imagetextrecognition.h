#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QTextEdit>
#include <QProcess>
#include <QList>
#include <QPixmap>
#include "../../common/dynamicobjectbase.h"

struct OcrItem {
    QString imagePath;
    QPixmap pixmap;
    QString resultText;
    bool recognized = false;
};

class ImageTextRecognition : public QWidget, public DynamicObjectBase {
    Q_OBJECT
public:
    explicit ImageTextRecognition();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddImages();
    void onPasteFromClipboard();
    void onClearAll();
    void onRecognizeAll();
    void onCopyAll();
    void onCopyAllNoSeparator();

private:
    void setupUI();
    void addImageFiles(const QStringList& files);
    void addImageFromPixmap(const QPixmap& pixmap);
    void rebuildResultsView();
    QString recognizeImage(const QString& imagePath);

    // OCR 引擎
    enum OcrEngine { Auto, PaddleOCR, Tesseract };
    bool checkPaddleOcr();
    bool checkTesseract();
    QString recognizeWithPaddle(const QString& imagePath);
    QString recognizeWithTesseract(const QString& imagePath);
    void onConfigEngine();
    void saveEngineSettings();
    void loadEngineSettings();

    // UI
    QPushButton* m_addBtn;
    QPushButton* m_pasteBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_recognizeBtn;
    QPushButton* m_copyAllBtn;
    QPushButton* m_copyNoSepBtn;
    QPushButton* m_configBtn;
    QComboBox* m_langCombo;
    QComboBox* m_engineCombo;
    QScrollArea* m_scrollArea;
    QWidget* m_resultsWidget;
    QVBoxLayout* m_resultsLayout;
    QLabel* m_statusLabel;

    QList<OcrItem> m_items;

    // 引擎配置
    QString m_paddleOcrPath; // paddleocr 可执行文件路径
    OcrEngine m_currentEngine = Auto;
};
