#pragma once

#include <QString>
#include <QHash>
#include <QSet>
#include "sqltypes.h"

// ============================================================
// SQL Dialect — 方言接口（类似 LLVM Target）
//
// 每个数据库方言定义：
// 1. 关键字集合及映射
// 2. 标识符引用规则（``, "", []）
// 3. 字符串转义规则
// 4. 特有运算符
// 5. 特有语法的解析 hook
// ============================================================

class SqlDialect {
public:
    virtual ~SqlDialect() = default;

    // 方言类型标识
    virtual SqlDialectType type() const = 0;
    virtual QString name() const = 0;

    // ── 关键字 ──
    // 返回 keyword → token type 的映射
    virtual QHash<QString, SqlTokenType> keywords() const {
        return ansiKeywords();
    }

    // 是否是该方言的保留关键字（不可作为标识符）
    virtual bool isReservedWord(const QString& word) const {
        return keywords().contains(word.toUpper());
    }

    // ── 引用规则 ──
    virtual QChar identifierQuoteChar() const { return '"'; }  // ANSI 标准
    virtual QChar stringQuoteChar() const { return '\''; }
    virtual QString escapeString(const QString& str) const {
        QString result = str;
        result.replace("'", "''"); // ANSI 标准转义
        return result;
    }

    // ── 特有运算符 ──
    virtual bool hasOperator(const QString& op) const {
        Q_UNUSED(op)
        return false;
    }

    // ── 格式化偏好 ──
    virtual bool uppercaseKeywords() const { return true; }
    virtual int defaultIndent() const { return 2; }

    // ANSI SQL 标准关键字（public 让 Lexer 直接使用）
    static QHash<QString, SqlTokenType> ansiKeywords() {
        static QHash<QString, SqlTokenType> kw = {
            {"SELECT", SqlTokenType::KW_SELECT}, {"FROM", SqlTokenType::KW_FROM},
            {"WHERE", SqlTokenType::KW_WHERE}, {"INSERT", SqlTokenType::KW_INSERT},
            {"INTO", SqlTokenType::KW_INTO}, {"VALUES", SqlTokenType::KW_VALUES},
            {"UPDATE", SqlTokenType::KW_UPDATE}, {"SET", SqlTokenType::KW_SET},
            {"DELETE", SqlTokenType::KW_DELETE}, {"CREATE", SqlTokenType::KW_CREATE},
            {"ALTER", SqlTokenType::KW_ALTER}, {"DROP", SqlTokenType::KW_DROP},
            {"TABLE", SqlTokenType::KW_TABLE}, {"INDEX", SqlTokenType::KW_INDEX},
            {"VIEW", SqlTokenType::KW_VIEW}, {"DATABASE", SqlTokenType::KW_DATABASE},
            {"SCHEMA", SqlTokenType::KW_SCHEMA},
            {"AND", SqlTokenType::KW_AND}, {"OR", SqlTokenType::KW_OR},
            {"NOT", SqlTokenType::KW_NOT}, {"IN", SqlTokenType::KW_IN},
            {"BETWEEN", SqlTokenType::KW_BETWEEN}, {"LIKE", SqlTokenType::KW_LIKE},
            {"IS", SqlTokenType::KW_IS}, {"NULL", SqlTokenType::KW_NULL_KW},
            {"TRUE", SqlTokenType::KW_TRUE_KW}, {"FALSE", SqlTokenType::KW_FALSE_KW},
            {"AS", SqlTokenType::KW_AS}, {"ON", SqlTokenType::KW_ON},
            {"JOIN", SqlTokenType::KW_JOIN}, {"INNER", SqlTokenType::KW_INNER},
            {"LEFT", SqlTokenType::KW_LEFT}, {"RIGHT", SqlTokenType::KW_RIGHT},
            {"FULL", SqlTokenType::KW_FULL}, {"OUTER", SqlTokenType::KW_OUTER},
            {"CROSS", SqlTokenType::KW_CROSS}, {"NATURAL", SqlTokenType::KW_NATURAL},
            {"ORDER", SqlTokenType::KW_ORDER}, {"BY", SqlTokenType::KW_BY},
            {"GROUP", SqlTokenType::KW_GROUP}, {"HAVING", SqlTokenType::KW_HAVING},
            {"ASC", SqlTokenType::KW_ASC}, {"DESC", SqlTokenType::KW_DESC},
            {"LIMIT", SqlTokenType::KW_LIMIT}, {"OFFSET", SqlTokenType::KW_OFFSET},
            {"UNION", SqlTokenType::KW_UNION}, {"INTERSECT", SqlTokenType::KW_INTERSECT},
            {"EXCEPT", SqlTokenType::KW_EXCEPT}, {"DISTINCT", SqlTokenType::KW_DISTINCT},
            {"CASE", SqlTokenType::KW_CASE}, {"WHEN", SqlTokenType::KW_WHEN},
            {"THEN", SqlTokenType::KW_THEN}, {"ELSE", SqlTokenType::KW_ELSE},
            {"END", SqlTokenType::KW_END}, {"EXISTS", SqlTokenType::KW_EXISTS},
            {"PRIMARY", SqlTokenType::KW_PRIMARY}, {"KEY", SqlTokenType::KW_KEY},
            {"FOREIGN", SqlTokenType::KW_FOREIGN}, {"REFERENCES", SqlTokenType::KW_REFERENCES},
            {"UNIQUE", SqlTokenType::KW_UNIQUE}, {"CHECK", SqlTokenType::KW_CHECK},
            {"DEFAULT", SqlTokenType::KW_DEFAULT}, {"CONSTRAINT", SqlTokenType::KW_CONSTRAINT},
            {"IF", SqlTokenType::KW_IF}, {"CASCADE", SqlTokenType::KW_CASCADE},
            {"WITH", SqlTokenType::KW_WITH}, {"RECURSIVE", SqlTokenType::KW_RECURSIVE},
            {"USE", SqlTokenType::KW_USE}, {"EXPLAIN", SqlTokenType::KW_EXPLAIN},
            {"BEGIN", SqlTokenType::KW_BEGIN}, {"COMMIT", SqlTokenType::KW_COMMIT},
            {"ROLLBACK", SqlTokenType::KW_ROLLBACK},
            {"COUNT", SqlTokenType::KW_COUNT}, {"SUM", SqlTokenType::KW_SUM},
            {"AVG", SqlTokenType::KW_AVG}, {"MIN", SqlTokenType::KW_MIN},
            {"MAX", SqlTokenType::KW_MAX},
            {"TRIGGER", SqlTokenType::KW_TRIGGER}, {"PROCEDURE", SqlTokenType::KW_PROCEDURE},
            {"FUNCTION", SqlTokenType::KW_FUNCTION},
            {"USING", SqlTokenType::KW_USING}, {"ALL", SqlTokenType::KW_ALL},
            {"ANY", SqlTokenType::KW_ANY}, {"SOME", SqlTokenType::KW_SOME},
            {"COMMENT", SqlTokenType::KW_COMMENT},
            {"SHOW", SqlTokenType::KW_SHOW}, {"DESCRIBE", SqlTokenType::KW_DESCRIBE},
            {"MERGE", SqlTokenType::KW_MERGE},
        };
        return kw;
    }
};

// ── MySQL 方言 ──

class MySqlDialect : public SqlDialect {
public:
    SqlDialectType type() const override { return SqlDialectType::MySQL; }
    QString name() const override { return "MySQL"; }
    QChar identifierQuoteChar() const override { return '`'; }

    QHash<QString, SqlTokenType> keywords() const override {
        auto kw = ansiKeywords();
        // MySQL 特有关键字
        kw["AUTO_INCREMENT"] = SqlTokenType::KW_AUTO_INCREMENT;
        kw["ENGINE"] = SqlTokenType::KW_DIALECT_0;
        kw["CHARSET"] = SqlTokenType::KW_DIALECT_1;
        kw["COLLATE"] = SqlTokenType::KW_DIALECT_2;
        kw["UNSIGNED"] = SqlTokenType::KW_DIALECT_3;
        kw["ZEROFILL"] = SqlTokenType::KW_DIALECT_4;
        kw["ENUM"] = SqlTokenType::KW_DIALECT_5;
        kw["DUPLICATE"] = SqlTokenType::KW_DIALECT_6;
        kw["IGNORE"] = SqlTokenType::KW_DIALECT_7;
        kw["REPLACE"] = SqlTokenType::KW_DIALECT_8;
        return kw;
    }

    QString escapeString(const QString& str) const override {
        QString result = str;
        result.replace("\\", "\\\\");
        result.replace("'", "\\'");
        return result;
    }
};

// ── PostgreSQL 方言 ──

class PgDialect : public SqlDialect {
public:
    SqlDialectType type() const override { return SqlDialectType::PostgreSQL; }
    QString name() const override { return "PostgreSQL"; }

    QHash<QString, SqlTokenType> keywords() const override {
        auto kw = ansiKeywords();
        kw["RETURNING"] = SqlTokenType::KW_DIALECT_0;
        kw["ILIKE"] = SqlTokenType::KW_DIALECT_1;
        kw["SERIAL"] = SqlTokenType::KW_DIALECT_2;
        kw["BIGSERIAL"] = SqlTokenType::KW_DIALECT_3;
        kw["JSONB"] = SqlTokenType::KW_DIALECT_4;
        kw["ARRAY"] = SqlTokenType::KW_DIALECT_5;
        return kw;
    }

    bool hasOperator(const QString& op) const override {
        return op == "::" || op == "->>" || op == "->" || op == "@>" || op == "<@";
    }
};

// ── SQLite 方言 ──

class SqliteDialect : public SqlDialect {
public:
    SqlDialectType type() const override { return SqlDialectType::SQLite; }
    QString name() const override { return "SQLite"; }

    QHash<QString, SqlTokenType> keywords() const override {
        auto kw = ansiKeywords();
        kw["AUTOINCREMENT"] = SqlTokenType::KW_AUTO_INCREMENT;
        kw["GLOB"] = SqlTokenType::KW_DIALECT_0;
        kw["PRAGMA"] = SqlTokenType::KW_DIALECT_1;
        kw["VACUUM"] = SqlTokenType::KW_DIALECT_2;
        return kw;
    }
};

// ── 方言工厂 ──

inline std::unique_ptr<SqlDialect> createDialect(SqlDialectType type) {
    switch (type) {
    case SqlDialectType::MySQL:      return std::make_unique<MySqlDialect>();
    case SqlDialectType::PostgreSQL: return std::make_unique<PgDialect>();
    case SqlDialectType::SQLite:     return std::make_unique<SqliteDialect>();
    default:                         return std::make_unique<MySqlDialect>(); // 默认 MySQL
    }
}
