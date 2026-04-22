#pragma once

#include <QString>
#include "sqlast.h"
#include "sqldialect.h"

// ============================================================
// SQL Formatter — 基于 AST 的 SQL 格式化器
//
// AST → 美化的 SQL 文本
// 支持不同方言的格式化偏好
// ============================================================

class SqlFormatter : public SqlAstVisitor {
public:
    explicit SqlFormatter(SqlDialect* dialect = nullptr);

    // 格式化入口
    QString format(AstNode& node);

    // 快捷方法：解析 + 格式化
    static QString formatSql(const QString& sql, SqlDialectType dialect = SqlDialectType::MySQL);

    // Visitor 实现
    void visit(LiteralExpr& node) override;
    void visit(ColumnRefExpr& node) override;
    void visit(StarExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(FunctionCallExpr& node) override;
    void visit(SubQueryExpr& node) override;
    void visit(CaseExpr& node) override;
    void visit(InExpr& node) override;
    void visit(BetweenExpr& node) override;
    void visit(LikeExpr& node) override;
    void visit(IsNullExpr& node) override;
    void visit(CastExpr& node) override;
    void visit(ExprListNode& node) override;
    void visit(TableRefNode& node) override;
    void visit(JoinClauseNode& node) override;
    void visit(OrderByItemNode& node) override;
    void visit(SelectStmt& node) override;
    void visit(InsertStmt& node) override;
    void visit(UpdateStmt& node) override;
    void visit(DeleteStmt& node) override;
    void visit(CreateTableStmt& node) override;
    void visit(CreateDatabaseStmt& node) override;
    void visit(DropStmt& node) override;
    void visit(UseStmt& node) override;
    void visit(SetOperationStmt& node) override;
    void visit(TypeNameNode& node) override;
    void visit(ColumnDefNode& node) override;
    void visit(RawSqlNode& node) override;
    void visit(ProgramNode& node) override;

private:
    void append(const QString& text);
    void appendKeyword(const QString& keyword);
    void newline();
    void indent();
    void dedent();

    QString m_result;
    int m_indentLevel = 0;
    int m_indentSize = 2;
    bool m_uppercase = true;
    SqlDialect* m_dialect;
    QChar m_identQuote = '`';
};
