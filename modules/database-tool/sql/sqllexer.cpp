#include "sqllexer.h"

SqlLexer::SqlLexer(const QString& source, SqlDialect* dialect)
    : m_source(source), m_dialect(dialect)
{
    if (m_dialect)
        m_keywords = m_dialect->keywords();
    else
        m_keywords = SqlDialect::ansiKeywords();
}

QChar SqlLexer::peek() const {
    if (isAtEnd()) return QChar();
    return m_source[m_pos];
}

QChar SqlLexer::peekNext() const {
    if (m_pos + 1 >= m_source.size()) return QChar();
    return m_source[m_pos + 1];
}

QChar SqlLexer::advance() {
    QChar c = m_source[m_pos++];
    if (c == '\n') { m_line++; m_column = 1; }
    else m_column++;
    return c;
}

bool SqlLexer::isAtEnd() const {
    return m_pos >= m_source.size();
}

bool SqlLexer::match(QChar expected) {
    if (isAtEnd() || m_source[m_pos] != expected) return false;
    advance();
    return true;
}

SqlToken SqlLexer::makeToken(SqlTokenType type, int start) const {
    QString text = m_source.mid(start, m_pos - start);
    // 计算起始行列（简化：用当前位置回推）
    return SqlToken(type, text, start, m_pos - start, m_line, m_column - (m_pos - start));
}

SqlToken SqlLexer::next() {
    while (!isAtEnd()) {
        int start = m_pos;
        int startLine = m_line;
        int startCol = m_column;
        QChar c = advance();

        // 空白
        if (c.isSpace()) {
            while (!isAtEnd() && m_source[m_pos].isSpace() && m_source[m_pos] != '\n')
                advance();
            if (c == '\n')
                return SqlToken(SqlTokenType::Newline, "\n", start, 1, startLine, startCol);
            return SqlToken(SqlTokenType::Whitespace, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
        }

        // 行注释 --
        if (c == '-' && !isAtEnd() && peek() == '-')
            return scanLineComment();

        // 块注释 /* */
        if (c == '/' && !isAtEnd() && peek() == '*')
            return scanBlockComment();

        // 字符串
        if (c == '\'') return scanString('\'');
        if (c == '"' && (!m_dialect || m_dialect->identifierQuoteChar() != '"'))
            return scanString('"');

        // 带引号的标识符
        if (c == '`') return scanQuotedIdentifier('`');
        if (c == '"' && m_dialect && m_dialect->identifierQuoteChar() == '"')
            return scanQuotedIdentifier('"');
        if (c == '[') return scanQuotedIdentifier(']');

        // 数字
        if (c.isDigit()) { m_pos--; m_column--; return scanNumber(); }

        // 标识符或关键字
        if (c.isLetter() || c == '_') { m_pos--; m_column--; return scanIdentifier(); }

        // 运算符和分隔符
        switch (c.unicode()) {
        case '(': return SqlToken(SqlTokenType::LParen, "(", start, 1, startLine, startCol);
        case ')': return SqlToken(SqlTokenType::RParen, ")", start, 1, startLine, startCol);
        case ',': return SqlToken(SqlTokenType::Comma, ",", start, 1, startLine, startCol);
        case ';': return SqlToken(SqlTokenType::Semicolon, ";", start, 1, startLine, startCol);
        case '.': return SqlToken(SqlTokenType::Dot, ".", start, 1, startLine, startCol);
        case '+': return SqlToken(SqlTokenType::Plus, "+", start, 1, startLine, startCol);
        case '-': return SqlToken(SqlTokenType::Minus, "-", start, 1, startLine, startCol);
        case '*': return SqlToken(SqlTokenType::Star, "*", start, 1, startLine, startCol);
        case '/': return SqlToken(SqlTokenType::Slash, "/", start, 1, startLine, startCol);
        case '%': return SqlToken(SqlTokenType::Percent, "%", start, 1, startLine, startCol);
        case '&': return SqlToken(SqlTokenType::BitAnd, "&", start, 1, startLine, startCol);
        case '~': return SqlToken(SqlTokenType::BitNot, "~", start, 1, startLine, startCol);
        case '^': return SqlToken(SqlTokenType::BitXor, "^", start, 1, startLine, startCol);
        case '@': return SqlToken(SqlTokenType::AtSign, "@", start, 1, startLine, startCol);
        case '#': return SqlToken(SqlTokenType::Hash, "#", start, 1, startLine, startCol);

        case '=': return SqlToken(SqlTokenType::Eq, "=", start, 1, startLine, startCol);

        case '!':
            if (match('=')) return SqlToken(SqlTokenType::Neq, "!=", start, 2, startLine, startCol);
            return SqlToken(SqlTokenType::Error, "!", start, 1, startLine, startCol);

        case '<':
            if (match('=')) return SqlToken(SqlTokenType::LtEq, "<=", start, 2, startLine, startCol);
            if (match('>')) return SqlToken(SqlTokenType::Neq, "<>", start, 2, startLine, startCol);
            if (match('<')) return SqlToken(SqlTokenType::LeftShift, "<<", start, 2, startLine, startCol);
            return SqlToken(SqlTokenType::Lt, "<", start, 1, startLine, startCol);

        case '>':
            if (match('=')) return SqlToken(SqlTokenType::GtEq, ">=", start, 2, startLine, startCol);
            if (match('>')) return SqlToken(SqlTokenType::RightShift, ">>", start, 2, startLine, startCol);
            return SqlToken(SqlTokenType::Gt, ">", start, 1, startLine, startCol);

        case '|':
            if (match('|')) return SqlToken(SqlTokenType::Concat, "||", start, 2, startLine, startCol);
            return SqlToken(SqlTokenType::BitOr, "|", start, 1, startLine, startCol);

        case ':':
            if (match(':')) return SqlToken(SqlTokenType::DoubleColon, "::", start, 2, startLine, startCol);
            return SqlToken(SqlTokenType::Colon, ":", start, 1, startLine, startCol);

        case '?':
            return SqlToken(SqlTokenType::Parameter, "?", start, 1, startLine, startCol);

        default:
            return SqlToken(SqlTokenType::Error, QString(c), start, 1, startLine, startCol);
        }
    }

    return SqlToken(SqlTokenType::Eof, "", m_pos, 0, m_line, m_column);
}

SqlToken SqlLexer::scanString(QChar quote) {
    int start = m_pos - 1; // 包含开头的引号
    int startLine = m_line;
    int startCol = m_column - 1;

    while (!isAtEnd()) {
        QChar c = advance();
        if (c == quote) {
            // 检查转义的引号 '' 或 \'
            if (!isAtEnd() && peek() == quote) {
                advance(); // 跳过转义
                continue;
            }
            return SqlToken(SqlTokenType::String, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
        }
        if (c == '\\' && !isAtEnd()) advance(); // 反斜杠转义
    }

    // 未闭合的字符串
    return SqlToken(SqlTokenType::Error, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
}

SqlToken SqlLexer::scanNumber() {
    int start = m_pos;
    int startLine = m_line;
    int startCol = m_column;
    bool isFloat = false;

    // 0x hex prefix
    if (peek() == '0' && (peekNext() == 'x' || peekNext() == 'X')) {
        advance(); advance(); // skip 0x
        while (!isAtEnd() && (peek().isDigit() || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F')))
            advance();
        return SqlToken(SqlTokenType::HexString, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
    }

    while (!isAtEnd() && peek().isDigit()) advance();

    if (!isAtEnd() && peek() == '.' && peekNext().isDigit()) {
        isFloat = true;
        advance(); // skip .
        while (!isAtEnd() && peek().isDigit()) advance();
    }

    // 科学计数法
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        advance();
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) advance();
        while (!isAtEnd() && peek().isDigit()) advance();
    }

    SqlTokenType type = isFloat ? SqlTokenType::Float : SqlTokenType::Integer;
    return SqlToken(type, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
}

SqlToken SqlLexer::scanIdentifier() {
    int start = m_pos;
    int startLine = m_line;
    int startCol = m_column;

    while (!isAtEnd() && (peek().isLetterOrNumber() || peek() == '_'))
        advance();

    QString text = m_source.mid(start, m_pos - start);
    SqlTokenType type = identifierOrKeyword(text);
    return SqlToken(type, text, start, m_pos - start, startLine, startCol);
}

SqlToken SqlLexer::scanQuotedIdentifier(QChar closeQuote) {
    int start = m_pos - 1;
    int startLine = m_line;
    int startCol = m_column - 1;

    while (!isAtEnd()) {
        QChar c = advance();
        if (c == closeQuote)
            return SqlToken(SqlTokenType::QuotedIdentifier, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
    }

    return SqlToken(SqlTokenType::Error, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
}

SqlToken SqlLexer::scanLineComment() {
    int start = m_pos - 1; // 从第一个 - 开始
    int startLine = m_line;
    int startCol = m_column - 1;
    advance(); // skip second -

    while (!isAtEnd() && peek() != '\n')
        advance();

    return SqlToken(SqlTokenType::LineComment, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
}

SqlToken SqlLexer::scanBlockComment() {
    int start = m_pos - 1; // 从 / 开始
    int startLine = m_line;
    int startCol = m_column - 1;
    advance(); // skip *

    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
            advance(); advance(); // skip */
            return SqlToken(SqlTokenType::BlockComment, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
        }
        advance();
    }

    return SqlToken(SqlTokenType::Error, m_source.mid(start, m_pos - start), start, m_pos - start, startLine, startCol);
}

SqlTokenType SqlLexer::identifierOrKeyword(const QString& text) const {
    QString upper = text.toUpper();
    if (m_keywords.contains(upper))
        return m_keywords.value(upper);
    return SqlTokenType::Identifier;
}

QList<SqlToken> SqlLexer::tokenize() {
    QList<SqlToken> tokens;
    while (true) {
        SqlToken tok = next();
        if (tok.type == SqlTokenType::Whitespace || tok.type == SqlTokenType::Newline)
            continue;
        tokens.append(tok);
        if (tok.type == SqlTokenType::Eof) break;
    }
    return tokens;
}

QList<SqlToken> SqlLexer::tokenizeAll() {
    QList<SqlToken> tokens;
    while (true) {
        SqlToken tok = next();
        tokens.append(tok);
        if (tok.type == SqlTokenType::Eof) break;
    }
    return tokens;
}
