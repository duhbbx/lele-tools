#ifndef TORRENTFILEANALYSIS_H
#define TORRENTFILEANALYSIS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QScrollArea>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QFrame>

#include "../../common/dynamicobjectbase.h"

// Bencode值类型
enum class BencodeType {
    String,
    Integer,
    List,
    Dictionary
};

// Bencode值结构
struct BencodeValue {
    BencodeType type;
    QByteArray stringValue;
    qint64 intValue;
    QList<BencodeValue> listValue;
    QMap<QByteArray, BencodeValue> dictValue;
    
    BencodeValue() : type(BencodeType::String), intValue(0) {}
    explicit BencodeValue(const QByteArray &str) : type(BencodeType::String), stringValue(str), intValue(0) {}
    explicit BencodeValue(qint64 num) : type(BencodeType::Integer), intValue(num) {}
    explicit BencodeValue(const QList<BencodeValue> &list) : type(BencodeType::List), listValue(list), intValue(0) {}
    explicit BencodeValue(const QMap<QByteArray, BencodeValue> &dict) : type(BencodeType::Dictionary), dictValue(dict), intValue(0) {}
};

// 种子文件信息结构
struct TorrentInfo {
    QString name;
    QString comment;
    QString createdBy;
    QDateTime creationDate;
    QString announce;
    QStringList announceList;
    QByteArray infoHash;
    QString magnetLink;
    qint64 totalSize;
    qint64 pieceLength;
    int pieceCount;
    QStringList files;
    QList<qint64> fileSizes;
    bool isPrivate;
    QString encoding;
    
    TorrentInfo() : totalSize(0), pieceLength(0), pieceCount(0), isPrivate(false) {}
};

// 文件项结构
struct FileItem {
    QString path;
    qint64 size;
    QString sizeString;
    double percentage;
    
    FileItem() : size(0), percentage(0.0) {}
    FileItem(const QString &p, qint64 s, const QString &ss, double pct) 
        : path(p), size(s), sizeString(ss), percentage(pct) {}
};

// Bencode解码器
class BencodeDecoder {
public:
    static BencodeValue decode(const QByteArray &data);
    static QString valueToString(const BencodeValue &value, int indent = 0);
    
private:
    static BencodeValue decodeValue(const QByteArray &data, int &pos);
    static BencodeValue decodeString(const QByteArray &data, int &pos);
    static BencodeValue decodeInteger(const QByteArray &data, int &pos);
    static BencodeValue decodeList(const QByteArray &data, int &pos);
    static BencodeValue decodeDictionary(const QByteArray &data, int &pos);
};

// 种子文件分析器主类
class TorrentFileAnalysis : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit TorrentFileAnalysis();
    ~TorrentFileAnalysis();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onOpenFile();
    void onClearAll();
    void onCopyMagnetLink();
    void onCopyInfoHash();
    void onExportFileList();
    void onSaveRawData();
    void onAnalyzeTorrent();
    void onFileItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onShowFileDetails();

private:
    void setupUI();
    void setupFileArea();
    void setupInfoArea();
    void setupFileListArea();
    void setupRawDataArea();
    void setupStatusArea();
    
    void analyzeTorrentFile(const QString &filePath);
    TorrentInfo parseTorrentData(const BencodeValue &root);
    void updateInfoDisplay(const TorrentInfo &info);
    void updateFileList(const TorrentInfo &info);
    void updateRawData(const BencodeValue &root);
    void clearAllData();
    
    QString formatFileSize(qint64 bytes) const;
    QString formatDateTime(const QDateTime &dateTime) const;
    QByteArray calculateInfoHash(const BencodeValue &infoDict);
    QString generateMagnetLink(const QByteArray &infoHash, const QString &name);
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    QTabWidget *tabWidget;
    
    // 文件选择区域
    QGroupBox *fileGroup;
    QHBoxLayout *fileLayout;
    QLineEdit *filePathEdit;
    QPushButton *openFileBtn;
    QPushButton *clearBtn;
    QLabel *dropHintLabel;
    
    // 种子信息区域
    QWidget *infoWidget;
    QScrollArea *infoScrollArea;
    QGroupBox *basicInfoGroup;
    QGridLayout *basicInfoLayout;
    QLabel *nameLabel;
    QLabel *commentLabel;
    QLabel *createdByLabel;
    QLabel *creationDateLabel;
    QLabel *announceLabel;
    QLabel *infoHashLabel;
    QLabel *totalSizeLabel;
    QLabel *pieceLengthLabel;
    QLabel *pieceCountLabel;
    QLabel *fileCountLabel;
    QLabel *isPrivateLabel;
    QLabel *encodingLabel;
    
    QGroupBox *magnetGroup;
    QVBoxLayout *magnetLayout;
    QTextEdit *magnetLinkEdit;
    QHBoxLayout *magnetButtonLayout;
    QPushButton *copyMagnetBtn;
    QPushButton *copyHashBtn;
    
    QGroupBox *announceGroup;
    QVBoxLayout *announceLayout;
    QTextEdit *announceListEdit;
    
    // 文件列表区域
    QWidget *fileListWidget;
    QVBoxLayout *fileListLayout;
    QGroupBox *fileListGroup;
    QTreeWidget *fileTree;
    QHBoxLayout *fileButtonLayout;
    QPushButton *exportFileListBtn;
    QPushButton *showDetailsBtn;
    QLabel *fileStatsLabel;
    
    // 原始数据区域
    QWidget *rawDataWidget;
    QVBoxLayout *rawDataLayout;
    QGroupBox *rawDataGroup;
    QPlainTextEdit *rawDataEdit;
    QHBoxLayout *rawButtonLayout;
    QPushButton *saveRawBtn;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QString m_currentFilePath;
    TorrentInfo m_currentTorrent;
    BencodeValue m_rawData;
    QList<FileItem> m_fileItems;
    bool m_hasValidTorrent;
    
    QTimer *m_statusTimer;
};

#endif // TORRENTFILEANALYSIS_H