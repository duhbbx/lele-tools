#include "sqlformatter.h"
#include "sqlparser.h"

SqlFormatter::SqlFormatter(SqlDialect* dialect)
    : m_dialect(dialect)
{
    if (dialect) {
        m_uppercase = dialect->uppercaseKeywords();
        m_indentSize = dialect->defaultIndent();
        m_identQuote = dialect->identifierQuoteChar();
    }
}

QString SqlFormatter::format(AstNode& node) {
    m_result.clear();
    m_indentLevel = 0;
    node.accept(*this);
    return m_result.trimmed();
}

QString SqlFormatter::formatSql(const QString& sql, SqlDialectType dialectType) {
    auto dialect = createDialect(dialectType);
    SqlParser parser(sql, dialect.get());
    AstPtr ast = parser.parse();
    if (!ast) return sql; // 解析失败返回原文

    SqlFormatter formatter(dialect.get());
    return formatter.format(*ast);
}

void SqlFormatter::append(const QString& text) { m_result += text; }
void SqlFormatter::appendKeyword(const QString& kw) {
    append(m_uppercase ? kw.toUpper() : kw.toLower());
}
void SqlFormatter::newline() {
    m_result += "\n";
    m_result += QString(m_indentLevel * m_indentSize, ' ');
}
void SqlFormatter::indent() { m_indentLevel++; }
void SqlFormatter::dedent() { if (m_indentLevel > 0) m_indentLevel--; }

// ── 表达式 ──

void SqlFormatter::visit(LiteralExpr& node) {
    if (!node.value.isValid()) { appendKeyword("NULL"); return; }
    if (node.value.typeId() == QMetaType::Bool) {
        appendKeyword(node.value.toBool() ? "TRUE" : "FALSE");
    } else if (node.value.typeId() == QMetaType::QString) {
        append("'" + node.value.toString() + "'");
    } else {
        append(node.value.toString());
    }
}

void SqlFormatter::visit(ColumnRefExpr& node) {
    if (!node.schema.isEmpty()) append(node.schema + ".");
    if (!node.table.isEmpty()) append(node.table + ".");
    append(node.column);
}

void SqlFormatter::visit(StarExpr& node) {
    if (!node.table.isEmpty()) append(node.table + ".");
    append("*");
}

void SqlFormatter::visit(BinaryExpr& node) {
    node.left->accept(*this);
    QString op = node.op.text.toUpper();
    if (op == "AND" || op == "OR") {
        newline();
        appendKeyword(op);
        append(" ");
    } else {
        append(" " + node.op.text + " ");
    }
    node.right->accept(*this);
}

void SqlFormatter::visit(UnaryExpr& node) {
    appendKeyword(node.op.text);
    append(" ");
    node.operand->accept(*this);
}

void SqlFormatter::visit(FunctionCallExpr& node) {
    append(node.name.toUpper() + "(");
    if (node.distinct) appendKeyword("DISTINCT ");
    for (int i = 0; i < node.args.size(); i++) {
        if (i > 0) append(", ");
        node.args[i]->accept(*this);
    }
    append(")");
}

void SqlFormatter::visit(SubQueryExpr& node) {
    append("(");
    indent();
    newline();
    if (node.query) node.query->accept(*this);
    dedent();
    newline();
    append(")");
}

void SqlFormatter::visit(CaseExpr& node) {
    appendKeyword("CASE");
    if (node.operand) { append(" "); node.operand->accept(*this); }
    indent();
    for (auto& wc : node.whenClauses) {
        newline();
        appendKeyword("WHEN ");
        wc.condition->accept(*this);
        appendKeyword(" THEN ");
        wc.result->accept(*this);
    }
    if (node.elseResult) {
        newline();
        appendKeyword("ELSE ");
        node.elseResult->accept(*this);
    }
    dedent();
    newline();
    appendKeyword("END");
}

void SqlFormatter::visit(InExpr& node) {
    node.expr->accept(*this);
    if (node.negated) appendKeyword(" NOT");
    appendKeyword(" IN ");
    append("(");
    if (node.subQuery) {
        node.subQuery->accept(*this);
    } else {
        for (int i = 0; i < node.values.size(); i++) {
            if (i > 0) append(", ");
            node.values[i]->accept(*this);
        }
    }
    append(")");
}

void SqlFormatter::visit(BetweenExpr& node) {
    node.expr->accept(*this);
    if (node.negated) appendKeyword(" NOT");
    appendKeyword(" BETWEEN ");
    node.low->accept(*this);
    appendKeyword(" AND ");
    node.high->accept(*this);
}

void SqlFormatter::visit(LikeExpr& node) {
    node.expr->accept(*this);
    if (node.negated) appendKeyword(" NOT");
    appendKeyword(" LIKE ");
    node.pattern->accept(*this);
}

void SqlFormatter::visit(IsNullExpr& node) {
    node.expr->accept(*this);
    appendKeyword(node.negated ? " IS NOT NULL" : " IS NULL");
}

void SqlFormatter::visit(CastExpr& node) {
    appendKeyword("CAST");
    append("(");
    if (node.expr) node.expr->accept(*this);
    appendKeyword(" AS ");
    append(node.targetType);
    if (node.precision >= 0) {
        append("(" + QString::number(node.precision));
        if (node.scale >= 0) append(", " + QString::number(node.scale));
        append(")");
    }
    append(")");
}

void SqlFormatter::visit(ExprListNode& node) {
    for (int i = 0; i < node.items.size(); i++) {
        if (i > 0) append(", ");
        node.items[i]->accept(*this);
    }
}

void SqlFormatter::visit(TableRefNode& node) {
    if (node.subQuery) {
        append("(");
        indent();
        newline();
        node.subQuery->accept(*this);
        dedent();
        newline();
        append(")");
    } else {
        if (!node.schema.isEmpty()) append(node.schema + ".");
        append(node.name);
    }
    if (!node.alias.isEmpty()) {
        appendKeyword(" AS ");
        append(node.alias);
    }
}

void SqlFormatter::visit(JoinClauseNode& node) {
    newline();
    switch (node.joinType) {
    case JoinClauseNode::Inner:  appendKeyword("INNER JOIN "); break;
    case JoinClauseNode::Left:   appendKeyword("LEFT JOIN "); break;
    case JoinClauseNode::Right:  appendKeyword("RIGHT JOIN "); break;
    case JoinClauseNode::Full:   appendKeyword("FULL OUTER JOIN "); break;
    case JoinClauseNode::Cross:  appendKeyword("CROSS JOIN "); break;
    case JoinClauseNode::Natural:appendKeyword("NATURAL JOIN "); break;
    }
    if (node.table) node.table->accept(*this);
    if (node.condition) {
        appendKeyword(" ON ");
        node.condition->accept(*this);
    }
}

void SqlFormatter::visit(OrderByItemNode& node) {
    if (node.expr) node.expr->accept(*this);
    appendKeyword(node.ascending ? " ASC" : " DESC");
}

// ── DML 语句 ──

void SqlFormatter::visit(SelectStmt& node) {
    appendKeyword("SELECT ");
    if (node.distinct) appendKeyword("DISTINCT ");

    for (int i = 0; i < node.columns.size(); i++) {
        if (i > 0) append(", ");
        node.columns[i]->accept(*this);
    }

    if (!node.from.isEmpty()) {
        newline();
        appendKeyword("FROM ");
        for (int i = 0; i < node.from.size(); i++) {
            if (i > 0) append(", ");
            node.from[i]->accept(*this);
        }
    }

    for (auto& j : node.joins)
        if (j) j->accept(*this);

    if (node.where) {
        newline();
        appendKeyword("WHERE ");
        indent();
        node.where->accept(*this);
        dedent();
    }

    if (!node.groupBy.isEmpty()) {
        newline();
        appendKeyword("GROUP BY ");
        for (int i = 0; i < node.groupBy.size(); i++) {
            if (i > 0) append(", ");
            node.groupBy[i]->accept(*this);
        }
    }

    if (node.having) {
        newline();
        appendKeyword("HAVING ");
        node.having->accept(*this);
    }

    if (!node.orderBy.isEmpty()) {
        newline();
        appendKeyword("ORDER BY ");
        for (int i = 0; i < node.orderBy.size(); i++) {
            if (i > 0) append(", ");
            node.orderBy[i]->accept(*this);
        }
    }

    if (node.limit) {
        newline();
        appendKeyword("LIMIT ");
        node.limit->accept(*this);
    }

    if (node.offset) {
        appendKeyword(" OFFSET ");
        node.offset->accept(*this);
    }
}

void SqlFormatter::visit(InsertStmt& node) {
    appendKeyword("INSERT INTO ");
    if (node.table) node.table->accept(*this);

    if (!node.columns.isEmpty()) {
        append(" (");
        for (int i = 0; i < node.columns.size(); i++) {
            if (i > 0) append(", ");
            node.columns[i]->accept(*this);
        }
        append(")");
    }

    if (!node.valueRows.isEmpty()) {
        newline();
        appendKeyword("VALUES");
        for (int i = 0; i < node.valueRows.size(); i++) {
            if (i > 0) append(",");
            newline();
            indent();
            append("(");
            node.valueRows[i]->accept(*this);
            append(")");
            dedent();
        }
    } else if (node.selectQuery) {
        newline();
        node.selectQuery->accept(*this);
    }
}

void SqlFormatter::visit(UpdateStmt& node) {
    appendKeyword("UPDATE ");
    if (node.table) node.table->accept(*this);
    newline();
    appendKeyword("SET ");
    indent();
    for (int i = 0; i < node.assignments.size(); i++) {
        if (i > 0) { append(","); newline(); }
        append(node.assignments[i].column + " = ");
        if (node.assignments[i].value) node.assignments[i].value->accept(*this);
    }
    dedent();
    if (node.where) {
        newline();
        appendKeyword("WHERE ");
        node.where->accept(*this);
    }
}

void SqlFormatter::visit(DeleteStmt& node) {
    appendKeyword("DELETE FROM ");
    if (node.table) node.table->accept(*this);
    if (node.where) {
        newline();
        appendKeyword("WHERE ");
        node.where->accept(*this);
    }
}

// ── DDL ──

void SqlFormatter::visit(CreateTableStmt& node) {
    appendKeyword("CREATE TABLE ");
    if (node.ifNotExists) appendKeyword("IF NOT EXISTS ");
    if (!node.schema.isEmpty()) append(node.schema + ".");
    append(node.name);
    append(" (");
    indent();
    for (int i = 0; i < node.columns.size(); i++) {
        if (i > 0) append(",");
        newline();
        node.columns[i]->accept(*this);
    }
    dedent();
    newline();
    append(")");

    for (auto it = node.options.begin(); it != node.options.end(); ++it) {
        append(" " + it.key() + "=" + it.value());
    }
}

void SqlFormatter::visit(CreateDatabaseStmt& node) {
    appendKeyword("CREATE DATABASE ");
    if (node.ifNotExists) appendKeyword("IF NOT EXISTS ");
    append(node.name);
    if (!node.charset.isEmpty()) {
        newline();
        indent();
        appendKeyword("CHARACTER SET ");
        append(node.charset);
        dedent();
    }
    if (!node.collation.isEmpty()) {
        newline();
        indent();
        appendKeyword("COLLATE ");
        append(node.collation);
        dedent();
    }
}

void SqlFormatter::visit(DropStmt& node) {
    appendKeyword("DROP ");
    switch (node.objectType) {
    case DropStmt::Table:    appendKeyword("TABLE "); break;
    case DropStmt::Database: appendKeyword("DATABASE "); break;
    case DropStmt::Index:    appendKeyword("INDEX "); break;
    case DropStmt::View:     appendKeyword("VIEW "); break;
    case DropStmt::Schema:   appendKeyword("SCHEMA "); break;
    }
    if (node.ifExists) appendKeyword("IF EXISTS ");
    if (!node.schema.isEmpty()) append(node.schema + ".");
    append(node.name);
    if (node.cascade) appendKeyword(" CASCADE");
}

void SqlFormatter::visit(UseStmt& node) {
    appendKeyword("USE ");
    append(node.database);
}

void SqlFormatter::visit(SetOperationStmt& node) {
    if (node.left) node.left->accept(*this);
    newline();
    switch (node.opType) {
    case SetOperationStmt::Union:     appendKeyword("UNION"); break;
    case SetOperationStmt::UnionAll:  appendKeyword("UNION ALL"); break;
    case SetOperationStmt::Intersect: appendKeyword("INTERSECT"); break;
    case SetOperationStmt::Except:    appendKeyword("EXCEPT"); break;
    }
    newline();
    if (node.right) node.right->accept(*this);
}

void SqlFormatter::visit(TypeNameNode& node) {
    append(node.name.toUpper());
    if (node.precision >= 0) {
        append("(" + QString::number(node.precision));
        if (node.scale >= 0) append(", " + QString::number(node.scale));
        append(")");
    }
}

void SqlFormatter::visit(ColumnDefNode& node) {
    append(node.name + " ");
    if (node.type) node.type->accept(*this);
    if (node.notNull) appendKeyword(" NOT NULL");
    if (node.primaryKey) appendKeyword(" PRIMARY KEY");
    if (node.unique) appendKeyword(" UNIQUE");
    if (node.autoIncrement) appendKeyword(" AUTO_INCREMENT");
    if (node.defaultValue) {
        appendKeyword(" DEFAULT ");
        node.defaultValue->accept(*this);
    }
    if (!node.comment.isEmpty()) {
        appendKeyword(" COMMENT ");
        append("'" + node.comment + "'");
    }
}

void SqlFormatter::visit(RawSqlNode& node) {
    append(node.sql);
}

void SqlFormatter::visit(ProgramNode& node) {
    for (int i = 0; i < node.statements.size(); i++) {
        if (i > 0) { append(";"); newline(); newline(); }
        node.statements[i]->accept(*this);
    }
    if (!node.statements.isEmpty()) append(";");
}
