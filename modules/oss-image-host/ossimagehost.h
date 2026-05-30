#ifndef OSSIMAGEHOST_H
#define OSSIMAGEHOST_H

#include <QWidget>

#include "../../common/dynamicobjectbase.h"

class QPushButton;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QNetworkAccessManager;
class QNetworkReply;

namespace SqliteWrapper { class SqliteManager; }

class OssImageHost : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit OssImageHost();
    ~OssImageHost() override;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onConfigure();
    void onUploadFile();
    void onPasteImage();
    void onCopyUrl();
    void onCopyMd();
    void onCopyHtml();
    void onCopyBbcode();
    void onListItemActivated();
    void onDeleteRecord();
    void onOpenInBrowser();
    void onRefreshList();
    void onFetchBucket();      // 从 OSS 拉取所有对象

private:
    void setupUI();
    void initDb();
    void loadHistory();
    void loadConfig();
    void saveConfig();

    void uploadFile(const QString& localPath);
    void uploadBytes(const QByteArray& data, const QString& mime, const QString& origName);
    void onUploadFinished(QNetworkReply* reply,
                          const QString& key,
                          const QString& publicUrl,
                          const QString& mime,
                          int width, int height,
                          qint64 size,
                          const QString& origName);

    void updateOutputs(const QString& url, const QString& nameForAlt);
    void clearOutputs();
    void loadThumbnail(const QString& remoteKey, const QString& fullUrl);
    void populateFromBucket(const QByteArray& listXml);
    void fetchBucketPage(const QString& marker);     // 分页拉取
    void appendBucketObjectsToList();                 // 把累积的 objects 落到 UI

    // ── 配置（保存到 QSettings 的 OssImageHost 分组下）
    QString m_endpoint;         // 如 oss-cn-hangzhou.aliyuncs.com
    QString m_bucket;
    QString m_accessKey;
    QString m_secretKey;
    QString m_customDomain;     // 可选，如 https://img.example.com
    QString m_pathPrefix;       // 如 images/
    bool    m_useHttps = true;

    // ── UI
    QPushButton* m_configBtn = nullptr;
    QPushButton* m_uploadBtn = nullptr;
    QPushButton* m_pasteBtn  = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_fetchBucketBtn = nullptr;
    QLabel*      m_statusLabel = nullptr;

    QListWidget* m_historyList = nullptr;
    QLabel*      m_previewLabel = nullptr;
    QLabel*      m_metaLabel  = nullptr;

    QLineEdit*   m_urlEdit  = nullptr;
    QLineEdit*   m_mdEdit   = nullptr;
    QLineEdit*   m_htmlEdit = nullptr;
    QLineEdit*   m_bbEdit   = nullptr;

    QPushButton* m_copyUrlBtn = nullptr;
    QPushButton* m_copyMdBtn  = nullptr;
    QPushButton* m_copyHtmlBtn = nullptr;
    QPushButton* m_copyBbBtn  = nullptr;
    QPushButton* m_openBrowserBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;

    QNetworkAccessManager*           m_net = nullptr;
    SqliteWrapper::SqliteManager*    m_db  = nullptr;
};

#endif // OSSIMAGEHOST_H
