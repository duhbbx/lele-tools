#pragma once

#include "sqlparser.h"

// ============================================================
// MySQL Parser — MySQL 方言特有语法
//
// 覆盖：
// - INSERT ... ON DUPLICATE KEY UPDATE
// - CREATE TABLE ... ENGINE/CHARSET/AUTO_INCREMENT 表选项
// - LIMIT N OFFSET M / LIMIT M, N
// - 类型: ENUM, SET, UNSIGNED, ZEROFILL
// - 语句: SHOW, DESCRIBE, USE
// ============================================================

class MySqlParser : public SqlParser {
public:
    using SqlParser::SqlParser;

protected:
    AstPtr parseStatement() override;
    AstPtr parseInsertStmt() override;
    AstPtr parseCreateTable() override;
    void parseLimitClause(SelectStmt& stmt) override;
    AstPtr parseTypeName() override;

private:
    // MySQL-specific statements
    AstPtr parseShowStmt();
    AstPtr parseDescribeStmt();
};
