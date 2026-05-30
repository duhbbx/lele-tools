#ifndef DBCOMPARE_H
#define DBCOMPARE_H

#include <QWidget>
#include <QString>
#include <QList>

#include "../../common/dynamicobjectbase.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QTreeWidget;
class QPlainTextEdit;
class QCheckBox;

namespace DbCompareNS {

enum class DbType { MySQL, PostgreSQL, Oracle };

struct ConnectionConfig {
    DbType type = DbType::MySQL;
    QString host = "127.0.0.1";
    int port = 3306;
    QString user;
    QString password;
    QString database;   // MySQL: db; PostgreSQL: db; Oracle: service / SID
    QString schema;     // PostgreSQL: schema (default public); Oracle: owner (uppercase)
};

struct ColumnInfo {
    QString name;
    QString dataType;        // 标准化后的类型字符串，如 "VARCHAR(255)"、"NUMBER(10,2)"
    QString rawType;         // 原始类型名
    int length = -1;         // 字符长度
    int precision = -1;      // 数值精度
    int scale = -1;          // 数值小数位
    bool nullable = true;
    QString charset;
    QString collation;
    QString defaultValue;
    QString comment;
};

struct TableInfo {
    QString name;
    QList<ColumnInfo> columns;
    QString collation;       // 表级 collation（MySQL）
    QString engine;          // 引擎（MySQL）
    QString comment;
};

enum class DiffStatus { Match, OnlyInSource, OnlyInTarget, Differ };

struct FieldDiff {
    QString fieldName;
    DiffStatus status = DiffStatus::Match;
    QStringList reasons;    // 差异原因（同构模式下可能多条）
    QString sourceDesc;
    QString targetDesc;
};

struct TableDiff {
    QString tableName;
    DiffStatus status = DiffStatus::Match;
    QStringList reasons;
    int sourceColumnCount = 0;
    int targetColumnCount = 0;
    QList<FieldDiff> fieldDiffs;
};

}   // namespace DbCompareNS


class DbCompare : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit DbCompare();
    ~DbCompare() override;

private slots:
    void onTestSource();
    void onTestTarget();
    void onCompare();
    void onSourceTypeChanged(int idx);
    void onTargetTypeChanged(int idx);
    void onSaveHistory();
    void onDeleteHistory();
    void onLoadHistoryActivated(int idx);

private:
    void setupUI();
    QWidget* buildConnectionPanel(const QString& title, bool isSource);
    DbCompareNS::ConnectionConfig readConfig(bool source) const;
    bool runFetchSchema(const DbCompareNS::ConnectionConfig& cfg,
                        const QString& connName,
                        QList<DbCompareNS::TableInfo>& outTables,
                        QString* errMsg);
    bool runListDbsAndSchemas(const DbCompareNS::ConnectionConfig& cfg,
                              QStringList& dbs, QStringList& schemas,
                              QString* errMsg);
    void populateCombos(bool source,
                        const QStringList& dbs,
                        const QStringList& schemas);
    void reloadHistoryCombo();
    void applyConfig(bool source, const DbCompareNS::ConnectionConfig& c);
    void renderResults(const QList<DbCompareNS::TableDiff>& diffs, bool homogeneous);
    void setBusy(bool busy);

    // Source widgets
    QComboBox*   m_srcType   = nullptr;
    QLineEdit*   m_srcHost   = nullptr;
    QSpinBox*    m_srcPort   = nullptr;
    QLineEdit*   m_srcUser   = nullptr;
    QLineEdit*   m_srcPwd    = nullptr;
    QLabel*      m_srcDbLabel = nullptr;
    QComboBox*   m_srcDb     = nullptr;
    QLabel*      m_srcSchemaLabel = nullptr;
    QComboBox*   m_srcSchema = nullptr;
    QWidget*     m_srcSchemaRow = nullptr;   // schema 整行（label + combo）— 用于隐藏
    QPushButton* m_srcTestBtn= nullptr;
    QLabel*      m_srcStatus = nullptr;

    // Target widgets
    QComboBox*   m_tgtType   = nullptr;
    QLineEdit*   m_tgtHost   = nullptr;
    QSpinBox*    m_tgtPort   = nullptr;
    QLineEdit*   m_tgtUser   = nullptr;
    QLineEdit*   m_tgtPwd    = nullptr;
    QLabel*      m_tgtDbLabel = nullptr;
    QComboBox*   m_tgtDb     = nullptr;
    QLabel*      m_tgtSchemaLabel = nullptr;
    QComboBox*   m_tgtSchema = nullptr;
    QWidget*     m_tgtSchemaRow = nullptr;
    QPushButton* m_tgtTestBtn= nullptr;
    QLabel*      m_tgtStatus = nullptr;

    // History (saved connection pairs)
    QComboBox*   m_historyCombo = nullptr;
    QPushButton* m_historySaveBtn = nullptr;
    QPushButton* m_historyDelBtn = nullptr;

    // Compare controls + results
    QPushButton* m_compareBtn = nullptr;
    QLabel*      m_modeLabel  = nullptr;
    QLabel*      m_summaryLbl = nullptr;
    QTreeWidget* m_resultTree = nullptr;
    QCheckBox*   m_onlyDiffCheck = nullptr;
    QPlainTextEdit* m_logText = nullptr;
};

#endif // DBCOMPARE_H
