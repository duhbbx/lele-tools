#include "pgparser.h"

// ============================================================
// PgParser — PostgreSQL 方言解析
// ============================================================

// ── INSERT ... ON CONFLICT ... RETURNING ──

AstPtr PgParser::parseInsertStmt() {
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

    // PG: ON CONFLICT (cols) DO UPDATE SET ... / DO NOTHING
    if (match(SqlTokenType::KW_ON)) {
        // Check for "CONFLICT" as identifier
        if (peek().text.toUpper() == "CONFLICT") {
            advance(); // CONFLICT

            // Optional conflict target: (col, col, ...)
            if (match(SqlTokenType::LParen)) {
                while (!isAtEnd() && !check(SqlTokenType::RParen))
                    advance();
                expect(SqlTokenType::RParen, "Expected )");
            }

            // DO UPDATE SET ... / DO NOTHING
            if (peek().text.toUpper() == "DO") {
                advance(); // DO
                if (peek().text.toUpper() == "NOTHING") {
                    advance(); // NOTHING
                    // Store empty ExprListNode to indicate ON CONFLICT DO NOTHING
                    stmt->onConflict = std::make_shared<ExprListNode>();
                } else if (check(SqlTokenType::KW_UPDATE)) {
                    advance(); // UPDATE
                    expect(SqlTokenType::KW_SET, "Expected SET");
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
        }
    }

    // PG: RETURNING col1, col2, ...
    // RETURNING is KW_DIALECT_0 in PG dialect
    if (check(SqlTokenType::KW_DIALECT_0) && peek().text.toUpper() == "RETURNING") {
        advance(); // RETURNING
        do {
            stmt->returning.append(parseExpr());
        } while (match(SqlTokenType::Comma));
    }

    return stmt;
}

// ── CREATE TABLE with PG extensions ──

AstPtr PgParser::parseCreateTable() {
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
            while (!isAtEnd() && !check(SqlTokenType::Comma) && !check(SqlTokenType::RParen))
                advance();
        } else {
            stmt->columns.append(parseColumnDef());
        }
    } while (match(SqlTokenType::Comma));

    expect(SqlTokenType::RParen, "Expected )");

    // PG: INHERITS (parent_table)
    if (peek().text.toUpper() == "INHERITS") {
        advance();
        if (match(SqlTokenType::LParen)) {
            stmt->options["INHERITS"] = advance().text;
            expect(SqlTokenType::RParen, "Expected )");
        }
    }

    // PG: PARTITION BY RANGE/LIST/HASH (col)
    if (peek().text.toUpper() == "PARTITION") {
        advance(); // PARTITION
        expect(SqlTokenType::KW_BY, "Expected BY");
        QString partType = advance().text; // RANGE/LIST/HASH
        stmt->options["PARTITION_BY"] = partType;
        if (match(SqlTokenType::LParen)) {
            stmt->options["PARTITION_COLS"] = advance().text;
            expect(SqlTokenType::RParen, "Expected )");
        }
    }

    // Remaining table options
    while (!isAtEnd() && !check(SqlTokenType::Semicolon)) {
        SqlToken tok = advance();
        if (match(SqlTokenType::Eq)) {
            stmt->options[tok.text.toUpper()] = advance().text;
        }
    }

    return stmt;
}

// ── LIMIT / OFFSET / FETCH FIRST N ROWS ONLY ──

void PgParser::parseLimitClause(SelectStmt& stmt) {
    // Standard LIMIT/OFFSET
    if (match(SqlTokenType::KW_LIMIT)) {
        // LIMIT ALL is valid in PG
        if (check(SqlTokenType::KW_ALL)) {
            advance(); // ALL — no limit
        } else {
            stmt.limit = parseExpr();
        }
    }
    if (match(SqlTokenType::KW_OFFSET))
        stmt.offset = parseExpr();

    // SQL:2008 FETCH FIRST N ROWS ONLY
    if (match(SqlTokenType::KW_FETCH)) {
        // FETCH FIRST|NEXT N ROWS ONLY
        if (match(SqlTokenType::KW_FIRST) || match(SqlTokenType::KW_NEXT)) {
            if (!check(SqlTokenType::KW_ROWS)) // N is optional (defaults to 1)
                stmt.limit = parseExpr();
            match(SqlTokenType::KW_ROWS); // ROWS
            // ONLY keyword
            if (peek().text.toUpper() == "ONLY") advance();
        }
    }
}

// ── PG types: SERIAL, BIGSERIAL, JSONB, UUID, TEXT[], etc. ──

AstPtr PgParser::parseTypeName() {
    auto tn = std::make_shared<TypeNameNode>();
    tn->name = advance().text;

    // Standard precision
    if (match(SqlTokenType::LParen)) {
        tn->precision = advance().text.toInt();
        if (match(SqlTokenType::Comma))
            tn->scale = advance().text.toInt();
        expect(SqlTokenType::RParen, "Expected )");
    }

    // Array notation: TYPE[]
    if (peek().text == "[" || peek().text == "[]") {
        tn->name += "[]";
        advance();
        if (peek().text == "]") advance(); // handle separate ] token
    }

    return tn;
}

// ── :: type cast operator ──

AstPtr PgParser::parsePrimaryExpr() {
    AstPtr expr = SqlParser::parsePrimaryExpr();

    // Check for :: cast operator (DoubleColon)
    while (check(SqlTokenType::DoubleColon)) {
        advance(); // ::
        auto cast = std::make_shared<CastExpr>();
        cast->expr = expr;
        cast->targetType = advance().text; // type name
        // Optional precision: ::NUMERIC(10,2)
        if (match(SqlTokenType::LParen)) {
            cast->precision = advance().text.toInt();
            if (match(SqlTokenType::Comma))
                cast->scale = advance().text.toInt();
            expect(SqlTokenType::RParen, "Expected )");
        }
        expr = cast;
    }

    return expr;
}
