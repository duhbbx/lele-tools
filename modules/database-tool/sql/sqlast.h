#pragma once

#include <QString>
#include <QList>
#include <QVariant>
#include <memory>
#include "sqltypes.h"
#include "sqltoken.h"

// ============================================================
// SQL AST — 统一的抽象语法树（类似 LLVM IR）
//
// 设计原则：
// 1. 所有方言的 SQL 解析为同一套 AST 节点
// 2. 方言差异通过 metadata/flags 表达，不增加节点类型
// 3. AST 可被 Visitor 遍历，用于格式化/分析/转换
// 4. 每个节点记录源码位置，支持精确错误报告
// ============================================================

class SqlAstVisitor; // 前向声明

// ── 基类 ──────────────────────────────────────────────────

class AstNode {
public:
    AstNodeType nodeType;
    int startOffset = 0;
    int endOffset = 0;

    explicit AstNode(AstNodeType type) : nodeType(type) {}
    virtual ~AstNode() = default;

    // Visitor 模式
    virtual void accept(SqlAstVisitor& visitor) = 0;

    // 便利方法
    template<typename T> T* as() { return dynamic_cast<T*>(this); }
    template<typename T> const T* as() const { return dynamic_cast<const T*>(this); }
    template<typename T> bool isa() const { return dynamic_cast<const T*>(this) != nullptr; }
};

using AstPtr = std::shared_ptr<AstNode>;
using AstList = QList<AstPtr>;

// ── 表达式节点 ────────────────────────────────────────────

// 字面量: 123, 'hello', NULL, TRUE
class LiteralExpr : public AstNode {
public:
    SqlToken token;  // 保留原始 token
    QVariant value;

    LiteralExpr() : AstNode(AstNodeType::Literal) {}
    void accept(SqlAstVisitor& v) override;
};

// 列引用: table.column, schema.table.column
class ColumnRefExpr : public AstNode {
public:
    QString schema;
    QString table;
    QString column;

    ColumnRefExpr() : AstNode(AstNodeType::ColumnRef) {}
    void accept(SqlAstVisitor& v) override;
};

// SELECT *
class StarExpr : public AstNode {
public:
    QString table; // table.* 时非空

    StarExpr() : AstNode(AstNodeType::Star) {}
    void accept(SqlAstVisitor& v) override;
};

// 二元表达式: a + b, a AND b, a = b
class BinaryExpr : public AstNode {
public:
    AstPtr left;
    AstPtr right;
    SqlToken op; // 运算符 token

    BinaryExpr() : AstNode(AstNodeType::BinaryExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// 一元表达式: NOT a, -a
class UnaryExpr : public AstNode {
public:
    AstPtr operand;
    SqlToken op;

    UnaryExpr() : AstNode(AstNodeType::UnaryExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// 函数调用: func(arg1, arg2)
class FunctionCallExpr : public AstNode {
public:
    QString name;
    AstList args;
    bool distinct = false; // COUNT(DISTINCT col)

    FunctionCallExpr() : AstNode(AstNodeType::FunctionCall) {}
    void accept(SqlAstVisitor& v) override;
};

// 子查询: (SELECT ...)
class SubQueryExpr : public AstNode {
public:
    AstPtr query; // SelectStmt

    SubQueryExpr() : AstNode(AstNodeType::SubQuery) {}
    void accept(SqlAstVisitor& v) override;
};

// CASE WHEN ... THEN ... ELSE ... END
class CaseExpr : public AstNode {
public:
    AstPtr operand; // CASE operand WHEN ... (简单 CASE)
    struct WhenClause { AstPtr condition; AstPtr result; };
    QList<WhenClause> whenClauses;
    AstPtr elseResult;

    CaseExpr() : AstNode(AstNodeType::CaseExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// IN 表达式: x IN (1, 2, 3) 或 x IN (SELECT ...)
class InExpr : public AstNode {
public:
    AstPtr expr;
    AstList values;     // 值列表
    AstPtr subQuery;    // 子查询（与 values 互斥）
    bool negated = false; // NOT IN

    InExpr() : AstNode(AstNodeType::InExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// BETWEEN: x BETWEEN a AND b
class BetweenExpr : public AstNode {
public:
    AstPtr expr;
    AstPtr low;
    AstPtr high;
    bool negated = false;

    BetweenExpr() : AstNode(AstNodeType::BetweenExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// LIKE: x LIKE pattern
class LikeExpr : public AstNode {
public:
    AstPtr expr;
    AstPtr pattern;
    bool negated = false;

    LikeExpr() : AstNode(AstNodeType::LikeExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// IS NULL: x IS NULL / x IS NOT NULL
class IsNullExpr : public AstNode {
public:
    AstPtr expr;
    bool negated = false; // IS NOT NULL

    IsNullExpr() : AstNode(AstNodeType::IsNullExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// CAST: CAST(x AS type)
class CastExpr : public AstNode {
public:
    AstPtr expr;
    QString targetType;
    int precision = -1;
    int scale = -1;

    CastExpr() : AstNode(AstNodeType::CastExpr) {}
    void accept(SqlAstVisitor& v) override;
};

// 表达式列表（逗号分隔）
class ExprListNode : public AstNode {
public:
    AstList items;

    ExprListNode() : AstNode(AstNodeType::ExprList) {}
    void accept(SqlAstVisitor& v) override;
};

// ── 表引用 ────────────────────────────────────────────────

// 表引用: schema.table AS alias
class TableRefNode : public AstNode {
public:
    QString schema;
    QString name;
    QString alias;
    AstPtr subQuery; // 子查询作为表源

    TableRefNode() : AstNode(AstNodeType::TableRef) {}
    void accept(SqlAstVisitor& v) override;
};

// JOIN 子句
class JoinClauseNode : public AstNode {
public:
    enum JoinType { Inner, Left, Right, Full, Cross, Natural };
    JoinType joinType = Inner;
    AstPtr table;       // TableRefNode
    AstPtr condition;   // ON condition

    JoinClauseNode() : AstNode(AstNodeType::JoinClause) {}
    void accept(SqlAstVisitor& v) override;
};

// ORDER BY 项
class OrderByItemNode : public AstNode {
public:
    AstPtr expr;
    bool ascending = true;
    bool nullsFirst = false; // NULLS FIRST/LAST

    OrderByItemNode() : AstNode(AstNodeType::OrderByItem) {}
    void accept(SqlAstVisitor& v) override;
};

// ── DML 语句 ──────────────────────────────────────────────

// SELECT 语句
class SelectStmt : public AstNode {
public:
    bool distinct = false;
    AstList columns;        // 选择列（ExprList）
    AstList from;           // FROM 表列表
    AstList joins;          // JOIN 子句列表
    AstPtr where;           // WHERE 条件
    AstList groupBy;        // GROUP BY 表达式
    AstPtr having;          // HAVING 条件
    AstList orderBy;        // ORDER BY 项
    AstPtr limit;           // LIMIT 表达式
    AstPtr offset;          // OFFSET 表达式

    // CTE (WITH)
    struct CTE { QString name; AstList columns; AstPtr query; };
    QList<CTE> ctes;
    bool recursive = false;

    SelectStmt() : AstNode(AstNodeType::SelectStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// INSERT 语句
class InsertStmt : public AstNode {
public:
    AstPtr table;           // 目标表
    AstList columns;        // 列名列表
    AstList valueRows;      // VALUES (...), (...) 每行一个 ExprList
    AstPtr selectQuery;     // INSERT ... SELECT ...
    AstPtr onConflict;      // ON DUPLICATE KEY UPDATE (MySQL) / ON CONFLICT (PG)
    AstList returning;      // RETURNING 列表 (PG)

    InsertStmt() : AstNode(AstNodeType::InsertStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// UPDATE 语句
class UpdateStmt : public AstNode {
public:
    AstPtr table;
    struct Assignment { QString column; AstPtr value; };
    QList<Assignment> assignments;
    AstPtr where;

    UpdateStmt() : AstNode(AstNodeType::UpdateStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// DELETE 语句
class DeleteStmt : public AstNode {
public:
    AstPtr table;
    AstPtr where;

    DeleteStmt() : AstNode(AstNodeType::DeleteStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// ── DDL 语句 ──────────────────────────────────────────────

// 类型名
class TypeNameNode : public AstNode {
public:
    QString name;       // VARCHAR, INT, etc.
    int precision = -1; // VARCHAR(255)
    int scale = -1;     // DECIMAL(10, 2)

    TypeNameNode() : AstNode(AstNodeType::TypeName) {}
    void accept(SqlAstVisitor& v) override;
};

// 列定义
class ColumnDefNode : public AstNode {
public:
    QString name;
    AstPtr type;            // TypeNameNode
    bool notNull = false;
    bool autoIncrement = false;
    AstPtr defaultValue;
    QString comment;
    bool primaryKey = false;
    bool unique = false;

    ColumnDefNode() : AstNode(AstNodeType::ColumnDef) {}
    void accept(SqlAstVisitor& v) override;
};

// CREATE TABLE
class CreateTableStmt : public AstNode {
public:
    QString schema;
    QString name;
    AstList columns;        // ColumnDefNode list
    AstList constraints;    // 表级约束
    bool ifNotExists = false;
    QMap<QString, QString> options; // ENGINE=InnoDB, CHARSET=utf8mb4, etc.

    CreateTableStmt() : AstNode(AstNodeType::CreateTableStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// CREATE DATABASE
class CreateDatabaseStmt : public AstNode {
public:
    QString name;
    bool ifNotExists = false;
    QString charset;
    QString collation;

    CreateDatabaseStmt() : AstNode(AstNodeType::CreateDatabaseStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// DROP 语句
class DropStmt : public AstNode {
public:
    enum ObjectType { Table, Database, Index, View, Schema };
    ObjectType objectType = Table;
    QString schema;
    QString name;
    bool ifExists = false;
    bool cascade = false;

    DropStmt() : AstNode(AstNodeType::DropStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// USE 语句
class UseStmt : public AstNode {
public:
    QString database;

    UseStmt() : AstNode(AstNodeType::UseStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// UNION / INTERSECT / EXCEPT
class SetOperationStmt : public AstNode {
public:
    enum OpType { Union, UnionAll, Intersect, Except };
    OpType opType = Union;
    AstPtr left;
    AstPtr right;

    SetOperationStmt() : AstNode(AstNodeType::UnionStmt) {}
    void accept(SqlAstVisitor& v) override;
};

// 原始 SQL（无法解析的部分）
class RawSqlNode : public AstNode {
public:
    QString sql;

    RawSqlNode() : AstNode(AstNodeType::RawSql) {}
    void accept(SqlAstVisitor& v) override;
};

// 顶层程序：多条语句
class ProgramNode : public AstNode {
public:
    AstList statements;

    ProgramNode() : AstNode(AstNodeType::Program) {}
    void accept(SqlAstVisitor& v) override;
};

// ── Visitor 接口 ─────────────────────────────────────────

class SqlAstVisitor {
public:
    virtual ~SqlAstVisitor() = default;

    virtual void visit(LiteralExpr& node) = 0;
    virtual void visit(ColumnRefExpr& node) = 0;
    virtual void visit(StarExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(FunctionCallExpr& node) = 0;
    virtual void visit(SubQueryExpr& node) = 0;
    virtual void visit(CaseExpr& node) = 0;
    virtual void visit(InExpr& node) = 0;
    virtual void visit(BetweenExpr& node) = 0;
    virtual void visit(LikeExpr& node) = 0;
    virtual void visit(IsNullExpr& node) = 0;
    virtual void visit(CastExpr& node) = 0;
    virtual void visit(ExprListNode& node) = 0;
    virtual void visit(TableRefNode& node) = 0;
    virtual void visit(JoinClauseNode& node) = 0;
    virtual void visit(OrderByItemNode& node) = 0;
    virtual void visit(SelectStmt& node) = 0;
    virtual void visit(InsertStmt& node) = 0;
    virtual void visit(UpdateStmt& node) = 0;
    virtual void visit(DeleteStmt& node) = 0;
    virtual void visit(CreateTableStmt& node) = 0;
    virtual void visit(CreateDatabaseStmt& node) = 0;
    virtual void visit(DropStmt& node) = 0;
    virtual void visit(UseStmt& node) = 0;
    virtual void visit(SetOperationStmt& node) = 0;
    virtual void visit(TypeNameNode& node) = 0;
    virtual void visit(ColumnDefNode& node) = 0;
    virtual void visit(RawSqlNode& node) = 0;
    virtual void visit(ProgramNode& node) = 0;
};
