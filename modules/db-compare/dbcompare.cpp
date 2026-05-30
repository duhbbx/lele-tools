#include "dbcompare.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <QHash>
#include <QSet>
#include <QStringList>

// 原生客户端头文件
#ifdef WITH_MYSQL
  #include <mysql.h>
#endif

#ifdef WITH_LIBPQ
  #include <libpq-fe.h>
#endif

#ifdef WITH_ORACLE_OCI
  #include <oci.h>
#endif

REGISTER_DYNAMICOBJECT(DbCompare);

using namespace DbCompareNS;

// ──────────────────────────────────────────────────────────────
// 工具函数
// ──────────────────────────────────────────────────────────────

namespace {

QString driverDisplayName(DbType t) {
    switch (t) {
        case DbType::MySQL:      return QObject::tr("MySQL");
        case DbType::PostgreSQL: return QObject::tr("PostgreSQL");
        case DbType::Oracle:     return QObject::tr("Oracle");
    }
    return {};
}

int defaultPort(DbType t) {
    switch (t) {
        case DbType::MySQL:      return 3306;
        case DbType::PostgreSQL: return 5432;
        case DbType::Oracle:     return 1521;
    }
    return 0;
}

bool driverCompiled(DbType t) {
    switch (t) {
        case DbType::MySQL:
#ifdef WITH_MYSQL
            return true;
#else
            return false;
#endif
        case DbType::PostgreSQL:
#ifdef WITH_LIBPQ
            return true;
#else
            return false;
#endif
        case DbType::Oracle:
#ifdef WITH_ORACLE_OCI
            return true;
#else
            return false;
#endif
    }
    return false;
}

QString diskStatusText(DiffStatus s) {
    switch (s) {
        case DiffStatus::Match:        return QObject::tr("一致");
        case DiffStatus::OnlyInSource: return QObject::tr("仅源端有");
        case DiffStatus::OnlyInTarget: return QObject::tr("仅目标端有");
        case DiffStatus::Differ:       return QObject::tr("存在差异");
    }
    return {};
}

QColor statusColor(DiffStatus s) {
    switch (s) {
        case DiffStatus::Match:        return QColor(0x2f, 0x9e, 0x44);
        case DiffStatus::OnlyInSource: return QColor(0xc9, 0x2a, 0x2a);
        case DiffStatus::OnlyInTarget: return QColor(0xc9, 0x2a, 0x2a);
        case DiffStatus::Differ:       return QColor(0xe6, 0x77, 0x00);
    }
    return Qt::black;
}

// 列描述：用作 UI 显示与"同构模式"下生成 reasons 的对比字符串
QString columnDesc(const ColumnInfo& c, bool homogeneous) {
    QString s = c.rawType.isEmpty() ? c.dataType : c.rawType;
    if (c.length > 0 && !s.contains('(')) s += QString("(%1)").arg(c.length);
    if (c.precision > 0 && !s.contains('(')) {
        if (c.scale > 0) s += QString("(%1,%2)").arg(c.precision).arg(c.scale);
        else             s += QString("(%1)").arg(c.precision);
    }
    if (!c.nullable) s += " NOT NULL";
    if (homogeneous) {
        if (!c.charset.isEmpty())   s += " charset=" + c.charset;
        if (!c.collation.isEmpty()) s += " collate=" + c.collation;
    }
    return s;
}

// 比对两列的细节差异，只在 homogeneous 下使用
QStringList columnDiffReasons(const ColumnInfo& a, const ColumnInfo& b) {
    QStringList r;
    auto cmp = [&](const QString& key, const QString& l, const QString& rgt) {
        if (l != rgt) r << QString("%1: %2 ≠ %3").arg(key, l.isEmpty() ? "-" : l, rgt.isEmpty() ? "-" : rgt);
    };
    auto cmpInt = [&](const QString& key, int l, int rgt) {
        if (l != rgt) r << QString("%1: %2 ≠ %3").arg(key).arg(l).arg(rgt);
    };
    cmp(QObject::tr("类型"),
        a.rawType.isEmpty() ? a.dataType : a.rawType,
        b.rawType.isEmpty() ? b.dataType : b.rawType);
    cmpInt(QObject::tr("长度"),    a.length,    b.length);
    cmpInt(QObject::tr("精度"),    a.precision, b.precision);
    cmpInt(QObject::tr("小数位"),  a.scale,     b.scale);
    if (a.nullable != b.nullable) {
        r << QObject::tr("可空: %1 ≠ %2")
                 .arg(a.nullable ? "YES" : "NO", b.nullable ? "YES" : "NO");
    }
    cmp(QObject::tr("字符集"),     a.charset,   b.charset);
    cmp(QObject::tr("排序规则"),   a.collation, b.collation);
    return r;
}

}   // namespace

// ──────────────────────────────────────────────────────────────
// MySQL 客户端（libmysqlclient）
// ──────────────────────────────────────────────────────────────

#ifdef WITH_MYSQL
namespace {

class MySqlClient {
public:
    bool connect(const ConnectionConfig& cfg, QString* err) {
        m_conn = mysql_init(nullptr);
        if (!m_conn) {
            if (err) *err = "mysql_init 失败";
            return false;
        }
        // UTF8MB4 字符集 — 元数据查询里有可能含中文
        mysql_options(m_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        unsigned int timeoutSec = 8;
        mysql_options(m_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeoutSec);
        mysql_options(m_conn, MYSQL_OPT_READ_TIMEOUT,    &timeoutSec);
        mysql_options(m_conn, MYSQL_OPT_WRITE_TIMEOUT,   &timeoutSec);

        // SSL：开发/内网数据库常用自签证书，默认走 PREFERRED — 加密但不验证证书。
        // 验证证书的严格模式留给后续 UI 开关，眼下避免被自签链直接拒掉。
#ifdef SSL_MODE_PREFERRED
        {
            unsigned int sslMode = SSL_MODE_PREFERRED;
            mysql_options(m_conn, MYSQL_OPT_SSL_MODE, &sslMode);
        }
#endif
        {
            // 老 API 兜底：MYSQL_OPT_SSL_VERIFY_SERVER_CERT = 0，type 在各版本里
            // 是 my_bool/bool/char，用 char 最稳（mysql_options 按字节读）
            char noVerify = 0;
            mysql_options(m_conn, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &noVerify);
        }

        if (!mysql_real_connect(m_conn,
                                cfg.host.toUtf8().constData(),
                                cfg.user.toUtf8().constData(),
                                cfg.password.toUtf8().constData(),
                                cfg.database.toUtf8().constData(),
                                static_cast<unsigned int>(cfg.port),
                                nullptr, 0))
        {
            if (err) *err = QString::fromUtf8(mysql_error(m_conn));
            mysql_close(m_conn);
            m_conn = nullptr;
            return false;
        }
        m_schema = cfg.database;
        return true;
    }

    void disconnect() {
        if (m_conn) { mysql_close(m_conn); m_conn = nullptr; }
    }

    ~MySqlClient() { disconnect(); }

    QString escape(const QString& s) const {
        QByteArray in = s.toUtf8();
        QByteArray out(in.size() * 2 + 1, '\0');
        unsigned long len = mysql_real_escape_string(
            m_conn, out.data(), in.constData(), in.size());
        return QString::fromUtf8(out.constData(), int(len));
    }

    QStringList listDatabases(QString* err) {
        QStringList out;
        if (!m_conn) { if (err) *err = "未连接"; return out; }
        if (mysql_query(m_conn, "SHOW DATABASES") != 0) {
            if (err) *err = QString::fromUtf8(mysql_error(m_conn));
            return out;
        }
        MYSQL_RES* res = mysql_store_result(m_conn);
        if (!res) return out;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            QString name = row[0] ? QString::fromUtf8(row[0]) : QString();
            if (name == "information_schema" || name == "mysql"
                || name == "performance_schema" || name == "sys") continue;
            out << name;
        }
        mysql_free_result(res);
        return out;
    }

    QList<TableInfo> fetchSchema(QString* err) {
        QList<TableInfo> tables;
        if (!m_conn) { if (err) *err = "未连接"; return tables; }

        QString esc = escape(m_schema);
        // 表
        QString sql = QString(
            "SELECT TABLE_NAME, IFNULL(TABLE_COLLATION,''), IFNULL(ENGINE,''), IFNULL(TABLE_COMMENT,'') "
            "FROM information_schema.TABLES "
            "WHERE TABLE_SCHEMA = '%1' AND TABLE_TYPE = 'BASE TABLE' "
            "ORDER BY TABLE_NAME").arg(esc);
        if (mysql_query(m_conn, sql.toUtf8().constData()) != 0) {
            if (err) *err = QString::fromUtf8(mysql_error(m_conn));
            return tables;
        }
        MYSQL_RES* res = mysql_store_result(m_conn);
        if (!res) {
            if (err) *err = QString::fromUtf8(mysql_error(m_conn));
            return tables;
        }
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            TableInfo t;
            t.name      = row[0] ? QString::fromUtf8(row[0]) : QString();
            t.collation = row[1] ? QString::fromUtf8(row[1]) : QString();
            t.engine    = row[2] ? QString::fromUtf8(row[2]) : QString();
            t.comment   = row[3] ? QString::fromUtf8(row[3]) : QString();
            tables << t;
        }
        mysql_free_result(res);

        // 列：每张表逐个查
        for (TableInfo& t : tables) {
            QString tEsc = escape(t.name);
            QString colSql = QString(
                "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, "
                "       IFNULL(CHARACTER_MAXIMUM_LENGTH, -1), "
                "       IFNULL(NUMERIC_PRECISION, -1), "
                "       IFNULL(NUMERIC_SCALE, -1), "
                "       IS_NULLABLE, "
                "       IFNULL(CHARACTER_SET_NAME, ''), "
                "       IFNULL(COLLATION_NAME, ''), "
                "       IFNULL(COLUMN_DEFAULT, ''), "
                "       IFNULL(COLUMN_COMMENT, '') "
                "FROM information_schema.COLUMNS "
                "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
                "ORDER BY ORDINAL_POSITION").arg(esc, tEsc);
            if (mysql_query(m_conn, colSql.toUtf8().constData()) != 0) continue;
            MYSQL_RES* cres = mysql_store_result(m_conn);
            if (!cres) continue;
            MYSQL_ROW crow;
            while ((crow = mysql_fetch_row(cres))) {
                ColumnInfo c;
                c.name         = crow[0] ? QString::fromUtf8(crow[0]) : QString();
                c.dataType     = crow[1] ? QString::fromUtf8(crow[1]).toUpper() : QString();
                c.rawType      = crow[2] ? QString::fromUtf8(crow[2]) : QString();
                c.length       = crow[3] ? QString(crow[3]).toInt()   : -1;
                c.precision    = crow[4] ? QString(crow[4]).toInt()   : -1;
                c.scale        = crow[5] ? QString(crow[5]).toInt()   : -1;
                c.nullable     = crow[6] && QString::fromUtf8(crow[6]).compare("YES", Qt::CaseInsensitive) == 0;
                c.charset      = crow[7] ? QString::fromUtf8(crow[7]) : QString();
                c.collation    = crow[8] ? QString::fromUtf8(crow[8]) : QString();
                c.defaultValue = crow[9] ? QString::fromUtf8(crow[9]) : QString();
                c.comment      = crow[10] ? QString::fromUtf8(crow[10]) : QString();
                t.columns << c;
            }
            mysql_free_result(cres);
        }
        return tables;
    }

private:
    MYSQL* m_conn = nullptr;
    QString m_schema;
};

}   // namespace
#endif // WITH_MYSQL

// ──────────────────────────────────────────────────────────────
// PostgreSQL 客户端（libpq）
// ──────────────────────────────────────────────────────────────

#ifdef WITH_LIBPQ
namespace {

class PgClient {
public:
    bool connect(const ConnectionConfig& cfg, QString* err) {
        QByteArray host = cfg.host.toUtf8();
        QByteArray portStr = QByteArray::number(cfg.port);
        QByteArray user = cfg.user.toUtf8();
        QByteArray pwd  = cfg.password.toUtf8();
        QByteArray db   = cfg.database.toUtf8();

        m_conn = PQsetdbLogin(host.constData(), portStr.constData(),
                              nullptr, nullptr,
                              db.constData(),
                              user.constData(),
                              pwd.constData());
        if (PQstatus(m_conn) != CONNECTION_OK) {
            if (err) *err = QString::fromUtf8(PQerrorMessage(m_conn)).trimmed();
            PQfinish(m_conn);
            m_conn = nullptr;
            return false;
        }
        m_schema = cfg.schema.isEmpty() ? QStringLiteral("public") : cfg.schema;
        // 客户端编码 → UTF8
        PQexec(m_conn, "SET client_encoding TO 'UTF8'");
        return true;
    }

    void disconnect() {
        if (m_conn) { PQfinish(m_conn); m_conn = nullptr; }
    }

    ~PgClient() { disconnect(); }

    QStringList listDatabases(QString* err) {
        QStringList out;
        if (!m_conn) { if (err) *err = "未连接"; return out; }
        PGresult* res = PQexec(m_conn,
            "SELECT datname FROM pg_database "
            "WHERE datistemplate = false AND datallowconn ORDER BY datname");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            if (err) *err = QString::fromUtf8(PQerrorMessage(m_conn)).trimmed();
            PQclear(res); return out;
        }
        int n = PQntuples(res);
        for (int i = 0; i < n; ++i) out << QString::fromUtf8(PQgetvalue(res, i, 0));
        PQclear(res);
        return out;
    }

    QStringList listSchemas(QString* err) {
        QStringList out;
        if (!m_conn) { if (err) *err = "未连接"; return out; }
        PGresult* res = PQexec(m_conn,
            "SELECT schema_name FROM information_schema.schemata "
            "WHERE schema_name NOT IN ('pg_catalog','information_schema','pg_toast') "
            "  AND schema_name NOT LIKE 'pg_temp_%' "
            "  AND schema_name NOT LIKE 'pg_toast_temp_%' "
            "ORDER BY schema_name");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            if (err) *err = QString::fromUtf8(PQerrorMessage(m_conn)).trimmed();
            PQclear(res); return out;
        }
        int n = PQntuples(res);
        for (int i = 0; i < n; ++i) out << QString::fromUtf8(PQgetvalue(res, i, 0));
        PQclear(res);
        return out;
    }

    QList<TableInfo> fetchSchema(QString* err) {
        QList<TableInfo> tables;
        if (!m_conn) { if (err) *err = "未连接"; return tables; }

        // 表
        const char* tblSql =
            "SELECT table_name "
            "FROM information_schema.tables "
            "WHERE table_schema = $1 AND table_type = 'BASE TABLE' "
            "ORDER BY table_name";
        QByteArray schemaUtf8 = m_schema.toUtf8();
        const char* params[1] = { schemaUtf8.constData() };
        PGresult* res = PQexecParams(m_conn, tblSql, 1, nullptr, params,
                                     nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            if (err) *err = QString::fromUtf8(PQerrorMessage(m_conn)).trimmed();
            PQclear(res);
            return tables;
        }
        int nrows = PQntuples(res);
        tables.reserve(nrows);
        for (int i = 0; i < nrows; ++i) {
            TableInfo t;
            t.name = QString::fromUtf8(PQgetvalue(res, i, 0));
            tables << t;
        }
        PQclear(res);

        // 列：每张表查
        const char* colSql =
            "SELECT column_name, data_type, udt_name, "
            "       COALESCE(character_maximum_length, -1), "
            "       COALESCE(numeric_precision, -1), "
            "       COALESCE(numeric_scale, -1), "
            "       is_nullable, "
            "       COALESCE(character_set_name, ''), "
            "       COALESCE(collation_name, ''), "
            "       COALESCE(column_default, '') "
            "FROM information_schema.columns "
            "WHERE table_schema = $1 AND table_name = $2 "
            "ORDER BY ordinal_position";
        for (TableInfo& t : tables) {
            QByteArray tName = t.name.toUtf8();
            const char* cparams[2] = { schemaUtf8.constData(), tName.constData() };
            PGresult* cres = PQexecParams(m_conn, colSql, 2, nullptr, cparams,
                                          nullptr, nullptr, 0);
            if (PQresultStatus(cres) != PGRES_TUPLES_OK) {
                PQclear(cres);
                continue;
            }
            int cn = PQntuples(cres);
            for (int j = 0; j < cn; ++j) {
                ColumnInfo c;
                c.name         = QString::fromUtf8(PQgetvalue(cres, j, 0));
                c.dataType     = QString::fromUtf8(PQgetvalue(cres, j, 1)).toUpper();
                c.rawType      = QString::fromUtf8(PQgetvalue(cres, j, 2));
                c.length       = QString(PQgetvalue(cres, j, 3)).toInt();
                c.precision    = QString(PQgetvalue(cres, j, 4)).toInt();
                c.scale        = QString(PQgetvalue(cres, j, 5)).toInt();
                c.nullable     = QString::fromUtf8(PQgetvalue(cres, j, 6)).compare("YES", Qt::CaseInsensitive) == 0;
                c.charset      = QString::fromUtf8(PQgetvalue(cres, j, 7));
                c.collation    = QString::fromUtf8(PQgetvalue(cres, j, 8));
                c.defaultValue = QString::fromUtf8(PQgetvalue(cres, j, 9));
                t.columns << c;
            }
            PQclear(cres);
        }
        return tables;
    }

private:
    PGconn* m_conn = nullptr;
    QString m_schema;
};

}   // namespace
#endif // WITH_LIBPQ

// ──────────────────────────────────────────────────────────────
// Oracle 客户端（OCI）— 可选编译
// ──────────────────────────────────────────────────────────────

#ifdef WITH_ORACLE_OCI
namespace {

class OracleClient {
public:
    bool connect(const ConnectionConfig& cfg, QString* err) {
        // service_name (cfg.database) + host + port → easy connect string
        // 形如 "//host:port/service_name"
        m_connStr = QString("//%1:%2/%3").arg(cfg.host).arg(cfg.port).arg(cfg.database);
        m_owner = cfg.schema.toUpper();
        if (m_owner.isEmpty()) m_owner = cfg.user.toUpper();

        sword r = OCIEnvCreate(&m_env, OCI_DEFAULT, nullptr, nullptr, nullptr, nullptr, 0, nullptr);
        if (r != OCI_SUCCESS) { if (err) *err = "OCIEnvCreate 失败"; return false; }

        OCIHandleAlloc(m_env, (void**)&m_err, OCI_HTYPE_ERROR, 0, nullptr);

        QByteArray u = cfg.user.toUtf8();
        QByteArray p = cfg.password.toUtf8();
        QByteArray d = m_connStr.toUtf8();
        r = OCILogon(m_env, m_err, &m_svc,
                     (OraText*)u.constData(), u.size(),
                     (OraText*)p.constData(), p.size(),
                     (OraText*)d.constData(), d.size());
        if (r != OCI_SUCCESS) {
            char buf[512] = {0};
            sb4 code = 0;
            OCIErrorGet(m_err, 1, nullptr, &code, (OraText*)buf, sizeof(buf), OCI_HTYPE_ERROR);
            if (err) *err = QString::fromUtf8(buf).trimmed();
            cleanup();
            return false;
        }
        return true;
    }

    void disconnect() { cleanup(); }
    ~OracleClient() { cleanup(); }

    QStringList listSchemas(QString* err) {
        QStringList out;
        if (!m_svc) { if (err) *err = "未连接"; return out; }
        runSimpleQuery("SELECT USERNAME FROM ALL_USERS ORDER BY USERNAME", 1,
            [&](const QStringList& row){ out << row[0]; }, err);
        return out;
    }

    QList<TableInfo> fetchSchema(QString* err) {
        QList<TableInfo> tables;
        if (!m_svc) { if (err) *err = "未连接"; return tables; }

        // 表名（限定 owner）
        QString sql = QString("SELECT TABLE_NAME FROM ALL_TABLES WHERE OWNER = '%1' ORDER BY TABLE_NAME").arg(m_owner);
        QStringList names;
        if (!runSimpleQuery(sql, 1, [&](const QStringList& row){
            names << row[0];
        }, err)) {
            return tables;
        }
        for (const QString& n : names) {
            TableInfo t; t.name = n;
            tables << t;
        }

        // 列
        for (TableInfo& t : tables) {
            QString cs = QString(
                "SELECT COLUMN_NAME, DATA_TYPE, "
                "NVL(DATA_LENGTH, -1), NVL(DATA_PRECISION, -1), NVL(DATA_SCALE, -1), "
                "NULLABLE, NVL(CHARACTER_SET_NAME,'') "
                "FROM ALL_TAB_COLUMNS WHERE OWNER = '%1' AND TABLE_NAME = '%2' "
                "ORDER BY COLUMN_ID").arg(m_owner, t.name);
            runSimpleQuery(cs, 7, [&](const QStringList& row){
                ColumnInfo c;
                c.name      = row[0];
                c.rawType   = row[1];
                c.dataType  = row[1].toUpper();
                c.length    = row[2].toInt();
                c.precision = row[3].toInt();
                c.scale     = row[4].toInt();
                c.nullable  = (row[5].toUpper() == "Y");
                c.charset   = row[6];
                t.columns << c;
            }, nullptr);
        }
        return tables;
    }

private:
    bool runSimpleQuery(const QString& sql, int colCount,
                        std::function<void(const QStringList&)> onRow,
                        QString* err)
    {
        OCIStmt* stmt = nullptr;
        OCIHandleAlloc(m_env, (void**)&stmt, OCI_HTYPE_STMT, 0, nullptr);
        QByteArray q = sql.toUtf8();
        sword r = OCIStmtPrepare(stmt, m_err, (OraText*)q.constData(), q.size(),
                                 OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (r != OCI_SUCCESS) {
            char buf[512] = {0}; sb4 code = 0;
            OCIErrorGet(m_err, 1, nullptr, &code, (OraText*)buf, sizeof(buf), OCI_HTYPE_ERROR);
            if (err) *err = QString::fromUtf8(buf).trimmed();
            OCIHandleFree(stmt, OCI_HTYPE_STMT);
            return false;
        }

        // 定义列输出缓冲（每列 4KB 文本）
        QVector<QByteArray> bufs(colCount, QByteArray(4096, '\0'));
        QVector<sb2> indicators(colCount, 0);
        for (int i = 0; i < colCount; ++i) {
            OCIDefine* def = nullptr;
            OCIDefineByPos(stmt, &def, m_err, (ub4)(i + 1),
                           bufs[i].data(), bufs[i].size(), SQLT_STR,
                           &indicators[i], nullptr, nullptr, OCI_DEFAULT);
        }

        r = OCIStmtExecute(m_svc, stmt, m_err, 0, 0, nullptr, nullptr, OCI_DEFAULT);
        if (r != OCI_SUCCESS && r != OCI_SUCCESS_WITH_INFO && r != OCI_NO_DATA) {
            char buf[512] = {0}; sb4 code = 0;
            OCIErrorGet(m_err, 1, nullptr, &code, (OraText*)buf, sizeof(buf), OCI_HTYPE_ERROR);
            if (err) *err = QString::fromUtf8(buf).trimmed();
            OCIHandleFree(stmt, OCI_HTYPE_STMT);
            return false;
        }

        while (true) {
            sword fr = OCIStmtFetch(stmt, m_err, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
            if (fr == OCI_NO_DATA) break;
            if (fr != OCI_SUCCESS && fr != OCI_SUCCESS_WITH_INFO) break;
            QStringList row;
            for (int i = 0; i < colCount; ++i) {
                if (indicators[i] == -1) row << QString();
                else row << QString::fromUtf8(bufs[i].constData());
            }
            onRow(row);
        }

        OCIHandleFree(stmt, OCI_HTYPE_STMT);
        return true;
    }

    void cleanup() {
        if (m_svc) { OCILogoff(m_svc, m_err); m_svc = nullptr; }
        if (m_err) { OCIHandleFree(m_err, OCI_HTYPE_ERROR); m_err = nullptr; }
        if (m_env) { OCIHandleFree(m_env, OCI_HTYPE_ENV); m_env = nullptr; }
    }

    OCIEnv* m_env = nullptr;
    OCIError* m_err = nullptr;
    OCISvcCtx* m_svc = nullptr;
    QString m_connStr;
    QString m_owner;
};

}   // namespace
#endif // WITH_ORACLE_OCI

// ──────────────────────────────────────────────────────────────
// 元数据 fetch 分发
// ──────────────────────────────────────────────────────────────

bool DbCompare::runFetchSchema(const ConnectionConfig& cfg,
                               const QString& connName,
                               QList<TableInfo>& outTables,
                               QString* errMsg)
{
    Q_UNUSED(connName);
    outTables.clear();

    if (!driverCompiled(cfg.type)) {
        if (errMsg) *errMsg = tr("%1 驱动未编译进当前构建。\n"
                                 "MySQL: 需要 libmysqlclient（默认已链接）；\n"
                                 "PostgreSQL: 需要 libpq，cmake -DWITH_LIBPQ=ON；\n"
                                 "Oracle: 需要 Oracle Instant Client + OCI，cmake -DWITH_ORACLE_OCI=ON")
                          .arg(driverDisplayName(cfg.type));
        return false;
    }

    switch (cfg.type) {
        case DbType::MySQL: {
#ifdef WITH_MYSQL
            MySqlClient c;
            if (!c.connect(cfg, errMsg)) return false;
            outTables = c.fetchSchema(errMsg);
            return errMsg ? errMsg->isEmpty() : true;
#else
            if (errMsg) *errMsg = tr("MySQL 驱动未编译");
            return false;
#endif
        }
        case DbType::PostgreSQL: {
#ifdef WITH_LIBPQ
            PgClient c;
            if (!c.connect(cfg, errMsg)) return false;
            outTables = c.fetchSchema(errMsg);
            return errMsg ? errMsg->isEmpty() : true;
#else
            if (errMsg) *errMsg = tr("PostgreSQL 驱动未编译");
            return false;
#endif
        }
        case DbType::Oracle: {
#ifdef WITH_ORACLE_OCI
            OracleClient c;
            if (!c.connect(cfg, errMsg)) return false;
            outTables = c.fetchSchema(errMsg);
            return errMsg ? errMsg->isEmpty() : true;
#else
            if (errMsg) *errMsg = tr("Oracle 驱动未编译，请用 -DWITH_ORACLE_OCI=ON 重新构建");
            return false;
#endif
        }
    }
    return false;
}

bool DbCompare::runListDbsAndSchemas(const ConnectionConfig& cfg,
                                     QStringList& dbs, QStringList& schemas,
                                     QString* errMsg)
{
    dbs.clear();
    schemas.clear();
    if (!driverCompiled(cfg.type)) {
        if (errMsg) *errMsg = tr("%1 驱动未编译").arg(driverDisplayName(cfg.type));
        return false;
    }
    switch (cfg.type) {
        case DbType::MySQL: {
#ifdef WITH_MYSQL
            MySqlClient c;
            ConnectionConfig tmp = cfg;
            if (tmp.database.isEmpty()) tmp.database = "mysql";   // 占位库便于连
            if (!c.connect(tmp, errMsg)) return false;
            dbs = c.listDatabases(errMsg);
            schemas = dbs;          // MySQL：schema == database
            return true;
#else
            break;
#endif
        }
        case DbType::PostgreSQL: {
#ifdef WITH_LIBPQ
            PgClient c;
            ConnectionConfig tmp = cfg;
            if (tmp.database.isEmpty()) tmp.database = "postgres";
            if (!c.connect(tmp, errMsg)) return false;
            dbs = c.listDatabases(errMsg);
            // schemas 属于具体 db；只在用户已选定数据库时拉取
            if (!cfg.database.isEmpty()) schemas = c.listSchemas(errMsg);
            return true;
#else
            break;
#endif
        }
        case DbType::Oracle: {
#ifdef WITH_ORACLE_OCI
            OracleClient c;
            if (!c.connect(cfg, errMsg)) return false;
            schemas = c.listSchemas(errMsg);  // Oracle 的 schema = USERNAME
            // Oracle 的 service name 在监听器层面，没法用 SQL 列；db 下拉不填
            return true;
#else
            break;
#endif
        }
    }
    if (errMsg) *errMsg = tr("驱动未编译");
    return false;
}

void DbCompare::populateCombos(bool source,
                               const QStringList& dbs,
                               const QStringList& schemas)
{
    QComboBox* dbCombo = source ? m_srcDb : m_tgtDb;
    QComboBox* schCombo = source ? m_srcSchema : m_tgtSchema;
    auto refill = [](QComboBox* cb, const QStringList& items) {
        QString cur = cb->currentText();
        QSignalBlocker block(cb);
        cb->clear();
        cb->addItems(items);
        // 保留用户已经输入/选择的内容
        if (!cur.isEmpty()) {
            int idx = cb->findText(cur);
            if (idx >= 0) cb->setCurrentIndex(idx);
            else cb->setEditText(cur);
        } else if (!items.isEmpty()) {
            cb->setCurrentIndex(0);
        }
    };
    refill(dbCombo, dbs);
    refill(schCombo, schemas);
}

// ──────────────────────────────────────────────────────────────
// 比对引擎
// ──────────────────────────────────────────────────────────────

namespace {

QString keyOf(const QString& s) { return s.toLower(); }

QList<TableDiff> compareSchemas(const QList<TableInfo>& src,
                                const QList<TableInfo>& tgt,
                                bool homogeneous)
{
    QHash<QString, const TableInfo*> srcMap, tgtMap;
    for (const auto& t : src) srcMap.insert(keyOf(t.name), &t);
    for (const auto& t : tgt) tgtMap.insert(keyOf(t.name), &t);

    QSet<QString> allKeys;
    for (auto it = srcMap.begin(); it != srcMap.end(); ++it) allKeys.insert(it.key());
    for (auto it = tgtMap.begin(); it != tgtMap.end(); ++it) allKeys.insert(it.key());

    QStringList sortedKeys(allKeys.begin(), allKeys.end());
    std::sort(sortedKeys.begin(), sortedKeys.end());

    QList<TableDiff> diffs;
    for (const QString& k : sortedKeys) {
        const TableInfo* a = srcMap.value(k);
        const TableInfo* b = tgtMap.value(k);
        TableDiff td;
        td.tableName = a ? a->name : b->name;
        if (!a) {
            td.status = DiffStatus::OnlyInTarget;
            td.targetColumnCount = b->columns.size();
            diffs << td;
            continue;
        }
        if (!b) {
            td.status = DiffStatus::OnlyInSource;
            td.sourceColumnCount = a->columns.size();
            diffs << td;
            continue;
        }
        td.sourceColumnCount = a->columns.size();
        td.targetColumnCount = b->columns.size();

        // 字段集合
        QHash<QString, const ColumnInfo*> aCols, bCols;
        for (const auto& c : a->columns) aCols.insert(keyOf(c.name), &c);
        for (const auto& c : b->columns) bCols.insert(keyOf(c.name), &c);

        QSet<QString> colKeys;
        for (auto it = aCols.begin(); it != aCols.end(); ++it) colKeys.insert(it.key());
        for (auto it = bCols.begin(); it != bCols.end(); ++it) colKeys.insert(it.key());

        QStringList sortedColKeys(colKeys.begin(), colKeys.end());
        std::sort(sortedColKeys.begin(), sortedColKeys.end());

        bool anyDiff = false;
        for (const QString& ck : sortedColKeys) {
            const ColumnInfo* ca = aCols.value(ck);
            const ColumnInfo* cb = bCols.value(ck);
            FieldDiff fd;
            fd.fieldName = ca ? ca->name : cb->name;
            if (!ca) {
                fd.status = DiffStatus::OnlyInTarget;
                fd.targetDesc = columnDesc(*cb, homogeneous);
                fd.reasons << QObject::tr("仅目标端有此字段");
                anyDiff = true;
            } else if (!cb) {
                fd.status = DiffStatus::OnlyInSource;
                fd.sourceDesc = columnDesc(*ca, homogeneous);
                fd.reasons << QObject::tr("仅源端有此字段");
                anyDiff = true;
            } else {
                fd.sourceDesc = columnDesc(*ca, homogeneous);
                fd.targetDesc = columnDesc(*cb, homogeneous);
                if (homogeneous) {
                    fd.reasons = columnDiffReasons(*ca, *cb);
                    fd.status  = fd.reasons.isEmpty() ? DiffStatus::Match : DiffStatus::Differ;
                } else {
                    // 异构模式：仅字段名一致即视为相同
                    fd.status = DiffStatus::Match;
                }
                if (fd.status != DiffStatus::Match) anyDiff = true;
            }
            td.fieldDiffs << fd;
        }

        if (homogeneous) {
            if (a->columns.size() != b->columns.size()) {
                td.reasons << QObject::tr("字段数: %1 ≠ %2")
                                  .arg(a->columns.size()).arg(b->columns.size());
                anyDiff = true;
            }
            if (a->collation != b->collation && !a->collation.isEmpty() && !b->collation.isEmpty()) {
                td.reasons << QObject::tr("表 collation: %1 ≠ %2")
                                  .arg(a->collation, b->collation);
                anyDiff = true;
            }
        } else {
            if (a->columns.size() != b->columns.size()) {
                td.reasons << QObject::tr("字段数: %1 ≠ %2")
                                  .arg(a->columns.size()).arg(b->columns.size());
                anyDiff = true;
            }
        }

        td.status = anyDiff ? DiffStatus::Differ : DiffStatus::Match;
        diffs << td;
    }
    return diffs;
}

}   // namespace

// ──────────────────────────────────────────────────────────────
// UI
// ──────────────────────────────────────────────────────────────

DbCompare::DbCompare() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
}

DbCompare::~DbCompare() = default;

void DbCompare::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
        }
        QSpinBox:focus, QComboBox:focus, QLineEdit:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QLabel { background: transparent; }
        QTreeWidget { border: 1px solid #dee2e6; border-radius: 4px; background: #fff; }
        QHeaderView::section { background: #f1f3f5; padding: 4px 6px; border: none;
            border-bottom: 1px solid #dee2e6; font-weight: bold; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(10, 8, 10, 8);
    main->setSpacing(8);

    // 历史配置行：保存源/目标连接对，方便下次再用
    auto* histRow = new QHBoxLayout();
    auto* histLbl = new QLabel(tr("已保存连接:"));
    histLbl->setStyleSheet("color:#495057;");
    histRow->addWidget(histLbl);
    m_historyCombo = new QComboBox();
    m_historyCombo->setEditable(true);
    m_historyCombo->setInsertPolicy(QComboBox::NoInsert);
    m_historyCombo->setMinimumWidth(280);
    m_historyCombo->lineEdit()->setPlaceholderText(tr("保存后这里会出现下拉选项"));
    histRow->addWidget(m_historyCombo);
    m_historySaveBtn = new QPushButton(tr("💾 保存当前"));
    m_historyDelBtn  = new QPushButton(tr("🗑 删除"));
    histRow->addWidget(m_historySaveBtn);
    histRow->addWidget(m_historyDelBtn);
    histRow->addStretch();
    main->addLayout(histRow);

    // 顶部：两个连接卡
    auto* connRow = new QHBoxLayout();
    connRow->setSpacing(8);
    connRow->addWidget(buildConnectionPanel(tr("🔵 源数据库"),  true), 1);
    connRow->addWidget(buildConnectionPanel(tr("🟢 目标数据库"), false), 1);
    main->addLayout(connRow);

    // 中间控制行
    auto* ctlRow = new QHBoxLayout();
    m_compareBtn = new QPushButton(tr("🔍 开始比对"));
    m_compareBtn->setObjectName("primaryBtn");
    ctlRow->addWidget(m_compareBtn);
    m_modeLabel = new QLabel();
    m_modeLabel->setStyleSheet("color:#495057; padding:0 8px;");
    ctlRow->addWidget(m_modeLabel);
    ctlRow->addStretch();
    m_onlyDiffCheck = new QCheckBox(tr("只显示差异"));
    ctlRow->addWidget(m_onlyDiffCheck);
    m_summaryLbl = new QLabel();
    m_summaryLbl->setStyleSheet("color:#495057;");
    ctlRow->addWidget(m_summaryLbl);
    main->addLayout(ctlRow);

    // 结果区
    auto* split = new QSplitter(Qt::Vertical);
    m_resultTree = new QTreeWidget();
    m_resultTree->setColumnCount(4);
    m_resultTree->setHeaderLabels({tr("对象"), tr("状态"), tr("源端"), tr("目标端")});
    m_resultTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_resultTree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_resultTree->setColumnWidth(2, 240);
    m_resultTree->setColumnWidth(3, 240);
    m_resultTree->setAlternatingRowColors(true);
    split->addWidget(m_resultTree);

    m_logText = new QPlainTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(120);
    m_logText->setStyleSheet(
        "background:#212529; color:#e9ecef; font-family:'SF Mono','Consolas',monospace; font-size:9pt;");
    m_logText->setPlaceholderText(tr("操作日志会显示在这里"));
    split->addWidget(m_logText);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    main->addWidget(split, 1);

    // 信号
    connect(m_srcTestBtn, &QPushButton::clicked, this, &DbCompare::onTestSource);
    connect(m_tgtTestBtn, &QPushButton::clicked, this, &DbCompare::onTestTarget);
    connect(m_compareBtn, &QPushButton::clicked, this, &DbCompare::onCompare);
    connect(m_srcType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DbCompare::onSourceTypeChanged);
    connect(m_tgtType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DbCompare::onTargetTypeChanged);
    connect(m_onlyDiffCheck, &QCheckBox::toggled, this, [this](bool on){
        // 递归隐藏 Match 的表
        for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_resultTree->topLevelItem(i);
            bool isMatch = item->data(0, Qt::UserRole).toInt() == int(DiffStatus::Match);
            item->setHidden(on && isMatch);
        }
    });

    // PostgreSQL：从下拉里选定数据库后，自动重连一次拉 schemas
    auto rebindSchemas = [this](bool source) {
        ConnectionConfig c = readConfig(source);
        if (c.type != DbType::PostgreSQL || c.database.isEmpty()) return;
        QString err;
        QStringList dbs, schemas;
        if (runListDbsAndSchemas(c, dbs, schemas, &err)) {
            populateCombos(source, dbs, schemas);
        }
    };
    connect(m_srcDb, QOverload<int>::of(&QComboBox::activated), this,
            [rebindSchemas](int){ rebindSchemas(true); });
    connect(m_tgtDb, QOverload<int>::of(&QComboBox::activated), this,
            [rebindSchemas](int){ rebindSchemas(false); });

    onSourceTypeChanged(m_srcType->currentIndex());
    onTargetTypeChanged(m_tgtType->currentIndex());

    // 历史相关信号
    connect(m_historySaveBtn, &QPushButton::clicked, this, &DbCompare::onSaveHistory);
    connect(m_historyDelBtn,  &QPushButton::clicked, this, &DbCompare::onDeleteHistory);
    connect(m_historyCombo, QOverload<int>::of(&QComboBox::activated),
            this, &DbCompare::onLoadHistoryActivated);
    reloadHistoryCombo();
}

QWidget* DbCompare::buildConnectionPanel(const QString& title, bool isSource)
{
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(4);

    auto* hdr = new QLabel(title);
    hdr->setStyleSheet("font-weight:bold; color:#495057; font-size:10pt;");
    v->addWidget(hdr);

    auto* typeCombo = new QComboBox();
    typeCombo->addItem(QString("MySQL%1").arg(driverCompiled(DbType::MySQL)?"":"  (未编译)"), int(DbType::MySQL));
    typeCombo->addItem(QString("PostgreSQL%1").arg(driverCompiled(DbType::PostgreSQL)?"":"  (未编译)"), int(DbType::PostgreSQL));
    typeCombo->addItem(QString("Oracle%1").arg(driverCompiled(DbType::Oracle)?"":"  (未编译)"), int(DbType::Oracle));

    auto* hostEdit = new QLineEdit("127.0.0.1");
    auto* portSpin = new QSpinBox();
    portSpin->setRange(1, 65535);
    portSpin->setValue(3306);

    auto* userEdit = new QLineEdit();
    userEdit->setPlaceholderText(tr("用户名"));
    auto* pwdEdit = new QLineEdit();
    pwdEdit->setEchoMode(QLineEdit::Password);
    pwdEdit->setPlaceholderText(tr("密码"));

    auto* dbCombo = new QComboBox();
    dbCombo->setEditable(true);
    dbCombo->setInsertPolicy(QComboBox::NoInsert);
    dbCombo->lineEdit()->setPlaceholderText(tr("数据库 / Service Name（测试后会自动列出）"));
    auto* schemaCombo = new QComboBox();
    schemaCombo->setEditable(true);
    schemaCombo->setInsertPolicy(QComboBox::NoInsert);
    schemaCombo->lineEdit()->setPlaceholderText(tr("Schema / Owner（测试后会自动列出）"));

    auto* testBtn = new QPushButton(tr("测试连接"));
    auto* statusLbl = new QLabel();
    statusLbl->setStyleSheet("color:#868e96; font-size:9pt;");
    statusLbl->setWordWrap(true);
    statusLbl->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statusLbl->setCursor(Qt::IBeamCursor);

    // 行容器（用 QWidget 包一层，整行能 hide/show + 可保留 label 引用）
    auto addRow = [&](const QString& label, QWidget* w, QLabel** outLabel = nullptr) -> QWidget* {
        auto* rowW = new QWidget();
        auto* row = new QHBoxLayout(rowW);
        row->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(label);
        l->setMinimumWidth(72);
        l->setStyleSheet("color:#495057;");
        row->addWidget(l);
        row->addWidget(w, 1);
        v->addWidget(rowW);
        if (outLabel) *outLabel = l;
        return rowW;
    };
    addRow(tr("类型"), typeCombo);

    {
        auto* rowW = new QWidget();
        auto* row = new QHBoxLayout(rowW);
        row->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(tr("主机/端口"));
        l->setMinimumWidth(72);
        l->setStyleSheet("color:#495057;");
        row->addWidget(l);
        row->addWidget(hostEdit, 3);
        row->addWidget(portSpin, 1);
        v->addWidget(rowW);
    }

    addRow(tr("用户"), userEdit);
    addRow(tr("密码"), pwdEdit);

    QLabel* dbLbl = nullptr;
    addRow(tr("数据库"), dbCombo, &dbLbl);

    QLabel* schLbl = nullptr;
    QWidget* schRow = addRow(tr("Schema"), schemaCombo, &schLbl);

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(testBtn);
    btnRow->addStretch();
    v->addLayout(btnRow);
    v->addWidget(statusLbl);

    if (isSource) {
        m_srcType = typeCombo; m_srcHost = hostEdit; m_srcPort = portSpin;
        m_srcUser = userEdit; m_srcPwd  = pwdEdit;
        m_srcDb   = dbCombo;  m_srcSchema = schemaCombo;
        m_srcDbLabel = dbLbl; m_srcSchemaLabel = schLbl; m_srcSchemaRow = schRow;
        m_srcTestBtn = testBtn; m_srcStatus = statusLbl;
    } else {
        m_tgtType = typeCombo; m_tgtHost = hostEdit; m_tgtPort = portSpin;
        m_tgtUser = userEdit; m_tgtPwd  = pwdEdit;
        m_tgtDb   = dbCombo;  m_tgtSchema = schemaCombo;
        m_tgtDbLabel = dbLbl; m_tgtSchemaLabel = schLbl; m_tgtSchemaRow = schRow;
        m_tgtTestBtn = testBtn; m_tgtStatus = statusLbl;
    }
    return card;
}

ConnectionConfig DbCompare::readConfig(bool source) const
{
    ConnectionConfig c;
    if (source) {
        c.type     = static_cast<DbType>(m_srcType->currentData().toInt());
        c.host     = m_srcHost->text();
        c.port     = m_srcPort->value();
        c.user     = m_srcUser->text();
        c.password = m_srcPwd->text();
        c.database = m_srcDb->currentText().trimmed();
        c.schema   = m_srcSchema->currentText().trimmed();
    } else {
        c.type     = static_cast<DbType>(m_tgtType->currentData().toInt());
        c.host     = m_tgtHost->text();
        c.port     = m_tgtPort->value();
        c.user     = m_tgtUser->text();
        c.password = m_tgtPwd->text();
        c.database = m_tgtDb->currentText().trimmed();
        c.schema   = m_tgtSchema->currentText().trimmed();
    }
    return c;
}

namespace {
// 根据 DB 类型设置 db / schema 字段的标签和可见性
void applyDbTypeLabels(DbType t,
                       QLabel* dbLbl, QComboBox* dbCombo,
                       QLabel* schemaLbl, QComboBox* schemaCombo,
                       QWidget* schemaRow)
{
    switch (t) {
        case DbType::MySQL:
            if (dbLbl) dbLbl->setText(QObject::tr("数据库"));
            if (dbCombo) dbCombo->lineEdit()->setPlaceholderText(
                QObject::tr("数据库名（测试后会自动列出）"));
            if (schemaRow) schemaRow->setVisible(false);   // MySQL 无 schema 概念
            break;
        case DbType::PostgreSQL:
            if (dbLbl) dbLbl->setText(QObject::tr("数据库"));
            if (dbCombo) dbCombo->lineEdit()->setPlaceholderText(
                QObject::tr("数据库名（不填则用 postgres 占位连接列库）"));
            if (schemaLbl) schemaLbl->setText(QObject::tr("Schema"));
            if (schemaCombo) schemaCombo->lineEdit()->setPlaceholderText(
                QObject::tr("Schema 名（默认 public）"));
            if (schemaRow) schemaRow->setVisible(true);
            break;
        case DbType::Oracle:
            if (dbLbl) dbLbl->setText(QObject::tr("Service"));
            if (dbCombo) dbCombo->lineEdit()->setPlaceholderText(
                QObject::tr("Service Name 或 SID"));
            if (schemaLbl) schemaLbl->setText(QObject::tr("Owner"));
            if (schemaCombo) schemaCombo->lineEdit()->setPlaceholderText(
                QObject::tr("Schema/Owner（大写，留空用登录用户）"));
            if (schemaRow) schemaRow->setVisible(true);
            break;
    }
}
}   // namespace

void DbCompare::onSourceTypeChanged(int)
{
    if (!m_srcType || !m_srcPort) return;
    DbType t = static_cast<DbType>(m_srcType->currentData().toInt());
    m_srcPort->setValue(defaultPort(t));
    applyDbTypeLabels(t, m_srcDbLabel, m_srcDb,
                      m_srcSchemaLabel, m_srcSchema, m_srcSchemaRow);
    if (m_modeLabel) {
        DbType s = static_cast<DbType>(m_srcType->currentData().toInt());
        DbType g = m_tgtType ? static_cast<DbType>(m_tgtType->currentData().toInt()) : s;
        m_modeLabel->setText(s == g
            ? tr("模式：同构（比对表名/数量/字段/类型/长度/字符集）")
            : tr("模式：异构（仅比对表名/数量/字段名）"));
    }
}

void DbCompare::onTargetTypeChanged(int)
{
    if (!m_tgtType || !m_tgtPort) return;
    DbType t = static_cast<DbType>(m_tgtType->currentData().toInt());
    m_tgtPort->setValue(defaultPort(t));
    applyDbTypeLabels(t, m_tgtDbLabel, m_tgtDb,
                      m_tgtSchemaLabel, m_tgtSchema, m_tgtSchemaRow);
    onSourceTypeChanged(0);   // 刷新 mode 标签
}

void DbCompare::setBusy(bool busy)
{
    m_compareBtn->setEnabled(!busy);
    m_srcTestBtn->setEnabled(!busy);
    m_tgtTestBtn->setEnabled(!busy);
}

void DbCompare::onTestSource()
{
    ConnectionConfig c = readConfig(true);
    if (!driverCompiled(c.type)) {
        m_srcStatus->setText(tr("❌ %1 驱动未编译").arg(driverDisplayName(c.type)));
        m_srcStatus->setStyleSheet("color:#c92a2a; font-size:9pt;");
        return;
    }
    m_srcStatus->setText(tr("⏳ 连接中…"));
    m_srcStatus->setStyleSheet("color:#868e96; font-size:9pt;");
    qApp->processEvents();

    QString err;
    QStringList dbs, schemas;
    bool ok = runListDbsAndSchemas(c, dbs, schemas, &err);
    if (ok) {
        populateCombos(true, dbs, schemas);
        QString hint = (c.type == DbType::PostgreSQL && c.database.isEmpty())
            ? tr("（选定数据库后再点测试可拉取 schemas）") : QString();
        m_srcStatus->setText(tr("✅ 连接成功 · 数据库 %1 · Schema %2 %3")
            .arg(dbs.size()).arg(schemas.size()).arg(hint));
        m_srcStatus->setStyleSheet("color:#2f9e44; font-size:9pt;");
        m_logText->appendPlainText(tr("[源] 连接成功 · 数据库 %1 · Schema %2")
            .arg(dbs.size()).arg(schemas.size()));
    } else {
        m_srcStatus->setText(tr("❌ %1").arg(err));
        m_srcStatus->setStyleSheet("color:#c92a2a; font-size:9pt;");
        m_logText->appendPlainText(tr("[源] 连接失败：%1").arg(err));
    }
}

void DbCompare::onTestTarget()
{
    ConnectionConfig c = readConfig(false);
    if (!driverCompiled(c.type)) {
        m_tgtStatus->setText(tr("❌ %1 驱动未编译").arg(driverDisplayName(c.type)));
        m_tgtStatus->setStyleSheet("color:#c92a2a; font-size:9pt;");
        return;
    }
    m_tgtStatus->setText(tr("⏳ 连接中…"));
    m_tgtStatus->setStyleSheet("color:#868e96; font-size:9pt;");
    qApp->processEvents();

    QString err;
    QStringList dbs, schemas;
    bool ok = runListDbsAndSchemas(c, dbs, schemas, &err);
    if (ok) {
        populateCombos(false, dbs, schemas);
        QString hint = (c.type == DbType::PostgreSQL && c.database.isEmpty())
            ? tr("（选定数据库后再点测试可拉取 schemas）") : QString();
        m_tgtStatus->setText(tr("✅ 连接成功 · 数据库 %1 · Schema %2 %3")
            .arg(dbs.size()).arg(schemas.size()).arg(hint));
        m_tgtStatus->setStyleSheet("color:#2f9e44; font-size:9pt;");
        m_logText->appendPlainText(tr("[目标] 连接成功 · 数据库 %1 · Schema %2")
            .arg(dbs.size()).arg(schemas.size()));
    } else {
        m_tgtStatus->setText(tr("❌ %1").arg(err));
        m_tgtStatus->setStyleSheet("color:#c92a2a; font-size:9pt;");
        m_logText->appendPlainText(tr("[目标] 连接失败：%1").arg(err));
    }
}

void DbCompare::onCompare()
{
    ConnectionConfig srcCfg = readConfig(true);
    ConnectionConfig tgtCfg = readConfig(false);
    if (!driverCompiled(srcCfg.type) || !driverCompiled(tgtCfg.type)) {
        QMessageBox::warning(this, tr("驱动未编译"),
            tr("源或目标的数据库驱动未编译进当前构建。\n"
               "重新构建时加：\n"
               "  cmake -DWITH_LIBPQ=ON -DWITH_ORACLE_OCI=ON ..."));
        return;
    }

    setBusy(true);
    m_logText->appendPlainText(tr("======== 开始比对 ========"));
    qApp->processEvents();

    QString srcErr, tgtErr;
    QList<TableInfo> srcTables, tgtTables;
    bool ok1 = runFetchSchema(srcCfg, "compare_src", srcTables, &srcErr);
    if (!ok1) {
        m_logText->appendPlainText(tr("[源] 失败：%1").arg(srcErr));
        QMessageBox::warning(this, tr("源端连接失败"), srcErr);
        setBusy(false);
        return;
    }
    m_logText->appendPlainText(tr("[源] 获取 %1 张表").arg(srcTables.size()));

    bool ok2 = runFetchSchema(tgtCfg, "compare_tgt", tgtTables, &tgtErr);
    if (!ok2) {
        m_logText->appendPlainText(tr("[目标] 失败：%1").arg(tgtErr));
        QMessageBox::warning(this, tr("目标端连接失败"), tgtErr);
        setBusy(false);
        return;
    }
    m_logText->appendPlainText(tr("[目标] 获取 %1 张表").arg(tgtTables.size()));

    bool homogeneous = (srcCfg.type == tgtCfg.type);
    QList<TableDiff> diffs = compareSchemas(srcTables, tgtTables, homogeneous);

    m_logText->appendPlainText(tr("比对完成 · 共 %1 项 · 模式：%2")
        .arg(diffs.size())
        .arg(homogeneous ? tr("同构") : tr("异构")));

    renderResults(diffs, homogeneous);
    setBusy(false);
}

void DbCompare::renderResults(const QList<TableDiff>& diffs, bool homogeneous)
{
    m_resultTree->clear();

    int matchCount = 0, diffCount = 0, srcOnly = 0, tgtOnly = 0;

    for (const auto& td : diffs) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, td.tableName);
        item->setText(1, diskStatusText(td.status));
        item->setForeground(1, QBrush(statusColor(td.status)));
        item->setData(0, Qt::UserRole, int(td.status));

        // 源/目标列描述
        if (td.status == DiffStatus::OnlyInSource) {
            item->setText(2, tr("✓ %1 字段").arg(td.sourceColumnCount));
            item->setText(3, tr("— 不存在 —"));
        } else if (td.status == DiffStatus::OnlyInTarget) {
            item->setText(2, tr("— 不存在 —"));
            item->setText(3, tr("✓ %1 字段").arg(td.targetColumnCount));
        } else {
            item->setText(2, tr("%1 字段").arg(td.sourceColumnCount));
            item->setText(3, tr("%1 字段").arg(td.targetColumnCount));
        }

        // 表级 reasons
        for (const QString& r : td.reasons) {
            auto* rItem = new QTreeWidgetItem();
            rItem->setText(0, "⚠ " + r);
            rItem->setForeground(0, QBrush(QColor(0xe6, 0x77, 0x00)));
            item->addChild(rItem);
        }
        // 字段级 diffs
        for (const FieldDiff& fd : td.fieldDiffs) {
            if (fd.status == DiffStatus::Match) continue;
            auto* fItem = new QTreeWidgetItem();
            fItem->setText(0, "  " + fd.fieldName);
            fItem->setText(1, diskStatusText(fd.status));
            fItem->setForeground(1, QBrush(statusColor(fd.status)));
            fItem->setText(2, fd.sourceDesc);
            fItem->setText(3, fd.targetDesc);
            // reasons 作为子项
            for (const QString& r : fd.reasons) {
                auto* rItem = new QTreeWidgetItem();
                rItem->setText(0, "    · " + r);
                rItem->setForeground(0, QBrush(QColor(0x86, 0x8e, 0x96)));
                fItem->addChild(rItem);
            }
            item->addChild(fItem);
        }
        m_resultTree->addTopLevelItem(item);
        if (td.status != DiffStatus::Match) item->setExpanded(true);

        switch (td.status) {
            case DiffStatus::Match:        matchCount++; break;
            case DiffStatus::OnlyInSource: srcOnly++;    break;
            case DiffStatus::OnlyInTarget: tgtOnly++;    break;
            case DiffStatus::Differ:       diffCount++;  break;
        }
    }

    m_summaryLbl->setText(tr("一致 %1 · 差异 %2 · 仅源 %3 · 仅目标 %4 · 模式 %5")
        .arg(matchCount).arg(diffCount).arg(srcOnly).arg(tgtOnly)
        .arg(homogeneous ? tr("同构") : tr("异构")));

    // 应用"只看差异"过滤
    if (m_onlyDiffCheck->isChecked()) {
        for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* it = m_resultTree->topLevelItem(i);
            if (it->data(0, Qt::UserRole).toInt() == int(DiffStatus::Match))
                it->setHidden(true);
        }
    }
}


// ──────────────────────────────────────────────────────────────
// 历史连接配置（保存到 QSettings 的 DbCompare/history 分组下）
// 仅保存连接信息，不保存比对结果。
// ──────────────────────────────────────────────────────────────

namespace {

QJsonObject configToJson(const ConnectionConfig& c) {
    QJsonObject o;
    o["type"]     = int(c.type);
    o["host"]     = c.host;
    o["port"]     = c.port;
    o["user"]     = c.user;
    o["password"] = c.password;     // 本地 QSettings 明文保存，按本地工具习惯
    o["database"] = c.database;
    o["schema"]   = c.schema;
    return o;
}

ConnectionConfig configFromJson(const QJsonObject& j) {
    ConnectionConfig c;
    c.type     = static_cast<DbType>(j.value("type").toInt(0));
    c.host     = j.value("host").toString();
    c.port     = j.value("port").toInt(0);
    c.user     = j.value("user").toString();
    c.password = j.value("password").toString();
    c.database = j.value("database").toString();
    c.schema   = j.value("schema").toString();
    return c;
}

}   // namespace

void DbCompare::applyConfig(bool source, const ConnectionConfig& c)
{
    QComboBox* typeCombo = source ? m_srcType : m_tgtType;
    int idx = typeCombo->findData(int(c.type));
    if (idx >= 0) typeCombo->setCurrentIndex(idx);

    if (source) {
        m_srcHost->setText(c.host);
        m_srcPort->setValue(c.port > 0 ? c.port : defaultPort(c.type));
        m_srcUser->setText(c.user);
        m_srcPwd->setText(c.password);
        m_srcDb->setEditText(c.database);
        m_srcSchema->setEditText(c.schema);
    } else {
        m_tgtHost->setText(c.host);
        m_tgtPort->setValue(c.port > 0 ? c.port : defaultPort(c.type));
        m_tgtUser->setText(c.user);
        m_tgtPwd->setText(c.password);
        m_tgtDb->setEditText(c.database);
        m_tgtSchema->setEditText(c.schema);
    }
}

void DbCompare::reloadHistoryCombo()
{
    QSettings s;
    s.beginGroup("DbCompare/history");
    QStringList names = s.childKeys();
    s.endGroup();
    names.sort();

    QSignalBlocker block(m_historyCombo);
    QString cur = m_historyCombo->currentText();
    m_historyCombo->clear();
    m_historyCombo->addItems(names);
    int idx = m_historyCombo->findText(cur);
    if (idx >= 0) m_historyCombo->setCurrentIndex(idx);
    else          m_historyCombo->setEditText(cur);
}

void DbCompare::onSaveHistory()
{
    QString name = m_historyCombo->currentText().trimmed();
    bool overwrite = false;
    if (!name.isEmpty()) {
        QSettings s;
        s.beginGroup("DbCompare/history");
        overwrite = s.contains(name);
        s.endGroup();
    }
    if (name.isEmpty()) {
        bool ok = false;
        QString defName = QString("配置_%1").arg(
            QDateTime::currentDateTime().toString("MMdd_HHmm"));
        name = QInputDialog::getText(this, tr("保存连接配置"),
            tr("给这套连接起个名字："), QLineEdit::Normal, defName, &ok);
        if (!ok) return;
        name = name.trimmed();
        if (name.isEmpty()) return;
    } else if (overwrite) {
        if (QMessageBox::question(this, tr("覆盖确认"),
                tr("「%1」已存在，覆盖吗？").arg(name))
            != QMessageBox::Yes) return;
    }

    QJsonObject pair;
    pair["source"] = configToJson(readConfig(true));
    pair["target"] = configToJson(readConfig(false));
    QJsonDocument doc(pair);

    QSettings s;
    s.beginGroup("DbCompare/history");
    s.setValue(name, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    s.endGroup();

    reloadHistoryCombo();
    int idx = m_historyCombo->findText(name);
    if (idx >= 0) m_historyCombo->setCurrentIndex(idx);
    m_logText->appendPlainText(tr("[历史] 已保存：%1").arg(name));
}

void DbCompare::onDeleteHistory()
{
    QString name = m_historyCombo->currentText().trimmed();
    if (name.isEmpty()) return;
    QSettings s;
    s.beginGroup("DbCompare/history");
    if (!s.contains(name)) {
        m_logText->appendPlainText(tr("[历史] 「%1」不存在，忽略").arg(name));
        return;
    }
    s.endGroup();

    if (QMessageBox::question(this, tr("删除确认"),
            tr("确定删除连接配置「%1」？").arg(name))
        != QMessageBox::Yes) return;

    s.beginGroup("DbCompare/history");
    s.remove(name);
    s.endGroup();
    reloadHistoryCombo();
    m_logText->appendPlainText(tr("[历史] 已删除：%1").arg(name));
}

void DbCompare::onLoadHistoryActivated(int idx)
{
    QString name = m_historyCombo->itemText(idx).trimmed();
    if (name.isEmpty()) return;

    QSettings s;
    s.beginGroup("DbCompare/history");
    QString json = s.value(name).toString();
    s.endGroup();
    if (json.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        m_logText->appendPlainText(tr("[历史] 「%1」数据损坏").arg(name));
        return;
    }
    QJsonObject pair = doc.object();
    applyConfig(true,  configFromJson(pair.value("source").toObject()));
    applyConfig(false, configFromJson(pair.value("target").toObject()));
    m_logText->appendPlainText(tr("[历史] 已加载：%1").arg(name));
}
