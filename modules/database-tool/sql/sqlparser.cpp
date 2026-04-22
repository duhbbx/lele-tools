#include "sqlparser.h"
#include "mysqlparser.h"
#include "pgparser.h"
#include "oracleparser.h"
#include "sqlserverparser.h"
#include <QDebug>

// accept 实现（放在 cpp 中避免循环依赖）
void LiteralExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void ColumnRefExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void StarExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void BinaryExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void UnaryExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void FunctionCallExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void SubQueryExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void CaseExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void InExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void BetweenExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void LikeExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void IsNullExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void CastExpr::accept(SqlAstVisitor& v) { v.visit(*this); }
void ExprListNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void TableRefNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void JoinClauseNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void OrderByItemNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void SelectStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void InsertStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void UpdateStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void DeleteStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void CreateTableStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void CreateDatabaseStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void DropStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void UseStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void SetOperationStmt::accept(SqlAstVisitor& v) { v.visit(*this); }
void TypeNameNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void ColumnDefNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void RawSqlNode::accept(SqlAstVisitor& v) { v.visit(*this); }
void ProgramNode::accept(SqlAstVisitor& v) { v.visit(*this); }

// ============================================================
// Parser 实现
// ============================================================

SqlParser::SqlParser(const QString& source, SqlDialectType dialectType)
    : m_ownedDialect(createDialect(dialectType))
    , m_dialect(m_ownedDialect.get())
{
    SqlLexer lexer(source, m_dialect);
    m_tokens = lexer.tokenize();
}

SqlParser::SqlParser(const QString& source, SqlDialect* dialect)
    : m_dialect(dialect)
{
    SqlLexer lexer(source, dialect);
    m_tokens = lexer.tokenize(); // 跳过空白
}

// ── Token 操作 ──

SqlToken SqlParser::peek() const {
    if (m_current >= m_tokens.size())
        return SqlToken(SqlTokenType::Eof, "", 0, 0, 0, 0);
    return m_tokens[m_current];
}

SqlToken SqlParser::advance() {
    SqlToken tok = peek();
    if (m_current < m_tokens.size()) m_current++;
    return tok;
}

bool SqlParser::check(SqlTokenType type) const {
    return peek().type == type;
}

bool SqlParser::match(SqlTokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

SqlToken SqlParser::expect(SqlTokenType type, const QString& message) {
    if (check(type)) return advance();
    error(message);
    return peek();
}

bool SqlParser::isAtEnd() const {
    return peek().isEof();
}

void SqlParser::error(const QString& message) {
    ParseError err;
    err.message = message;
    err.token = peek();
    err.line = err.token.line;
    err.column = err.token.column;
    m_errors.append(err);
}

void SqlParser::synchronize() {
    while (!isAtEnd()) {
        if (peek().is(SqlTokenType::Semicolon)) { advance(); return; }
        if (isStatementStart()) return;
        advance();
    }
}

bool SqlParser::isStatementStart() const {
    switch (peek().type) {
    case SqlTokenType::KW_SELECT: case SqlTokenType::KW_INSERT:
    case SqlTokenType::KW_UPDATE: case SqlTokenType::KW_DELETE:
    case SqlTokenType::KW_CREATE: case SqlTokenType::KW_ALTER:
    case SqlTokenType::KW_DROP: case SqlTokenType::KW_USE:
    case SqlTokenType::KW_WITH: case SqlTokenType::KW_EXPLAIN:
        return true;
    default: return false;
    }
}

// ── 解析入口 ──

AstPtr SqlParser::parse() {
    auto program = std::make_shared<ProgramNode>();
    while (!isAtEnd()) {
        if (match(SqlTokenType::Semicolon)) continue; // 跳过空语句
        AstPtr stmt = parseStatement();
        if (stmt) program->statements.append(stmt);
        match(SqlTokenType::Semicolon); // 可选的分号
    }
    return program;
}

AstPtr SqlParser::parseStatement() {
    switch (peek().type) {
    case SqlTokenType::KW_SELECT: return parseSelectStmt();
    case SqlTokenType::KW_INSERT: return parseInsertStmt();
    case SqlTokenType::KW_UPDATE: return parseUpdateStmt();
    case SqlTokenType::KW_DELETE: return parseDeleteStmt();
    case SqlTokenType::KW_CREATE: return parseCreateStmt();
    case SqlTokenType::KW_DROP:   return parseDropStmt();
    case SqlTokenType::KW_USE:    return parseUseStmt();
    default: {
        // 无法识别的语句，收集为 RawSql
        auto raw = std::make_shared<RawSqlNode>();
        int start = peek().offset;
        while (!isAtEnd() && !peek().is(SqlTokenType::Semicolon))
            advance();
        raw->sql = m_tokens.isEmpty() ? "" :
            QString(); // 简化：实际应从源码截取
        return raw;
    }
    }
}

AstPtr SqlParser::parseExpression() {
    return parseExpr();
}

// ── SELECT ──

AstPtr SqlParser::parseSelectStmt() {
    auto stmt = std::make_shared<SelectStmt>();
    expect(SqlTokenType::KW_SELECT, "Expected SELECT");

    // DISTINCT
    if (match(SqlTokenType::KW_DISTINCT)) stmt->distinct = true;

    // 选择列
    stmt->columns = parseSelectColumns();

    // FROM
    if (match(SqlTokenType::KW_FROM))
        stmt->from = parseFromClause();

    // JOIN
    while (check(SqlTokenType::KW_JOIN) || check(SqlTokenType::KW_INNER) ||
           check(SqlTokenType::KW_LEFT) || check(SqlTokenType::KW_RIGHT) ||
           check(SqlTokenType::KW_FULL) || check(SqlTokenType::KW_CROSS)) {
        stmt->joins.append(parseJoinClause());
    }

    // WHERE
    if (match(SqlTokenType::KW_WHERE))
        stmt->where = parseExpr();

    // GROUP BY
    if (match(SqlTokenType::KW_GROUP)) {
        expect(SqlTokenType::KW_BY, "Expected BY after GROUP");
        stmt->groupBy = parseGroupByClause();
    }

    // HAVING
    if (match(SqlTokenType::KW_HAVING))
        stmt->having = parseExpr();

    // ORDER BY
    if (match(SqlTokenType::KW_ORDER)) {
        expect(SqlTokenType::KW_BY, "Expected BY after ORDER");
        stmt->orderBy = parseOrderByClause();
    }

    // LIMIT / OFFSET（方言可重写）
    parseLimitClause(*stmt);

    // UNION / INTERSECT / EXCEPT
    if (check(SqlTokenType::KW_UNION) || check(SqlTokenType::KW_INTERSECT) || check(SqlTokenType::KW_EXCEPT)) {
        auto setOp = std::make_shared<SetOperationStmt>();
        if (match(SqlTokenType::KW_UNION)) {
            setOp->opType = match(SqlTokenType::KW_ALL) ? SetOperationStmt::UnionAll : SetOperationStmt::Union;
        } else if (match(SqlTokenType::KW_INTERSECT)) {
            setOp->opType = SetOperationStmt::Intersect;
        } else {
            advance();
            setOp->opType = SetOperationStmt::Except;
        }
        setOp->left = stmt;
        setOp->right = parseSelectStmt();
        return setOp;
    }

    return stmt;
}

AstList SqlParser::parseSelectColumns() {
    AstList cols;
    do {
        if (check(SqlTokenType::Star)) {
            advance();
            cols.append(std::make_shared<StarExpr>());
        } else {
            AstPtr expr = parseExpr();
            // AS alias
            if (match(SqlTokenType::KW_AS)) {
                // 别名只是附加到表达式的 metadata，简化处理
                advance(); // 跳过别名标识符
            } else if (check(SqlTokenType::Identifier) && !isStatementStart()) {
                advance(); // 隐式别名
            }
            cols.append(expr);
        }
    } while (match(SqlTokenType::Comma));
    return cols;
}

AstList SqlParser::parseFromClause() {
    AstList tables;
    do {
        tables.append(parseTableRef());
    } while (match(SqlTokenType::Comma));
    return tables;
}

AstPtr SqlParser::parseTableRef() {
    auto ref = std::make_shared<TableRefNode>();

    if (check(SqlTokenType::LParen)) {
        // 子查询
        advance();
        ref->subQuery = parseSelectStmt();
        expect(SqlTokenType::RParen, "Expected )");
    } else {
        // 表名（可能带 schema: schema.table）
        SqlToken name = expect(SqlTokenType::Identifier, "Expected table name");
        ref->name = name.text;
        if (match(SqlTokenType::Dot)) {
            ref->schema = ref->name;
            ref->name = advance().text;
        }
    }

    // AS alias
    if (match(SqlTokenType::KW_AS))
        ref->alias = advance().text;
    else if (check(SqlTokenType::Identifier) && !isStatementStart())
        ref->alias = advance().text;

    return ref;
}

AstPtr SqlParser::parseJoinClause() {
    auto join = std::make_shared<JoinClauseNode>();

    // JOIN 类型
    if (match(SqlTokenType::KW_INNER)) join->joinType = JoinClauseNode::Inner;
    else if (match(SqlTokenType::KW_LEFT)) { match(SqlTokenType::KW_OUTER); join->joinType = JoinClauseNode::Left; }
    else if (match(SqlTokenType::KW_RIGHT)) { match(SqlTokenType::KW_OUTER); join->joinType = JoinClauseNode::Right; }
    else if (match(SqlTokenType::KW_FULL)) { match(SqlTokenType::KW_OUTER); join->joinType = JoinClauseNode::Full; }
    else if (match(SqlTokenType::KW_CROSS)) join->joinType = JoinClauseNode::Cross;

    expect(SqlTokenType::KW_JOIN, "Expected JOIN");
    join->table = parseTableRef();

    if (match(SqlTokenType::KW_ON))
        join->condition = parseExpr();

    return join;
}

AstList SqlParser::parseGroupByClause() {
    AstList exprs;
    do { exprs.append(parseExpr()); } while (match(SqlTokenType::Comma));
    return exprs;
}

AstList SqlParser::parseOrderByClause() {
    AstList items;
    do {
        auto item = std::make_shared<OrderByItemNode>();
        item->expr = parseExpr();
        if (match(SqlTokenType::KW_DESC)) item->ascending = false;
        else match(SqlTokenType::KW_ASC);
        items.append(item);
    } while (match(SqlTokenType::Comma));
    return items;
}

// ── LIMIT/OFFSET（ANSI 基础实现）──

void SqlParser::parseLimitClause(SelectStmt& stmt) {
    if (match(SqlTokenType::KW_LIMIT))
        stmt.limit = parseExpr();
    if (match(SqlTokenType::KW_OFFSET))
        stmt.offset = parseExpr();
}

// ── INSERT ──

AstPtr SqlParser::parseInsertStmt() {
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

    return stmt;
}

// ── UPDATE ──

AstPtr SqlParser::parseUpdateStmt() {
    auto stmt = std::make_shared<UpdateStmt>();
    expect(SqlTokenType::KW_UPDATE, "Expected UPDATE");
    stmt->table = parseTableRef();
    expect(SqlTokenType::KW_SET, "Expected SET");

    do {
        UpdateStmt::Assignment assign;
        assign.column = advance().text;
        expect(SqlTokenType::Eq, "Expected =");
        assign.value = parseExpr();
        stmt->assignments.append(assign);
    } while (match(SqlTokenType::Comma));

    if (match(SqlTokenType::KW_WHERE))
        stmt->where = parseExpr();

    return stmt;
}

// ── DELETE ──

AstPtr SqlParser::parseDeleteStmt() {
    auto stmt = std::make_shared<DeleteStmt>();
    expect(SqlTokenType::KW_DELETE, "Expected DELETE");
    expect(SqlTokenType::KW_FROM, "Expected FROM");
    stmt->table = parseTableRef();

    if (match(SqlTokenType::KW_WHERE))
        stmt->where = parseExpr();

    return stmt;
}

// ── CREATE ──

AstPtr SqlParser::parseCreateStmt() {
    expect(SqlTokenType::KW_CREATE, "Expected CREATE");

    if (check(SqlTokenType::KW_TABLE)) return parseCreateTable();
    if (check(SqlTokenType::KW_DATABASE) || check(SqlTokenType::KW_SCHEMA))
        return parseCreateDatabase();

    // 无法识别的 CREATE
    error("Unsupported CREATE statement");
    synchronize();
    return std::make_shared<RawSqlNode>();
}

AstPtr SqlParser::parseCreateTable() {
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

    // 表选项 (ENGINE=InnoDB 等)
    while (!isAtEnd() && !check(SqlTokenType::Semicolon)) {
        SqlToken tok = advance();
        if (match(SqlTokenType::Eq)) {
            stmt->options[tok.text.toUpper()] = advance().text;
        }
    }

    return stmt;
}

AstPtr SqlParser::parseCreateDatabase() {
    auto stmt = std::make_shared<CreateDatabaseStmt>();
    advance(); // DATABASE or SCHEMA

    if (match(SqlTokenType::KW_IF)) {
        expect(SqlTokenType::KW_NOT, "Expected NOT");
        expect(SqlTokenType::KW_EXISTS, "Expected EXISTS");
        stmt->ifNotExists = true;
    }

    stmt->name = advance().text;

    // 选项
    while (!isAtEnd() && !check(SqlTokenType::Semicolon)) {
        if (peek().text.toUpper() == "CHARACTER" || peek().text.toUpper() == "CHARSET") {
            advance();
            if (peek().text.toUpper() == "SET") advance();
            if (match(SqlTokenType::Eq)) {}
            stmt->charset = advance().text;
        } else if (peek().text.toUpper() == "COLLATE") {
            advance();
            if (match(SqlTokenType::Eq)) {}
            stmt->collation = advance().text;
        } else {
            advance();
        }
    }

    return stmt;
}

AstPtr SqlParser::parseColumnDef() {
    auto col = std::make_shared<ColumnDefNode>();
    col->name = advance().text;
    col->type = parseTypeName();

    // 列约束
    while (!isAtEnd() && !check(SqlTokenType::Comma) && !check(SqlTokenType::RParen)) {
        if (match(SqlTokenType::KW_NOT)) {
            expect(SqlTokenType::KW_NULL_KW, "Expected NULL");
            col->notNull = true;
        } else if (match(SqlTokenType::KW_NULL_KW)) {
            col->notNull = false;
        } else if (match(SqlTokenType::KW_PRIMARY)) {
            expect(SqlTokenType::KW_KEY, "Expected KEY");
            col->primaryKey = true;
        } else if (match(SqlTokenType::KW_UNIQUE)) {
            col->unique = true;
        } else if (match(SqlTokenType::KW_AUTO_INCREMENT)) {
            col->autoIncrement = true;
        } else if (match(SqlTokenType::KW_DEFAULT)) {
            col->defaultValue = parseExpr();
        } else if (match(SqlTokenType::KW_COMMENT)) {
            col->comment = advance().text;
            // 去掉引号
            if (col->comment.startsWith('\'') && col->comment.endsWith('\''))
                col->comment = col->comment.mid(1, col->comment.length() - 2);
        } else {
            advance(); // 跳过不认识的约束
        }
    }

    return col;
}

AstPtr SqlParser::parseTypeName() {
    auto tn = std::make_shared<TypeNameNode>();
    tn->name = advance().text;

    if (match(SqlTokenType::LParen)) {
        tn->precision = advance().text.toInt();
        if (match(SqlTokenType::Comma))
            tn->scale = advance().text.toInt();
        expect(SqlTokenType::RParen, "Expected )");
    }

    return tn;
}

// ── DROP ──

AstPtr SqlParser::parseDropStmt() {
    auto stmt = std::make_shared<DropStmt>();
    expect(SqlTokenType::KW_DROP, "Expected DROP");

    if (match(SqlTokenType::KW_TABLE)) stmt->objectType = DropStmt::Table;
    else if (match(SqlTokenType::KW_DATABASE)) stmt->objectType = DropStmt::Database;
    else if (match(SqlTokenType::KW_SCHEMA)) stmt->objectType = DropStmt::Schema;
    else if (match(SqlTokenType::KW_INDEX)) stmt->objectType = DropStmt::Index;
    else if (match(SqlTokenType::KW_VIEW)) stmt->objectType = DropStmt::View;

    if (match(SqlTokenType::KW_IF)) {
        // IF EXISTS
        advance(); // EXISTS
        stmt->ifExists = true;
    }

    stmt->name = advance().text;
    if (match(SqlTokenType::Dot)) {
        stmt->schema = stmt->name;
        stmt->name = advance().text;
    }

    if (match(SqlTokenType::KW_CASCADE)) stmt->cascade = true;

    return stmt;
}

// ── USE ──

AstPtr SqlParser::parseUseStmt() {
    auto stmt = std::make_shared<UseStmt>();
    expect(SqlTokenType::KW_USE, "Expected USE");
    stmt->database = advance().text;
    return stmt;
}

// ── 表达式解析（优先级爬升） ──

AstPtr SqlParser::parseExpr() { return parseOrExpr(); }

AstPtr SqlParser::parseOrExpr() {
    AstPtr left = parseAndExpr();
    while (match(SqlTokenType::KW_OR)) {
        auto bin = std::make_shared<BinaryExpr>();
        bin->left = left;
        bin->op = m_tokens[m_current - 1];
        bin->right = parseAndExpr();
        left = bin;
    }
    return left;
}

AstPtr SqlParser::parseAndExpr() {
    AstPtr left = parseNotExpr();
    while (match(SqlTokenType::KW_AND)) {
        auto bin = std::make_shared<BinaryExpr>();
        bin->left = left;
        bin->op = m_tokens[m_current - 1];
        bin->right = parseNotExpr();
        left = bin;
    }
    return left;
}

AstPtr SqlParser::parseNotExpr() {
    if (match(SqlTokenType::KW_NOT)) {
        auto un = std::make_shared<UnaryExpr>();
        un->op = m_tokens[m_current - 1];
        un->operand = parseNotExpr();
        return un;
    }
    return parseComparisonExpr();
}

AstPtr SqlParser::parseComparisonExpr() {
    AstPtr left = parseAddExpr();

    // IS NULL / IS NOT NULL
    if (match(SqlTokenType::KW_IS)) {
        auto isn = std::make_shared<IsNullExpr>();
        isn->expr = left;
        if (match(SqlTokenType::KW_NOT)) isn->negated = true;
        expect(SqlTokenType::KW_NULL_KW, "Expected NULL");
        return isn;
    }

    // IN
    if (check(SqlTokenType::KW_IN) || (check(SqlTokenType::KW_NOT) && m_current + 1 < m_tokens.size() && m_tokens[m_current + 1].is(SqlTokenType::KW_IN))) {
        auto inExpr = std::make_shared<InExpr>();
        inExpr->expr = left;
        if (match(SqlTokenType::KW_NOT)) inExpr->negated = true;
        expect(SqlTokenType::KW_IN, "Expected IN");
        expect(SqlTokenType::LParen, "Expected (");
        if (check(SqlTokenType::KW_SELECT)) {
            inExpr->subQuery = parseSelectStmt();
        } else {
            do { inExpr->values.append(parseExpr()); } while (match(SqlTokenType::Comma));
        }
        expect(SqlTokenType::RParen, "Expected )");
        return inExpr;
    }

    // BETWEEN
    if (check(SqlTokenType::KW_BETWEEN) || (check(SqlTokenType::KW_NOT) && m_current + 1 < m_tokens.size() && m_tokens[m_current + 1].is(SqlTokenType::KW_BETWEEN))) {
        auto btw = std::make_shared<BetweenExpr>();
        btw->expr = left;
        if (match(SqlTokenType::KW_NOT)) btw->negated = true;
        expect(SqlTokenType::KW_BETWEEN, "Expected BETWEEN");
        btw->low = parseAddExpr();
        expect(SqlTokenType::KW_AND, "Expected AND");
        btw->high = parseAddExpr();
        return btw;
    }

    // LIKE
    if (check(SqlTokenType::KW_LIKE) || (check(SqlTokenType::KW_NOT) && m_current + 1 < m_tokens.size() && m_tokens[m_current + 1].is(SqlTokenType::KW_LIKE))) {
        auto like = std::make_shared<LikeExpr>();
        like->expr = left;
        if (match(SqlTokenType::KW_NOT)) like->negated = true;
        expect(SqlTokenType::KW_LIKE, "Expected LIKE");
        like->pattern = parseAddExpr();
        return like;
    }

    // 比较运算符
    if (check(SqlTokenType::Eq) || check(SqlTokenType::Neq) ||
        check(SqlTokenType::Lt) || check(SqlTokenType::Gt) ||
        check(SqlTokenType::LtEq) || check(SqlTokenType::GtEq)) {
        auto bin = std::make_shared<BinaryExpr>();
        bin->left = left;
        bin->op = advance();
        bin->right = parseAddExpr();
        return bin;
    }

    return left;
}

AstPtr SqlParser::parseAddExpr() {
    AstPtr left = parseMulExpr();
    while (check(SqlTokenType::Plus) || check(SqlTokenType::Minus) || check(SqlTokenType::Concat)) {
        auto bin = std::make_shared<BinaryExpr>();
        bin->left = left;
        bin->op = advance();
        bin->right = parseMulExpr();
        left = bin;
    }
    return left;
}

AstPtr SqlParser::parseMulExpr() {
    AstPtr left = parseUnaryExpr();
    while (check(SqlTokenType::Star) || check(SqlTokenType::Slash) || check(SqlTokenType::Percent)) {
        auto bin = std::make_shared<BinaryExpr>();
        bin->left = left;
        bin->op = advance();
        bin->right = parseUnaryExpr();
        left = bin;
    }
    return left;
}

AstPtr SqlParser::parseUnaryExpr() {
    if (check(SqlTokenType::Minus) || check(SqlTokenType::Plus) || check(SqlTokenType::BitNot)) {
        auto un = std::make_shared<UnaryExpr>();
        un->op = advance();
        un->operand = parseUnaryExpr();
        return un;
    }
    return parsePrimaryExpr();
}

AstPtr SqlParser::parsePrimaryExpr() {
    // NULL
    if (match(SqlTokenType::KW_NULL_KW)) {
        auto lit = std::make_shared<LiteralExpr>();
        lit->token = m_tokens[m_current - 1];
        lit->value = QVariant();
        return lit;
    }

    // TRUE / FALSE
    if (match(SqlTokenType::KW_TRUE_KW) || match(SqlTokenType::KW_FALSE_KW)) {
        auto lit = std::make_shared<LiteralExpr>();
        lit->token = m_tokens[m_current - 1];
        lit->value = lit->token.type == SqlTokenType::KW_TRUE_KW;
        return lit;
    }

    // 数字
    if (check(SqlTokenType::Integer) || check(SqlTokenType::Float)) {
        auto lit = std::make_shared<LiteralExpr>();
        lit->token = advance();
        lit->value = lit->token.type == SqlTokenType::Integer
            ? QVariant(lit->token.text.toLongLong())
            : QVariant(lit->token.text.toDouble());
        return lit;
    }

    // 字符串
    if (check(SqlTokenType::String)) {
        auto lit = std::make_shared<LiteralExpr>();
        lit->token = advance();
        QString text = lit->token.text;
        if (text.startsWith('\'') && text.endsWith('\''))
            text = text.mid(1, text.length() - 2);
        lit->value = text;
        return lit;
    }

    // 括号 / 子查询
    if (match(SqlTokenType::LParen)) {
        if (check(SqlTokenType::KW_SELECT)) {
            auto sub = std::make_shared<SubQueryExpr>();
            sub->query = parseSelectStmt();
            expect(SqlTokenType::RParen, "Expected )");
            return sub;
        }
        AstPtr expr = parseExpr();
        expect(SqlTokenType::RParen, "Expected )");
        return expr;
    }

    // EXISTS (SELECT ...)
    if (match(SqlTokenType::KW_EXISTS)) {
        expect(SqlTokenType::LParen, "Expected (");
        auto exists = std::make_shared<SubQueryExpr>();
        exists->query = parseSelectStmt();
        expect(SqlTokenType::RParen, "Expected )");
        return exists;
    }

    // CASE
    if (match(SqlTokenType::KW_CASE)) {
        auto caseExpr = std::make_shared<CaseExpr>();
        // 检查是否是简单 CASE（CASE expr WHEN ...）
        if (!check(SqlTokenType::KW_WHEN))
            caseExpr->operand = parseExpr();
        while (match(SqlTokenType::KW_WHEN)) {
            CaseExpr::WhenClause wc;
            wc.condition = parseExpr();
            expect(SqlTokenType::KW_THEN, "Expected THEN");
            wc.result = parseExpr();
            caseExpr->whenClauses.append(wc);
        }
        if (match(SqlTokenType::KW_ELSE))
            caseExpr->elseResult = parseExpr();
        expect(SqlTokenType::KW_END, "Expected END");
        return caseExpr;
    }

    // * (standalone star, e.g. SELECT *)
    if (check(SqlTokenType::Star)) {
        advance();
        return std::make_shared<StarExpr>();
    }

    // 标识符（列引用或函数调用）
    if (check(SqlTokenType::Identifier) || check(SqlTokenType::QuotedIdentifier) ||
        peek().isKeyword()) {
        SqlToken tok = advance();
        QString name = tok.text;
        // 去掉引号
        if (tok.type == SqlTokenType::QuotedIdentifier) {
            if (name.size() >= 2) name = name.mid(1, name.size() - 2);
        }

        // 函数调用: name(...)
        if (check(SqlTokenType::LParen) && !isStatementStart())
            return parseFunctionCall(name);

        // 列引用: schema.table.column 或 table.column
        auto col = std::make_shared<ColumnRefExpr>();
        col->column = name;
        if (match(SqlTokenType::Dot)) {
            col->table = col->column;
            col->column = advance().text;
            if (match(SqlTokenType::Dot)) {
                col->schema = col->table;
                col->table = col->column;
                col->column = advance().text;
            }
        }

        // table.*
        if (check(SqlTokenType::Star) && !col->table.isEmpty()) {
            advance();
            auto star = std::make_shared<StarExpr>();
            star->table = col->table;
            return star;
        }

        return col;
    }

    // 参数
    if (check(SqlTokenType::Parameter)) {
        auto lit = std::make_shared<LiteralExpr>();
        lit->token = advance();
        lit->value = lit->token.text;
        return lit;
    }

    error(QString("Unexpected token: %1").arg(peek().text));
    advance(); // 跳过
    return std::make_shared<RawSqlNode>();
}

AstPtr SqlParser::parseFunctionCall(const QString& name) {
    auto func = std::make_shared<FunctionCallExpr>();
    func->name = name;
    expect(SqlTokenType::LParen, "Expected (");

    if (match(SqlTokenType::KW_DISTINCT)) func->distinct = true;

    if (!check(SqlTokenType::RParen)) {
        if (check(SqlTokenType::Star)) {
            advance();
            func->args.append(std::make_shared<StarExpr>());
        } else {
            do { func->args.append(parseExpr()); } while (match(SqlTokenType::Comma));
        }
    }

    expect(SqlTokenType::RParen, "Expected )");
    return func;
}

AstPtr SqlParser::parseWhereClause() {
    return parseExpr();
}

AstPtr SqlParser::parseHavingClause() {
    return parseExpr();
}

// ── 解析器工厂 ──

std::unique_ptr<SqlParser> createParser(const QString& sql, SqlDialectType type) {
    switch (type) {
    case SqlDialectType::MySQL:
        return std::make_unique<MySqlParser>(sql, type);
    case SqlDialectType::PostgreSQL:
        return std::make_unique<PgParser>(sql, type);
    case SqlDialectType::Oracle:
        return std::make_unique<OracleParser>(sql, type);
    case SqlDialectType::SQLServer:
        return std::make_unique<SqlServerParser>(sql, type);
    default:
        return std::make_unique<SqlParser>(sql, type);
    }
}
