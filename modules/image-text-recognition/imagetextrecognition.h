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
#include <QElapsedTimer>
#include "../../common/dynamicobjectbase.h"

struct OcrItem {
    QString imagePath;
    QPixmap pixmap;
    QString resultText;
    QString engineUsed; // "PaddleOCR" 或 "Tesseract"
    bool recognized = false;
    qint64 elapsedMs = 0; // 识别耗时（毫秒）
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
    void showInstallDialog();
    void recognizeSingle(int itemIdx);
    void startPaddleBatch(const QList<int>& indexes, bool hasTess, int engineSel);
    void processNextPaddleCli(int idx, const QList<int>& indexes, bool hasTess, int engineSel);
    void processNextTesseract(int idx, const QList<int>& indexes);
    QString parsePaddleJsonResult(const QString& jsonDir, const QString& jsonFileName = {});
    QString buildLayoutText(const QJsonArray& texts, const QJsonArray& polys);
    void saveEngineSettings();
    void loadEngineSettings();

    // PaddleOCR 常驻服务
    void startPaddleService();
    void stopPaddleService();
    void onServiceReadyRead();
    void onServiceFinished(int exitCode, QProcess::ExitStatus status);
    void recognizeViaService(const QList<int>& indexes, bool hasTess, int engineSel);

    // UI
    QPushButton* m_addBtn;
    QPushButton* m_pasteBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_recognizeBtn;
    QPushButton* m_copyAllBtn;
    QPushButton* m_copyNoSepBtn;
    QPushButton* m_configBtn;
    QPushButton* m_serviceBtn;
    QComboBox* m_langCombo;
    QComboBox* m_engineCombo;
    QComboBox* m_serviceModeCombo;
    QScrollArea* m_scrollArea;
    QWidget* m_resultsWidget;
    QVBoxLayout* m_resultsLayout;
    QLabel* m_statusLabel;
    QLabel* m_serviceStatusLabel;

    QList<OcrItem> m_items;

    // 引擎配置
    QString m_paddleOcrPath; // paddleocr 可执行文件路径
    OcrEngine m_currentEngine = Auto;

    // PaddleOCR 服务
    QProcess* m_paddleService = nullptr;
    bool m_serviceReady = false;
    QByteArray m_serviceBuffer;
    // 服务逐张识别队列
    QString m_pendingRequestId;
    QList<int> m_serviceQueue;       // 待识别的 item 索引队列
    int m_serviceQueuePos = 0;       // 当前处理位置
    bool m_pendingHasTess = false;
    int m_pendingEngineSel = 0;
    QElapsedTimer m_serviceTimer;    // 当前图片计时
    void serviceSendNext();          // 发送队列中下一张
};
