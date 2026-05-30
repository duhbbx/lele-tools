#include "ossimagehost.h"
#include "../../common/sqlite/SqliteManager.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMessageAuthenticationCode>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRandomGenerator>
#include <QPointer>
#include <QSettings>
#include <QSplitter>
#include <QXmlStreamReader>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

REGISTER_DYNAMICOBJECT(OssImageHost);

using SqliteWrapper::SqliteManager;

// ─────────────────────────────────────────────────────────────
// 阿里云 OSS 签名 v1（HMAC-SHA1）
// 文档：https://help.aliyun.com/zh/oss/developer-reference/include-signatures-in-the-authorization-header
//
// Authorization: OSS <AccessKeyId>:<Signature>
// Signature = Base64(HMAC-SHA1(SecretKey,
//                              HTTP-Verb + "\n" +
//                              Content-MD5 + "\n" +
//                              Content-Type + "\n" +
//                              Date(GMT) + "\n" +
//                              CanonicalizedOSSHeaders +     ← 空（无 x-oss-* 自定义头）
//                              CanonicalizedResource))
// CanonicalizedResource = /<bucket>/<key>
// ─────────────────────────────────────────────────────────────

namespace {

QString rfc1123GmtNow()
{
    // Tue, 17 May 2026 12:00:00 GMT — 必须英文 locale
    QLocale en(QLocale::English);
    return en.toString(QDateTime::currentDateTimeUtc(),
                       "ddd, dd MMM yyyy HH:mm:ss") + " GMT";
}

QByteArray hmacSha1(const QByteArray& key, const QByteArray& data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha1);
}

QByteArray md5Base64(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5).toBase64();
}

QString aliyunSign(const QString& secretKey,
                   const QString& method,
                   const QString& contentMd5,
                   const QString& contentType,
                   const QString& date,
                   const QString& canonicalResource)
{
    QString stringToSign = method + "\n"
                         + contentMd5 + "\n"
                         + contentType + "\n"
                         + date + "\n"
                         + canonicalResource;
    QByteArray sig = hmacSha1(secretKey.toUtf8(), stringToSign.toUtf8()).toBase64();
    return QString::fromUtf8(sig);
}

QString mimeFromName(const QString& name)
{
    QMimeDatabase db;
    QMimeType t = db.mimeTypeForFile(name);
    QString s = t.name();
    if (s.isEmpty() || s == "application/octet-stream") s = "image/png";
    return s;
}

QString extForMime(const QString& mime)
{
    if (mime.endsWith("jpeg") || mime.endsWith("jpg")) return "jpg";
    if (mime.endsWith("png"))  return "png";
    if (mime.endsWith("webp")) return "webp";
    if (mime.endsWith("gif"))  return "gif";
    if (mime.endsWith("svg+xml")) return "svg";
    if (mime.endsWith("bmp"))  return "bmp";
    return "bin";
}

QString genObjectKey(const QString& pathPrefix, const QString& origName, const QString& mime)
{
    QString prefix = pathPrefix;
    if (!prefix.isEmpty() && !prefix.endsWith('/')) prefix += '/';
    QDateTime now = QDateTime::currentDateTime();
    QString datePart = now.toString("yyyy/MM/dd");
    QString stamp = now.toString("HHmmsszzz");
    QString rand = QString::number(quint64(QRandomGenerator::global()->generate64()), 16).left(6);
    QString ext = QFileInfo(origName).suffix().toLower();
    if (ext.isEmpty()) ext = extForMime(mime);
    return prefix + datePart + "/" + stamp + "_" + rand + "." + ext;
}

QString readableSize(qint64 b)
{
    if (b < 1024) return QString("%1 B").arg(b);
    if (b < 1024 * 1024) return QString("%1 KB").arg(b / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(b / 1024.0 / 1024.0, 0, 'f', 2);
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// Constructor / lifecycle
// ─────────────────────────────────────────────────────────────

OssImageHost::OssImageHost() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    m_net = new QNetworkAccessManager(this);
    setupUI();
    initDb();
    loadConfig();
    loadHistory();
    clearOutputs();
}

OssImageHost::~OssImageHost() = default;

// ─────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────

void OssImageHost::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QPlainTextEdit { border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: #228be6; }
        QPushButton { border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #fff; min-height: 22px; color: #343a40; }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton#primaryBtn { background: #228be6; border-color: #1c7ed6; color: white; font-weight: bold; }
        QPushButton#primaryBtn:hover { background: #1c7ed6; }
        QPushButton#dangerBtn { color: #c92a2a; }
        QPushButton#dangerBtn:hover { background: #fff5f5; }
        QListWidget { border: 1px solid #dee2e6; border-radius: 4px; background: #fff; }
        QLabel { background: transparent; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(10, 8, 10, 8);
    main->setSpacing(8);

    // 顶部工具条
    auto* topRow = new QHBoxLayout();
    m_configBtn  = new QPushButton(tr("⚙ 配置"));
    m_uploadBtn  = new QPushButton(tr("📤 上传文件"));
    m_uploadBtn->setObjectName("primaryBtn");
    m_pasteBtn   = new QPushButton(tr("📋 粘贴上传"));
    m_refreshBtn = new QPushButton(tr("🔄 本地列表"));
    m_refreshBtn->setToolTip(tr("显示本地 SQLite 上传历史（只是通过本工具上传的）"));
    m_fetchBucketBtn = new QPushButton(tr("🌐 拉取 Bucket"));
    m_fetchBucketBtn->setToolTip(tr("从 OSS 远端列出 bucket 下所有图片对象（带远端缩略图）"));
    m_statusLabel = new QLabel(tr("尚未配置，请点「⚙ 配置」"));
    m_statusLabel->setStyleSheet("color: #868e96; font-size:9pt; padding:0 8px;");
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topRow->addWidget(m_configBtn);
    topRow->addWidget(m_uploadBtn);
    topRow->addWidget(m_pasteBtn);
    topRow->addWidget(m_refreshBtn);
    topRow->addWidget(m_fetchBucketBtn);
    topRow->addWidget(m_statusLabel, 1);
    main->addLayout(topRow);

    // 主分割：左历史 | 右预览 + 输出
    auto* split = new QSplitter(Qt::Horizontal);

    // 左：历史
    m_historyList = new QListWidget();
    m_historyList->setIconSize(QSize(48, 48));
    m_historyList->setMinimumWidth(280);
    split->addWidget(m_historyList);

    // 右：预览 + 元信息 + 链接
    auto* right = new QWidget();
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(6);

    m_previewLabel = new QLabel(tr("（拖拽图片到此 / 点上传 / Ctrl+V 粘贴）"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(180);
    m_previewLabel->setStyleSheet(
        "background:#f8f9fa; border:1px dashed #adb5bd; border-radius:6px; color:#868e96;");
    rv->addWidget(m_previewLabel, 1);

    m_metaLabel = new QLabel();
    m_metaLabel->setStyleSheet("color:#495057; font-size:9pt;");
    m_metaLabel->setWordWrap(true);
    rv->addWidget(m_metaLabel);

    // 链接输出区域
    auto makeRow = [&](const QString& label, QLineEdit*& edit, QPushButton*& copy) {
        auto* row = new QHBoxLayout();
        auto* l = new QLabel(label);
        l->setMinimumWidth(72);
        l->setStyleSheet("color:#495057;");
        edit = new QLineEdit();
        edit->setReadOnly(true);
        edit->setStyleSheet("background:#f1f3f5;");
        copy = new QPushButton(tr("复制"));
        copy->setFixedWidth(60);
        row->addWidget(l);
        row->addWidget(edit, 1);
        row->addWidget(copy);
        rv->addLayout(row);
    };
    makeRow(tr("URL"),       m_urlEdit,  m_copyUrlBtn);
    makeRow(tr("Markdown"),  m_mdEdit,   m_copyMdBtn);
    makeRow(tr("HTML"),      m_htmlEdit, m_copyHtmlBtn);
    makeRow(tr("BBCode"),    m_bbEdit,   m_copyBbBtn);

    // 操作行
    auto* actRow = new QHBoxLayout();
    m_openBrowserBtn = new QPushButton(tr("🌐 浏览器打开"));
    m_deleteBtn = new QPushButton(tr("🗑 删除（含远端）"));
    m_deleteBtn->setObjectName("dangerBtn");
    actRow->addStretch();
    actRow->addWidget(m_openBrowserBtn);
    actRow->addWidget(m_deleteBtn);
    rv->addLayout(actRow);

    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    main->addWidget(split, 1);

    // 信号
    connect(m_configBtn,   &QPushButton::clicked, this, &OssImageHost::onConfigure);
    connect(m_uploadBtn,   &QPushButton::clicked, this, &OssImageHost::onUploadFile);
    connect(m_pasteBtn,    &QPushButton::clicked, this, &OssImageHost::onPasteImage);
    connect(m_refreshBtn,  &QPushButton::clicked, this, &OssImageHost::onRefreshList);
    connect(m_fetchBucketBtn, &QPushButton::clicked, this, &OssImageHost::onFetchBucket);
    connect(m_copyUrlBtn,  &QPushButton::clicked, this, &OssImageHost::onCopyUrl);
    connect(m_copyMdBtn,   &QPushButton::clicked, this, &OssImageHost::onCopyMd);
    connect(m_copyHtmlBtn, &QPushButton::clicked, this, &OssImageHost::onCopyHtml);
    connect(m_copyBbBtn,   &QPushButton::clicked, this, &OssImageHost::onCopyBbcode);
    connect(m_openBrowserBtn, &QPushButton::clicked, this, &OssImageHost::onOpenInBrowser);
    connect(m_deleteBtn,   &QPushButton::clicked, this, &OssImageHost::onDeleteRecord);
    connect(m_historyList, &QListWidget::itemSelectionChanged,
            this, &OssImageHost::onListItemActivated);
}

void OssImageHost::initDb()
{
    m_db = SqliteManager::getDefaultInstance();
    if (!m_db) return;
    m_db->execute(
        "CREATE TABLE IF NOT EXISTS oss_uploads ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, "
        " uploaded_at TEXT, "
        " local_name TEXT, "
        " remote_key TEXT, "
        " remote_url TEXT, "
        " size INTEGER, "
        " mime TEXT, "
        " width INTEGER, "
        " height INTEGER "
        ")");
}

void OssImageHost::loadConfig()
{
    QSettings s;
    s.beginGroup("OssImageHost");
    m_endpoint     = s.value("endpoint").toString();
    m_bucket       = s.value("bucket").toString();
    m_accessKey    = s.value("accessKey").toString();
    m_secretKey    = s.value("secretKey").toString();
    m_customDomain = s.value("customDomain").toString();
    m_pathPrefix   = s.value("pathPrefix").toString();
    m_useHttps     = s.value("useHttps", true).toBool();
    s.endGroup();

    bool ok = !m_endpoint.isEmpty() && !m_bucket.isEmpty()
            && !m_accessKey.isEmpty() && !m_secretKey.isEmpty();
    if (ok) {
        m_statusLabel->setText(tr("已连接: %1 / %2").arg(m_endpoint, m_bucket));
        m_statusLabel->setStyleSheet("color: #2f9e44; font-size:9pt; padding:0 8px;");
    } else {
        m_statusLabel->setText(tr("⚠️ 未配置（点「⚙ 配置」填写 endpoint / bucket / AK / SK）"));
        m_statusLabel->setStyleSheet("color: #c92a2a; font-size:9pt; padding:0 8px;");
    }
}

void OssImageHost::saveConfig()
{
    QSettings s;
    s.beginGroup("OssImageHost");
    s.setValue("endpoint",     m_endpoint);
    s.setValue("bucket",       m_bucket);
    s.setValue("accessKey",    m_accessKey);
    s.setValue("secretKey",    m_secretKey);
    s.setValue("customDomain", m_customDomain);
    s.setValue("pathPrefix",   m_pathPrefix);
    s.setValue("useHttps",     m_useHttps);
    s.endGroup();
}

// ─────────────────────────────────────────────────────────────
// 配置对话框
// ─────────────────────────────────────────────────────────────

void OssImageHost::onConfigure()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("OSS 配置 — 阿里云"));
    dlg.setMinimumWidth(560);

    auto* form = new QFormLayout();
    auto* ep   = new QLineEdit(m_endpoint);
    ep->setPlaceholderText(tr("如 oss-cn-hangzhou.aliyuncs.com"));
    auto* bk   = new QLineEdit(m_bucket);
    bk->setPlaceholderText(tr("Bucket 名称"));
    auto* ak   = new QLineEdit(m_accessKey);
    ak->setPlaceholderText(tr("AccessKey ID"));
    auto* sk   = new QLineEdit(m_secretKey);
    sk->setEchoMode(QLineEdit::Password);
    sk->setPlaceholderText(tr("AccessKey Secret"));
    auto* cdn  = new QLineEdit(m_customDomain);
    cdn->setPlaceholderText(tr("可选，如 https://img.example.com（不填则用 OSS 默认域名）"));
    auto* pp   = new QLineEdit(m_pathPrefix);
    pp->setPlaceholderText(tr("可选，对象 key 的前缀，如 images/"));

    form->addRow(tr("Endpoint *"),      ep);
    form->addRow(tr("Bucket *"),        bk);
    form->addRow(tr("AccessKey ID *"),  ak);
    form->addRow(tr("AccessKey Secret *"), sk);
    form->addRow(tr("自定义域名"),       cdn);
    form->addRow(tr("路径前缀"),         pp);

    auto* note = new QLabel(tr(
        "签名走阿里云 OSS v1（HMAC-SHA1）。腾讯云 COS / AWS S3 / MinIO 可改用"
        "各自的 S3-compatible 端点 — 现版本只支持阿里云原生签名。"));
    note->setWordWrap(true);
    note->setStyleSheet("color:#868e96; font-size:9pt;");

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* lay = new QVBoxLayout(&dlg);
    lay->addLayout(form);
    lay->addWidget(note);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;
    m_endpoint     = ep->text().trimmed();
    m_bucket       = bk->text().trimmed();
    m_accessKey    = ak->text().trimmed();
    m_secretKey    = sk->text().trimmed();
    m_customDomain = cdn->text().trimmed();
    m_pathPrefix   = pp->text().trimmed();
    saveConfig();
    loadConfig();   // 刷新状态条
}

// ─────────────────────────────────────────────────────────────
// 上传入口
// ─────────────────────────────────────────────────────────────

void OssImageHost::onUploadFile()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择要上传的图片"), QString(),
        tr("图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.svg)"));
    for (const QString& p : files) uploadFile(p);
}

void OssImageHost::onPasteImage()
{
    QClipboard* cb = QApplication::clipboard();
    QImage img = cb->image();
    if (img.isNull()) {
        const QMimeData* md = cb->mimeData();
        if (md && md->hasUrls()) {
            for (const QUrl& u : md->urls()) {
                if (u.isLocalFile()) uploadFile(u.toLocalFile());
            }
            return;
        }
        QMessageBox::information(this, tr("剪贴板为空"),
            tr("剪贴板里没有图片，也没有图片文件路径。"));
        return;
    }
    // 把剪贴板里的 QImage 编码为 PNG
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    uploadBytes(bytes, "image/png",
                QString("clipboard_%1.png")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
}

void OssImageHost::uploadFile(const QString& localPath)
{
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("无法读取"), tr("打不开：%1").arg(localPath));
        return;
    }
    QByteArray data = f.readAll();
    f.close();
    uploadBytes(data, mimeFromName(localPath), QFileInfo(localPath).fileName());
}

// ─────────────────────────────────────────────────────────────
// 核心：PUT object 上传
// ─────────────────────────────────────────────────────────────

void OssImageHost::uploadBytes(const QByteArray& data,
                               const QString& mime,
                               const QString& origName)
{
    if (m_endpoint.isEmpty() || m_bucket.isEmpty()
        || m_accessKey.isEmpty() || m_secretKey.isEmpty()) {
        QMessageBox::warning(this, tr("未配置"),
            tr("请先在「⚙ 配置」里填写 endpoint / bucket / AK / SK。"));
        return;
    }

    const QString key = genObjectKey(m_pathPrefix, origName, mime);
    const QString date = rfc1123GmtNow();
    const QString contentMd5 = QString::fromUtf8(md5Base64(data));
    const QString canonicalResource = QString("/%1/%2").arg(m_bucket, key);
    const QString sig = aliyunSign(m_secretKey, "PUT", contentMd5, mime, date, canonicalResource);

    const QString scheme = m_useHttps ? "https" : "http";
    const QString uploadUrl = QString("%1://%2.%3/%4")
                                  .arg(scheme, m_bucket, m_endpoint, key);

    QNetworkRequest req((QUrl(uploadUrl)));
    req.setRawHeader("Date", date.toUtf8());
    req.setRawHeader("Content-Type", mime.toUtf8());
    req.setRawHeader("Content-MD5", contentMd5.toUtf8());
    req.setRawHeader("Authorization",
        QString("OSS %1:%2").arg(m_accessKey, sig).toUtf8());

    m_statusLabel->setText(tr("⏳ 上传中… %1").arg(origName));
    m_statusLabel->setStyleSheet("color: #495057; font-size:9pt; padding:0 8px;");

    // 抓图片宽高（用于落库）
    int w = 0, h = 0;
    {
        QBuffer buf;
        buf.setData(data);
        buf.open(QIODevice::ReadOnly);
        QImageReader rd(&buf);
        QSize s = rd.size();
        if (s.isValid()) { w = s.width(); h = s.height(); }
    }

    // 构造对外公开 URL（自定义域名优先）
    QString publicUrl;
    if (!m_customDomain.isEmpty()) {
        QString dom = m_customDomain;
        if (dom.endsWith('/')) dom.chop(1);
        publicUrl = dom + "/" + key;
    } else {
        publicUrl = uploadUrl;
    }

    QNetworkReply* reply = m_net->put(req, data);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, key, publicUrl, mime, w, h, sz=qint64(data.size()), origName]() {
        onUploadFinished(reply, key, publicUrl, mime, w, h, sz, origName);
    });
}

void OssImageHost::onUploadFinished(QNetworkReply* reply,
                                    const QString& key,
                                    const QString& publicUrl,
                                    const QString& mime,
                                    int width, int height,
                                    qint64 size,
                                    const QString& origName)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        QString body = QString::fromUtf8(reply->readAll());
        QString err = reply->errorString();
        m_statusLabel->setText(tr("❌ 上传失败：%1").arg(err));
        m_statusLabel->setStyleSheet("color:#c92a2a; font-size:9pt; padding:0 8px;");
        QMessageBox::warning(this, tr("上传失败"),
            tr("%1\n\n服务器响应：\n%2").arg(err, body.left(800)));
        return;
    }
    int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code < 200 || code >= 300) {
        QString body = QString::fromUtf8(reply->readAll());
        m_statusLabel->setText(tr("❌ HTTP %1").arg(code));
        m_statusLabel->setStyleSheet("color:#c92a2a; font-size:9pt; padding:0 8px;");
        QMessageBox::warning(this, tr("上传失败"),
            tr("HTTP %1\n\n%2").arg(code).arg(body.left(800)));
        return;
    }

    // 落库
    if (m_db) {
        QVariantMap row;
        row["uploaded_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        row["local_name"] = origName;
        row["remote_key"] = key;
        row["remote_url"] = publicUrl;
        row["size"] = size;
        row["mime"] = mime;
        row["width"] = width;
        row["height"] = height;
        m_db->insert("oss_uploads", row);
    }

    m_statusLabel->setText(tr("✅ 上传成功：%1（%2）").arg(origName, readableSize(size)));
    m_statusLabel->setStyleSheet("color:#2f9e44; font-size:9pt; padding:0 8px;");

    loadHistory();
    // 选中刚上传的（最后一条）
    if (m_historyList->count() > 0)
        m_historyList->setCurrentRow(0);
}

// ─────────────────────────────────────────────────────────────
// 历史列表
// ─────────────────────────────────────────────────────────────

void OssImageHost::loadHistory()
{
    m_historyList->clear();
    if (!m_db) return;
    auto res = m_db->select(
        "SELECT id, uploaded_at, local_name, remote_key, remote_url, size, mime, width, height "
        "FROM oss_uploads ORDER BY id DESC LIMIT 500", {});
    if (!res.success) return;
    for (const QVariant& v : res.data) {
        auto m = v.toMap();
        auto* item = new QListWidgetItem();
        QString line1 = m["local_name"].toString();
        QString sub = QString("%1 · %2 · %3")
            .arg(m["uploaded_at"].toString().left(16))
            .arg(readableSize(m["size"].toLongLong()))
            .arg(m["mime"].toString());
        item->setText(line1 + "\n" + sub);
        item->setData(Qt::UserRole, m);
        m_historyList->addItem(item);
    }
}

void OssImageHost::onRefreshList()
{
    loadHistory();
}

void OssImageHost::onListItemActivated()
{
    QListWidgetItem* it = m_historyList->currentItem();
    if (!it) { clearOutputs(); return; }
    QVariantMap m = it->data(Qt::UserRole).toMap();
    QString url = m["remote_url"].toString();
    updateOutputs(url, m["local_name"].toString());

    QString meta = tr("%1×%2 · %3 · %4 · %5")
        .arg(m["width"].toInt())
        .arg(m["height"].toInt())
        .arg(readableSize(m["size"].toLongLong()))
        .arg(m["mime"].toString())
        .arg(m["uploaded_at"].toString().left(19).replace('T', ' '));
    m_metaLabel->setText(meta);

    // 异步加载远端预览。借助 OSS 图片处理参数 ?x-oss-process=image/resize 直接服务端缩小，
    // 避免拉原图（手机/相机图常常几 MB）。l_1200 限制最长边为 1200px。
    QString previewUrl = url + (url.contains('?') ? "&" : "?")
                       + "x-oss-process=image/resize,l_1200";
    QNetworkReply* r = m_net->get(QNetworkRequest(QUrl(previewUrl)));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return;
        QPixmap pm;
        if (!pm.loadFromData(r->readAll())) return;

        // HiDPI 清晰：按物理像素缩放，再 setDevicePixelRatio
        qreal dpr = devicePixelRatioF();
        if (dpr <= 0) dpr = 1.0;
        QSize availLogical = m_previewLabel->size() - QSize(20, 20);
        if (availLogical.width() < 50 || availLogical.height() < 50) return;
        QSize availPhys(int(availLogical.width()  * dpr + 0.5),
                        int(availLogical.height() * dpr + 0.5));
        QPixmap scaled = pm.scaled(availPhys, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        m_previewLabel->setPixmap(scaled);
        m_previewLabel->setStyleSheet(
            "background:#f8f9fa; border:1px solid #dee2e6; border-radius:6px;");
    });
}

// ─────────────────────────────────────────────────────────────
// 输出框
// ─────────────────────────────────────────────────────────────

void OssImageHost::updateOutputs(const QString& url, const QString& nameForAlt)
{
    m_urlEdit->setText(url);
    m_mdEdit->setText(QString("![%1](%2)").arg(QFileInfo(nameForAlt).baseName(), url));
    m_htmlEdit->setText(QString("<img src=\"%1\" alt=\"%2\">")
                            .arg(url, QFileInfo(nameForAlt).baseName()));
    m_bbEdit->setText(QString("[img]%1[/img]").arg(url));
}

void OssImageHost::clearOutputs()
{
    m_urlEdit->clear();
    m_mdEdit->clear();
    m_htmlEdit->clear();
    m_bbEdit->clear();
    m_metaLabel->clear();
    m_previewLabel->setText(tr("（拖拽图片到此 / 点上传 / Ctrl+V 粘贴）"));
    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setStyleSheet(
        "background:#f8f9fa; border:1px dashed #adb5bd; border-radius:6px; color:#868e96;");
}

// ─────────────────────────────────────────────────────────────
// 从 OSS 拉取 bucket 下所有对象（ListObjects API）
// GET /  签名同上传，资源 = /<bucket>/，加 query 参数 prefix / max-keys
// ─────────────────────────────────────────────────────────────

void OssImageHost::onFetchBucket()
{
    if (m_endpoint.isEmpty() || m_bucket.isEmpty()
        || m_accessKey.isEmpty() || m_secretKey.isEmpty()) {
        QMessageBox::warning(this, tr("未配置"),
            tr("先去「⚙ 配置」填 endpoint / bucket / AK / SK。"));
        return;
    }
    m_historyList->clear();
    m_statusLabel->setText(tr("⏳ 拉取远端列表中…"));
    m_statusLabel->setStyleSheet("color:#495057; font-size:9pt; padding:0 8px;");

    // 分页拉取整个 bucket。从空 marker 起，IsTruncated=true 时按 NextMarker 继续。
    // 不再过滤 prefix — 路径前缀只用来给新上传分目录，列表应该展示整个 bucket
    // 里所有的图片对象（包括以前从控制台或其他工具上传的）
    fetchBucketPage(QString());
}

void OssImageHost::fetchBucketPage(const QString& marker)
{
    const QString date = rfc1123GmtNow();
    const QString canonicalResource = QString("/%1/").arg(m_bucket);
    const QString sig = aliyunSign(m_secretKey, "GET", "", "", date, canonicalResource);
    const QString scheme = m_useHttps ? "https" : "http";

    QString url = QString("%1://%2.%3/?max-keys=1000").arg(scheme, m_bucket, m_endpoint);
    if (!marker.isEmpty()) {
        url += "&marker=" + QString::fromUtf8(QUrl::toPercentEncoding(marker));
    }

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("Date", date.toUtf8());
    req.setRawHeader("Authorization",
        QString("OSS %1:%2").arg(m_accessKey, sig).toUtf8());

    QNetworkReply* r = m_net->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            QString body = QString::fromUtf8(r->readAll()).left(800);
            m_statusLabel->setText(tr("❌ 拉取失败：%1").arg(r->errorString()));
            m_statusLabel->setStyleSheet("color:#c92a2a; font-size:9pt; padding:0 8px;");
            QMessageBox::warning(this, tr("拉取失败"),
                r->errorString() + (body.isEmpty() ? "" : "\n\n" + body));
            return;
        }
        populateFromBucket(r->readAll());
    });
}

void OssImageHost::populateFromBucket(const QByteArray& xml)
{
    // 解析当前一页的 <Contents>。同时读出 <IsTruncated> 和 <NextMarker> 判断是否还有下一页。
    struct Obj { QString key; qint64 size = 0; QString modified; };
    QList<Obj> objs;
    bool isTruncated = false;
    QString nextMarker;

    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement()) continue;
        const QString tag = r.name().toString();
        if (tag == "IsTruncated") {
            isTruncated = (r.readElementText().compare("true", Qt::CaseInsensitive) == 0);
        } else if (tag == "NextMarker") {
            nextMarker = r.readElementText();
        } else if (tag == "Contents") {
            Obj o;
            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && r.name() == QStringLiteral("Contents")) break;
                if (r.isStartElement()) {
                    QString name = r.name().toString();
                    QString text = r.readElementText();
                    if      (name == "Key")          o.key = text;
                    else if (name == "Size")         o.size = text.toLongLong();
                    else if (name == "LastModified") o.modified = text;
                }
            }
            if (!o.key.isEmpty()) objs.append(o);
        }
    }

    // 仅图片类型
    auto isImage = [](const QString& key) {
        static const QStringList exts = {
            ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg", ".ico", ".tiff"
        };
        QString lower = key.toLower();
        for (const auto& e : exts) if (lower.endsWith(e)) return true;
        return false;
    };

    QString baseUrl = m_customDomain;
    if (baseUrl.isEmpty()) {
        const QString scheme = m_useHttps ? "https" : "http";
        baseUrl = QString("%1://%2.%3").arg(scheme, m_bucket, m_endpoint);
    }
    if (baseUrl.endsWith('/')) baseUrl.chop(1);

    int imgAdded = 0;
    for (const Obj& o : objs) {
        if (!isImage(o.key)) continue;
        ++imgAdded;
        const QString fullUrl = baseUrl + "/"
                              + QString::fromUtf8(QUrl::toPercentEncoding(o.key, "/"));

        auto* item = new QListWidgetItem();
        QString title = QFileInfo(o.key).fileName();
        QString sub = QString("%1 · %2")
                          .arg(readableSize(o.size))
                          .arg(o.modified.left(19).replace('T', ' '));
        item->setText(title + "\n" + sub);

        QVariantMap data;
        data["remote_key"]  = o.key;
        data["remote_url"]  = fullUrl;
        data["local_name"]  = title;
        data["size"]        = o.size;
        data["mime"]        = mimeFromName(o.key);
        data["uploaded_at"] = o.modified;
        data["width"]       = 0;
        data["height"]      = 0;
        item->setData(Qt::UserRole, data);
        m_historyList->addItem(item);

        loadThumbnail(o.key, fullUrl);
    }

    int totalNow = m_historyList->count();
    if (isTruncated && !nextMarker.isEmpty()) {
        m_statusLabel->setText(tr("⏳ 已拉 %1 张，继续拉下一页…").arg(totalNow));
        fetchBucketPage(nextMarker);
    } else {
        m_statusLabel->setText(tr("✅ 拉取完成 · 共 %1 张图片").arg(totalNow));
        m_statusLabel->setStyleSheet("color:#2f9e44; font-size:9pt; padding:0 8px;");
    }
}

void OssImageHost::loadThumbnail(const QString& remoteKey, const QString& fullUrl)
{
    // OSS 图片处理：服务端缩到 96×96 fill，省带宽 + 不挪 UI 线程
    QString thumbUrl = fullUrl + (fullUrl.contains('?') ? "&" : "?")
                     + "x-oss-process=image/resize,m_fill,w_96,h_96";
    QNetworkReply* r = m_net->get(QNetworkRequest(QUrl(thumbUrl)));
    connect(r, &QNetworkReply::finished, this, [this, r, remoteKey]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return;
        QPixmap pm;
        if (!pm.loadFromData(r->readAll())) return;
        // 异步回来时 item 可能已被替换 — 按 remoteKey 重新查找
        for (int i = 0; i < m_historyList->count(); ++i) {
            QListWidgetItem* it = m_historyList->item(i);
            if (!it) continue;
            if (it->data(Qt::UserRole).toMap().value("remote_key").toString() == remoteKey) {
                it->setIcon(QIcon(pm));
                break;
            }
        }
    });
}

void OssImageHost::onCopyUrl()  { QApplication::clipboard()->setText(m_urlEdit->text()); m_statusLabel->setText(tr("✅ URL 已复制")); }
void OssImageHost::onCopyMd()   { QApplication::clipboard()->setText(m_mdEdit->text());  m_statusLabel->setText(tr("✅ Markdown 已复制")); }
void OssImageHost::onCopyHtml() { QApplication::clipboard()->setText(m_htmlEdit->text());m_statusLabel->setText(tr("✅ HTML 已复制")); }
void OssImageHost::onCopyBbcode(){QApplication::clipboard()->setText(m_bbEdit->text()); m_statusLabel->setText(tr("✅ BBCode 已复制")); }

void OssImageHost::onOpenInBrowser()
{
    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(url));
}

// ─────────────────────────────────────────────────────────────
// 删除：本地记录 + 远端对象（DELETE 也走签名）
// ─────────────────────────────────────────────────────────────

void OssImageHost::onDeleteRecord()
{
    QListWidgetItem* it = m_historyList->currentItem();
    if (!it) return;
    QVariantMap m = it->data(Qt::UserRole).toMap();
    QString key = m["remote_key"].toString();
    qint64 id = m["id"].toLongLong();

    QMessageBox box(this);
    box.setWindowTitle(tr("确认删除"));
    box.setText(tr("删除「%1」？").arg(m["local_name"].toString()));
    auto* delAll  = box.addButton(tr("远端 + 本地"), QMessageBox::AcceptRole);
    auto* delHist = box.addButton(tr("只删本地记录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    auto clicked = box.clickedButton();
    if (clicked != delAll && clicked != delHist) return;

    if (clicked == delAll) {
        // DELETE Object — 签名同 PUT，但 Verb = DELETE，Content-Type/MD5 都空
        const QString date = rfc1123GmtNow();
        const QString canonicalResource = QString("/%1/%2").arg(m_bucket, key);
        const QString sig = aliyunSign(m_secretKey, "DELETE", "", "", date, canonicalResource);
        const QString scheme = m_useHttps ? "https" : "http";
        const QString delUrl = QString("%1://%2.%3/%4")
                                   .arg(scheme, m_bucket, m_endpoint, key);
        QNetworkRequest req((QUrl(delUrl)));
        req.setRawHeader("Date", date.toUtf8());
        req.setRawHeader("Authorization",
            QString("OSS %1:%2").arg(m_accessKey, sig).toUtf8());

        QNetworkReply* r = m_net->deleteResource(req);
        connect(r, &QNetworkReply::finished, this, [this, r, id]() {
            r->deleteLater();
            int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (r->error() != QNetworkReply::NoError && code != 204) {
                QMessageBox::warning(this, tr("远端删除失败"),
                    r->errorString() + "\n" + QString::fromUtf8(r->readAll()).left(500));
                return;
            }
            if (m_db) m_db->execute("DELETE FROM oss_uploads WHERE id = :id",
                                    {{"id", id}});
            loadHistory();
            clearOutputs();
            m_statusLabel->setText(tr("✅ 已删除"));
        });
    } else {
        // 只删本地
        if (m_db) m_db->execute("DELETE FROM oss_uploads WHERE id = :id", {{"id", id}});
        loadHistory();
        clearOutputs();
        m_statusLabel->setText(tr("✅ 已删本地记录"));
    }
}

// ─────────────────────────────────────────────────────────────
// 拖拽支持
// ─────────────────────────────────────────────────────────────

void OssImageHost::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls() || e->mimeData()->hasImage()) {
        e->acceptProposedAction();
    }
}

void OssImageHost::dropEvent(QDropEvent* e)
{
    const QMimeData* md = e->mimeData();
    if (md->hasImage()) {
        QImage img = qvariant_cast<QImage>(md->imageData());
        if (!img.isNull()) {
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            uploadBytes(bytes, "image/png",
                QString("dropped_%1.png").arg(
                    QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
            return;
        }
    }
    if (md->hasUrls()) {
        for (const QUrl& u : md->urls()) {
            if (u.isLocalFile()) uploadFile(u.toLocalFile());
        }
    }
}
