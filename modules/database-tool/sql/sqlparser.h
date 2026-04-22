#pragma once

#include <QString>
#include <QList>
#include <memory>
#include "sqltoken.h"
#include "sqlast.h"
#include "sqldialect.h"
#include "sqllexer.h"

// ============================================================
// SQL Parser — 递归下降解析器
//
// 设计：
// - 基类处理 ANSI SQL 标准语法
// - 方言通过虚函数 hook 扩展特有语法
// - 解析错误不中断，尽可能恢复并继续
// ============================================================

struct ParseError {
    QString message;
    int line = 0;
    int column = 0;
    SqlToken token;
};

class SqlParser {
public:
    // 构造：传入方言类型，内部创建 dialect
    explicit SqlParser(const QString& source, SqlDialectType dialectType);
    // 兼容旧接口：传入外部 dialect 指针（不持有所有权）
    explicit SqlParser(const QString& source, SqlDialect* dialect = nullptr);
    virtual ~SqlParser() = default;

    // 解析入口
    AstPtr parse();              // 解析多条语句 → ProgramNode
    virtual AstPtr parseStatement();     // 解析单条语句
    AstPtr parseExpression();    // 解析单个表达式（用于 WHERE 条件等）

    // 错误信息
    const QList<ParseError>& errors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.isEmpty(); }

protected:
    // ── 语句解析 ──
    virtual AstPtr parseSelectStmt();
    virtual AstPtr parseInsertStmt();
    virtual AstPtr parseUpdateStmt();
    virtual AstPtr parseDeleteStmt();
    virtual AstPtr parseCreateStmt();
    virtual AstPtr parseDropStmt();
    virtual AstPtr parseUseStmt();

    // ── CREATE 子类型 ──
    virtual AstPtr parseCreateTable();
    virtual AstPtr parseCreateDatabase();
    virtual AstPtr parseColumnDef();
    virtual AstPtr parseTypeName();

    // ── LIMIT/OFFSET 子句（方言差异大） ──
    virtual void parseLimitClause(SelectStmt& stmt);

    // ── 表达式解析（优先级爬升） ──
    AstPtr parseExpr();
    AstPtr parseOrExpr();
    AstPtr parseAndExpr();
    AstPtr parseNotExpr();
    AstPtr parseComparisonExpr();
    AstPtr parseAddExpr();
    AstPtr parseMulExpr();
    AstPtr parseUnaryExpr();
    virtual AstPtr parsePrimaryExpr();
    AstPtr parseFunctionCall(const QString& name);

    // ── 子句解析 ──
    AstList parseSelectColumns();
    AstList parseFromClause();
    AstPtr parseJoinClause();
    AstPtr parseWhereClause();
    AstList parseGroupByClause();
    AstPtr parseHavingClause();
    AstList parseOrderByClause();
    AstPtr parseTableRef();

    // ── Token 操作 ──
    SqlToken peek() const;
    SqlToken advance();
    bool check(SqlTokenType type) const;
    bool match(SqlTokenType type);
    SqlToken expect(SqlTokenType type, const QString& message);
    bool isAtEnd() const;

    // ── 错误处理 ──
    void error(const QString& message);
    void synchronize(); // 错误恢复：跳到下一个语句边界

    // ── 工具 ──
    bool isSelectKeyword() const;
    bool isStatementStart() const;
    int currentPrecedence() const;

    QList<SqlToken> m_tokens;
    int m_current = 0;
    std::unique_ptr<SqlDialect> m_ownedDialect; // 内部持有的方言
    SqlDialect* m_dialect;
    QList<ParseError> m_errors;
};

// ── 解析器工厂 ──
std::unique_ptr<SqlParser> createParser(const QString& sql, SqlDialectType type);
