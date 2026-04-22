#include "mysqlparser.h"

// ============================================================
// MySqlParser — MySQL 方言解析
// ============================================================

// ── 语句分发（增加 SHOW / DESCRIBE）──

AstPtr MySqlParser::parseStatement() {
    switch (peek().type) {
    case SqlTokenType::KW_SHOW:     return parseShowStmt();
    case SqlTokenType::KW_DESCRIBE: return parseDescribeStmt();
    default: return SqlParser::parseStatement();
    }
}

// ── SHOW（简化：收集为 RawSql）──

AstPtr MySqlParser::parseShowStmt() {
    auto raw = std::make_shared<RawSqlNode>();
    int start = peek().offset;
    while (!isAtEnd() && !check(SqlTokenType::Semicolon))
        advance();
    // 从 token 范围重建 SQL 文本
    if (!m_tokens.isEmpty() && m_current > 0) {
        int end = m_tokens[m_current - 1].offset + m_tokens[m_current - 1].length;
        raw->sql = QString();
        for (int i = 0; i < m_tokens.size(); i++) {
            if (m_tokens[i].offset >= start && m_tokens[i].offset < end) {
                if (!raw->sql.isEmpty()) raw->sql += " ";
                raw->sql += m_tokens[i].text;
            }
        }
    }
    return raw;
}

// ── DESCRIBE（简化：收集为 RawSql）──

AstPtr MySqlParser::parseDescribeStmt() {
    auto raw = std::make_shared<RawSqlNode>();
    int start = peek().offset;
    while (!isAtEnd() && !check(SqlTokenType::Semicolon))
        advance();
    if (!m_tokens.isEmpty() && m_current > 0) {
        int end = m_tokens[m_current - 1].offset + m_tokens[m_current - 1].length;
        raw->sql = QString();
        for (int i = 0; i < m_tokens.size(); i++) {
            if (m_tokens[i].offset >= start && m_tokens[i].offset < end) {
                if (!raw->sql.isEmpty()) raw->sql += " ";
                raw->sql += m_tokens[i].text;
            }
        }
    }
    return raw;
}

// ── INSERT ... ON DUPLICATE KEY UPDATE ──

AstPtr MySqlParser::parseInsertStmt() {
    auto stmt = std::make_shared<InsertStmt>();
    expect(SqlTokenType::KW_INSERT, "Expected INSERT");
    expect(SqlTokenType::KW_INTO, "Expected INTO");

    stmt->table = parseTableRef();

    // 可选列名列表
    if (match(SqlTokenType::LParen)) {
        do {
            auto col = std::make_shared<ColumnRefExpr>();
            col->column = advance().text;
            stmt->columns.append(col);
        } while (match(SqlTokenType::Comma));
        expect(SqlTokenType::RParen, "Expected )");
    }

    if (match(SqlTokenType::KW_VALUES)) {
        do {
            expect(SqlTokenType::LParen, "Expected (");
            auto row = std::make_shared<ExprListNode>();
            do { row->items.append(parseExpr()); } while (match(SqlTokenType::Comma));
            expect(SqlTokenType::RParen, "Expected )");
            stmt->valueRows.append(row);
        } while (match(SqlTokenType::Comma));
    } else if (check(SqlTokenType::KW_SELECT)) {
        stmt->selectQuery = parseSelectStmt();
    }

    // MySQL: ON DUPLICATE KEY UPDATE col = expr, ...
    // KW_ON is used, then check for DUPLICATE (KW_DIALECT_6 in MySQL dialect)
    if (match(SqlTokenType::KW_ON)) {
        if (check(SqlTokenType::KW_DIALECT_6)) { // DUPLICATE
            advance(); // DUPLICATE
            expect(SqlTokenType::KW_KEY, "Expected KEY");
            expect(SqlTokenType::KW_UPDATE, "Expected UPDATE");
            // Parse assignments and store as onConflict metadata
            // We store as a list of ExprListNode items: [col, val, col, val, ...]
            auto conflictUpdate = std::make_shared<ExprListNode>();
            do {
                auto colRef = std::make_shared<ColumnRefExpr>();
                colRef->column = advance().text;
                conflictUpdate->items.append(colRef);
                expect(SqlTokenType::Eq, "Expected =");
                conflictUpdate->items.append(parseExpr());
            } while (match(SqlTokenType::Comma));
            stmt->onConflict = conflictUpdate;
        }
    }

    return stmt;
}

// ── CREATE TABLE with MySQL table options ──

AstPtr MySqlParser::parseCreateTable() {
    auto stmt = std::make_shared<CreateTableStmt>();
    expect(SqlTokenType::KW_TABLE, "Expected TABLE");

    if (match(SqlTokenType::KW_IF)) {
        expect(SqlTokenType::KW_NOT, "Expected NOT");
        expect(SqlTokenType::KW_EXISTS, "Expected EXISTS");
        stmt->ifNotExists = true;
    }

    // 表名
    SqlToken name = advance();
    stmt->name = name.text;
    if (match(SqlTokenType::Dot)) {
        stmt->schema = stmt->name;
        stmt->name = advance().text;
    }

    expect(SqlTokenType::LParen, "Expected (");

    // 列定义
    do {
        if (check(SqlTokenType::KW_PRIMARY) || check(SqlTokenType::KW_UNIQUE) ||
            check(SqlTokenType::KW_CHECK) || check(SqlTokenType::KW_FOREIGN) ||
            check(SqlTokenType::KW_CONSTRAINT)) {
            // 表级约束 — 简化：跳过
            while (!isAtEnd() && !check(SqlTokenType::Comma) && !check(SqlTokenType::RParen))
                advance();
        } else {
            stmt->columns.append(parseColumnDef());
        }
    } while (match(SqlTokenType::Comma));

    expect(SqlTokenType::RParen, "Expected )");

    // MySQL table options: ENGINE=X, DEFAULT CHARSET=X, COLLATE=X,
    // AUTO_INCREMENT=N, COMMENT='X'
    while (!isAtEnd() && !check(SqlTokenType::Semicolon)) {
        SqlToken tok = peek();
        QString upper = tok.text.toUpper();

        if (upper == "DEFAULT") {
            advance();
            // DEFAULT CHARSET / DEFAULT CHARACTER SET
            continue;
        }

        if (upper == "ENGINE" || upper == "CHARSET" || upper == "COLLATE" ||
            upper == "AUTO_INCREMENT" || upper == "CHARACTER" || upper == "COMMENT" ||
            upper == "ROW_FORMAT") {
            advance();
            if (upper == "CHARACTER") {
                // CHARACTER SET
                if (peek().text.toUpper() == "SET") {
                    advance();
                    upper = "CHARSET";
                }
            }
            match(SqlTokenType::Eq); // optional =
            stmt->options[upper] = advance().text;
        } else {
            // Unknown option, try key=value pattern
            advance();
            if (match(SqlTokenType::Eq)) {
                stmt->options[upper] = advance().text;
            }
        }
    }

    return stmt;
}

// ── LIMIT N OFFSET M / LIMIT M, N (MySQL shorthand) ──

void MySqlParser::parseLimitClause(SelectStmt& stmt) {
    if (match(SqlTokenType::KW_LIMIT)) {
        stmt.limit = parseExpr();
        // MySQL shorthand: LIMIT offset, count
        if (match(SqlTokenType::Comma)) {
            stmt.offset = stmt.limit;   // first was offset
            stmt.limit = parseExpr();   // second is count
        }
    }
    if (match(SqlTokenType::KW_OFFSET))
        stmt.offset = parseExpr();
}

// ── MySQL types: UNSIGNED, ZEROFILL, ENUM(...), SET(...) ──

AstPtr MySqlParser::parseTypeName() {
    auto tn = std::make_shared<TypeNameNode>();
    tn->name = advance().text;

    // ENUM('a', 'b') or SET('a', 'b')
    QString upper = tn->name.toUpper();
    if (upper == "ENUM" || upper == "SET") {
        if (match(SqlTokenType::LParen)) {
            // Skip the value list — store count as precision
            int count = 0;
            do {
                advance(); // value
                count++;
            } while (match(SqlTokenType::Comma));
            tn->precision = count;
            expect(SqlTokenType::RParen, "Expected )");
        }
        return tn;
    }

    // Standard precision: TYPE(N) or TYPE(N, M)
    if (match(SqlTokenType::LParen)) {
        tn->precision = advance().text.toInt();
        if (match(SqlTokenType::Comma))
            tn->scale = advance().text.toInt();
        expect(SqlTokenType::RParen, "Expected )");
    }

    // UNSIGNED / ZEROFILL modifiers
    if (peek().text.toUpper() == "UNSIGNED") {
        tn->name += " UNSIGNED";
        advance();
    }
    if (peek().text.toUpper() == "ZEROFILL") {
        tn->name += " ZEROFILL";
        advance();
    }

    return tn;
}
