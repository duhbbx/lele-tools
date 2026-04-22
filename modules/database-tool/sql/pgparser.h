#pragma once

#include "sqlparser.h"

// ============================================================
// PostgreSQL Parser — PG 方言特有语法
//
// 覆盖：
// - INSERT ... ON CONFLICT DO UPDATE/NOTHING, RETURNING
// - CREATE TABLE ... INHERITS, PARTITION BY
// - LIMIT N OFFSET M / FETCH FIRST N ROWS ONLY
// - 类型: SERIAL, BIGSERIAL, JSONB, UUID, TEXT[], etc.
// - 表达式: :: 类型转换运算符
// ============================================================

class PgParser : public SqlParser {
public:
    using SqlParser::SqlParser;

protected:
    AstPtr parseInsertStmt() override;
    AstPtr parseCreateTable() override;
    void parseLimitClause(SelectStmt& stmt) override;
    AstPtr parseTypeName() override;
    AstPtr parsePrimaryExpr() override;
};
