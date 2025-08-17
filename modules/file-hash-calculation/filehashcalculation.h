
#ifndef FILEHASHCALCULATION_H
#define FILEHASHCALCULATION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QSplitter>
#include <QDirIterator>
#include <QDesktopServices>

#include "../../common/dynamicobjectbase.h"

// 支持的哈希算法枚举
enum class HashAlgorithm {
    MD5,
    SHA1,
    SHA224,
    SHA256,
    SHA384,
    SHA512,
    SHA3_224,
    SHA3_256,
    SHA3_384,
    SHA3_512
};

// 哈希计算结果结构
struct HashResult {
    QString fileName;
    QString filePath;
    qint64 fileSize;
    QDateTime modifyTime;
    QString md5;
    QString sha1;
    QString sha256;
    QString sha512;
    bool isValid;
    QString errorMessage;
    
    HashResult() : fileSize(0), isValid(false) {}
};

// 哈希计算工作线程
class HashCalculationWorker : public QObject
{
    Q_OBJECT

public:
    explicit HashCalculationWorker(QObject *parent = nullptr);
    
    void setFiles(const QStringList &files);
    void setAlgorithms(const QList<HashAlgorithm> &algorithms);
    void setStopFlag(bool stop) { m_stopFlag = stop; }

public slots:
    void calculateHashes();

signals:
    void progressUpdate(int current, int total);
    void fileProcessed(const HashResult &result);
    void calculationFinished();
    void errorOccurred(const QString &error);

private:
    QString calculateFileHash(const QString &filePath, QCryptographicHash::Algorithm algorithm);
    QString getAlgorithmName(HashAlgorithm algorithm);
    QCryptographicHash::Algorithm getQtAlgorithm(HashAlgorithm algorithm);
    
    QStringList m_files;
    QList<HashAlgorithm> m_algorithms;
    bool m_stopFlag;
};

class FileHashCalculation : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit FileHashCalculation();
    ~FileHashCalculation();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onSelectFilesClicked();
    void onSelectFolderClicked();
    void onCalculateClicked();
    void onClearClicked();
    void onStopClicked();
    void onCopyResultClicked();
    void onSaveResultClicked();
    void onCompareClicked();
    void onVerifyClicked();
    
    // 算法选择
    void onAlgorithmChanged();
    
    // 工作线程回调
    void onProgressUpdate(int current, int total);
    void onFileProcessed(const HashResult &result);
    void onCalculationFinished();
    void onErrorOccurred(const QString &error);
    
    // 表格操作
    void onResultTableContextMenu(const QPoint &pos);
    void onResultTableItemClicked(QTableWidgetItem *item);

private:
    void setupUI();
    void setupFileSelectionArea();
    void setupAlgorithmArea();
    void setupControlArea();
    void setupResultArea();
    void setupComparisonArea();
    void setupStatusArea();
    
    void addFilesToList(const QStringList &files);
    void updateFileList();
    void clearResults();
    void startCalculation();
    void stopCalculation();
    void updateProgress(int current, int total);
    void addResultToTable(const HashResult &result);
    
    QStringList getSelectedAlgorithms();
    QString formatFileSize(qint64 size);
    QString formatDuration(qint64 ms);
    
    void saveResultsToFile();
    void copyResultsToClipboard();
    void compareHashValues();
    void verifyHashValue();
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    
    // 左侧面板
    QWidget *leftPanel;
    QVBoxLayout *leftLayout;
    
    // 文件选择区域
    QGroupBox *fileSelectionGroup;
    QVBoxLayout *fileSelectionLayout;
    QTextEdit *fileListEdit;
    QHBoxLayout *fileButtonLayout;
    QPushButton *selectFilesBtn;
    QPushButton *selectFolderBtn;
    QPushButton *clearFilesBtn;
    QLabel *fileCountLabel;
    
    // 算法选择区域
    QGroupBox *algorithmGroup;
    QGridLayout *algorithmLayout;
    QCheckBox *md5CheckBox;
    QCheckBox *sha1CheckBox;
    QCheckBox *sha224CheckBox;
    QCheckBox *sha256CheckBox;
    QCheckBox *sha384CheckBox;
    QCheckBox *sha512CheckBox;
    QCheckBox *sha3_224CheckBox;
    QCheckBox *sha3_256CheckBox;
    QCheckBox *sha3_384CheckBox;
    QCheckBox *sha3_512CheckBox;
    QPushButton *selectAllBtn;
    QPushButton *selectNoneBtn;
    
    // 控制区域
    QGroupBox *controlGroup;
    QHBoxLayout *controlLayout;
    QPushButton *calculateBtn;
    QPushButton *stopBtn;
    QPushButton *clearBtn;
    
    // 右侧面板
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    
    // 结果显示区域
    QGroupBox *resultGroup;
    QVBoxLayout *resultLayout;
    QTableWidget *resultTable;
    QHBoxLayout *resultButtonLayout;
    QPushButton *copyResultBtn;
    QPushButton *saveResultBtn;
    QPushButton *exportBtn;
    
    // 比较验证区域
    QGroupBox *comparisonGroup;
    QVBoxLayout *comparisonLayout;
    QLineEdit *expectedHashEdit;
    QComboBox *hashTypeCombo;
    QPushButton *compareBtn;
    QPushButton *verifyBtn;
    QLabel *comparisonResultLabel;
    
    // 状态区域
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QLabel *timeLabel;
    
    // 数据成员
    QStringList m_selectedFiles;
    QList<HashResult> m_results;
    QThread *m_workerThread;
    HashCalculationWorker *m_worker;
    QTimer *m_timer;
    QDateTime m_startTime;
    bool m_calculating;
    
    // 常量
    static const int MAX_FILES = 1000;
};

#endif // FILEHASHCALCULATION_H
