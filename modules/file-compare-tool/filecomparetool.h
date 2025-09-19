#ifndef FILECOMPARETOOL_H
#define FILECOMPARETOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QProgressBar>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QRegularExpression>
#include <QHeaderView>
#include <QStandardPaths>

#include "../../common/dynamicobjectbase.h"

// 差异类型枚举
enum class DiffType {
    Added,      // 新增行
    Deleted,    // 删除行
    Modified,   // 修改行
    Unchanged   // 未变更行
};

// 差异项结构
struct DiffItem {
    DiffType type;
    int leftLineNum;    // 左侧行号（-1表示不存在）
    int rightLineNum;   // 右侧行号（-1表示不存在）
    QString leftText;   // 左侧文本
    QString rightText;  // 右侧文本
    QString context;    // 上下文信息

    DiffItem(DiffType t = DiffType::Unchanged, int leftNum = -1, int rightNum = -1,
             const QString& leftTxt = "", const QString& rightTxt = "")
        : type(t), leftLineNum(leftNum), rightLineNum(rightNum), leftText(leftTxt), rightText(rightTxt) {}
};

// 文件比较结果
struct FileCompareResult {
    QString leftFile;
    QString rightFile;
    QList<DiffItem> differences;
    int addedLines;
    int deletedLines;
    int modifiedLines;
    int unchangedLines;
    double similarity;  // 相似度百分比
    QDateTime compareTime;
    QString error;

    FileCompareResult() {
        addedLines = deletedLines = modifiedLines = unchangedLines = 0;
        similarity = 0.0;
        compareTime = QDateTime::currentDateTime();
    }

    bool isValid() const {
        return error.isEmpty();
    }

    QString getSummary() const {
        if (!isValid()) return error;

        return QString("新增: %1行, 删除: %2行, 修改: %3行, 相似度: %4%")
               .arg(addedLines).arg(deletedLines).arg(modifiedLines).arg(similarity, 0, 'f', 1);
    }
};

// 目录比较结果项
struct DirCompareItem {
    QString relativePath;
    QString leftPath;
    QString rightPath;
    DiffType type;
    QString leftSize;
    QString rightSize;
    QString leftModified;
    QString rightModified;
    QString leftHash;
    QString rightHash;
    bool isDirectory;

    DirCompareItem() : type(DiffType::Unchanged), isDirectory(false) {}
};

// 文件差异算法接口
class IDiffAlgorithm {
public:
    virtual ~IDiffAlgorithm() = default;
    virtual QList<DiffItem> compare(const QStringList& left, const QStringList& right) = 0;
    virtual QString name() const = 0;
};

// Myers差异算法实现
class MyersDiffAlgorithm : public IDiffAlgorithm {
public:
    QList<DiffItem> compare(const QStringList& left, const QStringList& right) override;
    QString name() const override { return "Myers算法"; }

private:
    struct Point {
        int x, y;
        Point(int x = 0, int y = 0) : x(x), y(y) {}
    };

    QList<Point> shortestEditScript(const QStringList& left, const QStringList& right);
    QList<DiffItem> buildDiffItems(const QStringList& left, const QStringList& right, const QList<Point>& path);
};

// 简单差异算法实现
class SimpleDiffAlgorithm : public IDiffAlgorithm {
public:
    QList<DiffItem> compare(const QStringList& left, const QStringList& right) override;
    QString name() const override { return "简单算法"; }
};

// 文件比较工作线程
class FileCompareWorker : public QObject {
    Q_OBJECT

public:
    explicit FileCompareWorker(QObject* parent = nullptr);

    void compareFiles(const QString& leftFile, const QString& rightFile, IDiffAlgorithm* algorithm);
    void compareDirectories(const QString& leftDir, const QString& rightDir, bool recursive);
    void compareTexts(const QString& leftText, const QString& rightText, IDiffAlgorithm* algorithm);

signals:
    void fileCompareFinished(const FileCompareResult& result);
    void directoryCompareFinished(const QList<DirCompareItem>& items);
    void progressUpdated(int value, int maximum, const QString& message);
    void errorOccurred(const QString& error);

private slots:
    void doFileCompare();
    void doDirectoryCompare();
    void doTextCompare();

private:
    QString calculateFileHash(const QString& filePath);
    QStringList readFileLines(const QString& filePath);
    void scanDirectory(const QString& dir, QMap<QString, QFileInfo>& files, bool recursive);
    double calculateSimilarity(const QList<DiffItem>& differences);

    QString m_leftFile, m_rightFile;
    QString m_leftDir, m_rightDir;
    QString m_leftText, m_rightText;
    IDiffAlgorithm* m_algorithm;
    bool m_recursive;
    QMutex m_mutex;
    bool m_shouldStop;
};

// 文本对比编辑器
class DiffTextEditor : public QTextEdit {
    Q_OBJECT

public:
    explicit DiffTextEditor(QWidget* parent = nullptr);

    void setDiffItems(const QList<DiffItem>& items, bool isLeft = true);
    void highlightLine(int lineNumber);
    void clearHighlight();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupSyntaxHighlighting();
    QColor getLineColor(DiffType type) const;

    QList<DiffItem> m_diffItems;
    bool m_isLeftEditor;
    int m_highlightedLine;
};

// 文件内容对比组件
class FileContentCompare : public QWidget {
    Q_OBJECT

public:
    explicit FileContentCompare(QWidget* parent = nullptr);

private slots:
    void onSelectLeftFileClicked();
    void onSelectRightFileClicked();
    void onCompareClicked();
    void onClearClicked();
    void onSaveReportClicked();
    void onCopyDiffClicked();
    void onAlgorithmChanged();
    void onIgnoreWhitespaceToggled(bool ignore);
    void onIgnoreCaseToggled(bool ignore);
    void onFileCompareFinished(const FileCompareResult& result);
    void onProgressUpdated(int value, int maximum, const QString& message);
    void onCompareError(const QString& error);

private:
    void setupUI();
    void updateUI();
    void displayCompareResult(const FileCompareResult& result);
    void syncScrolling();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 文件选择区域
    QWidget* m_fileWidget;
    QVBoxLayout* m_fileLayout;
    QGroupBox* m_fileGroup;
    QGridLayout* m_fileGridLayout;

    QLabel* m_leftFileLabel;
    QLineEdit* m_leftFileEdit;
    QPushButton* m_selectLeftBtn;

    QLabel* m_rightFileLabel;
    QLineEdit* m_rightFileEdit;
    QPushButton* m_selectRightBtn;

    // 比较选项区域
    QGroupBox* m_optionsGroup;
    QFormLayout* m_optionsLayout;
    QComboBox* m_algorithmCombo;
    QCheckBox* m_ignoreWhitespaceCheck;
    QCheckBox* m_ignoreCaseCheck;

    // 控制按钮区域
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_compareBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_saveReportBtn;
    QPushButton* m_copyDiffBtn;

    // 进度显示区域
    QGroupBox* m_statusGroup;
    QVBoxLayout* m_statusLayout;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // 对比结果区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;

    // 统计信息
    QGroupBox* m_statsGroup;
    QFormLayout* m_statsLayout;
    QLabel* m_addedLabel;
    QLabel* m_deletedLabel;
    QLabel* m_modifiedLabel;
    QLabel* m_similarityLabel;

    // 差异显示区域
    QSplitter* m_diffSplitter;
    QGroupBox* m_leftGroup;
    QVBoxLayout* m_leftLayout;
    DiffTextEditor* m_leftEditor;

    QGroupBox* m_rightGroup;
    QVBoxLayout* m_rightLayout;
    DiffTextEditor* m_rightEditor;

    // 工作线程
    QThread* m_workerThread;
    FileCompareWorker* m_worker;

    // 算法实例
    MyersDiffAlgorithm m_myersAlgorithm;
    SimpleDiffAlgorithm m_simpleAlgorithm;

    // 当前结果
    FileCompareResult m_lastResult;
};

// 文本对比组件
class TextContentCompare : public QWidget {
    Q_OBJECT

public:
    explicit TextContentCompare(QWidget* parent = nullptr);

private slots:
    void onCompareClicked();
    void onClearClicked();
    void onLoadLeftFileClicked();
    void onLoadRightFileClicked();
    void onSaveLeftFileClicked();
    void onSaveRightFileClicked();
    void onTextCompareFinished(const FileCompareResult& result);

private:
    void setupUI();
    void displayCompareResult(const FileCompareResult& result);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 左侧文本区域
    QWidget* m_leftWidget;
    QVBoxLayout* m_leftLayout;
    QGroupBox* m_leftGroup;
    QVBoxLayout* m_leftGroupLayout;
    QHBoxLayout* m_leftButtonLayout;
    QPushButton* m_loadLeftBtn;
    QPushButton* m_saveLeftBtn;
    QPlainTextEdit* m_leftTextEdit;

    // 右侧文本区域
    QWidget* m_rightWidget;
    QVBoxLayout* m_rightLayout;
    QGroupBox* m_rightGroup;
    QVBoxLayout* m_rightGroupLayout;
    QHBoxLayout* m_rightButtonLayout;
    QPushButton* m_loadRightBtn;
    QPushButton* m_saveRightBtn;
    QPlainTextEdit* m_rightTextEdit;

    // 控制区域
    QWidget* m_controlWidget;
    QVBoxLayout* m_controlLayout;
    QGroupBox* m_controlGroup;
    QFormLayout* m_controlFormLayout;
    QComboBox* m_algorithmCombo;
    QPushButton* m_compareBtn;
    QPushButton* m_clearBtn;

    // 结果显示区域
    QGroupBox* m_resultGroup;
    QVBoxLayout* m_resultLayout;
    QTableWidget* m_diffTable;

    // 工作线程
    QThread* m_workerThread;
    FileCompareWorker* m_worker;

    // 算法实例
    MyersDiffAlgorithm m_myersAlgorithm;
    SimpleDiffAlgorithm m_simpleAlgorithm;
};

// 目录对比组件
class DirectoryCompare : public QWidget {
    Q_OBJECT

public:
    explicit DirectoryCompare(QWidget* parent = nullptr);

private slots:
    void onSelectLeftDirClicked();
    void onSelectRightDirClicked();
    void onCompareClicked();
    void onClearClicked();
    void onExportReportClicked();
    void onDirectoryCompareFinished(const QList<DirCompareItem>& items);
    void onProgressUpdated(int value, int maximum, const QString& message);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void updateUI();
    void displayCompareResult(const QList<DirCompareItem>& items);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 目录选择区域
    QWidget* m_dirWidget;
    QVBoxLayout* m_dirLayout;
    QGroupBox* m_dirGroup;
    QGridLayout* m_dirGridLayout;

    QLabel* m_leftDirLabel;
    QLineEdit* m_leftDirEdit;
    QPushButton* m_selectLeftDirBtn;

    QLabel* m_rightDirLabel;
    QLineEdit* m_rightDirEdit;
    QPushButton* m_selectRightDirBtn;

    // 比较选项区域
    QGroupBox* m_optionsGroup;
    QVBoxLayout* m_optionsLayout;
    QCheckBox* m_recursiveCheck;
    QCheckBox* m_compareContentCheck;
    QCheckBox* m_ignoreHiddenCheck;

    // 控制按钮区域
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_compareBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;

    // 进度显示区域
    QGroupBox* m_statusGroup;
    QVBoxLayout* m_statusLayout;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // 结果显示区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;
    QLabel* m_resultLabel;
    QTreeWidget* m_resultTree;

    // 统计信息
    QGroupBox* m_statsGroup;
    QFormLayout* m_statsLayout;
    QLabel* m_totalFilesLabel;
    QLabel* m_addedFilesLabel;
    QLabel* m_deletedFilesLabel;
    QLabel* m_modifiedFilesLabel;
    QLabel* m_identicalFilesLabel;

    // 工作线程
    QThread* m_workerThread;
    FileCompareWorker* m_worker;

    // 当前结果
    QList<DirCompareItem> m_lastResult;
};

// 文件对比工具主类
class FileCompareTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit FileCompareTool(QWidget* parent = nullptr);
    ~FileCompareTool() override = default;

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    FileContentCompare* m_fileCompare;
    TextContentCompare* m_textCompare;
    DirectoryCompare* m_directoryCompare;
};

#endif // FILECOMPARETOOL_H