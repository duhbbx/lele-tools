#pragma once

#include <QString>
#include <QList>
#include "sqltoken.h"
#include "sqldialect.h"

// ============================================================
// SQL Lexer — 词法分析器（支持多方言）
// ============================================================

class SqlLexer {
public:
    explicit SqlLexer(const QString& source, SqlDialect* dialect = nullptr);

    // 获取下一个 token
    SqlToken next();

    // 获取所有 token（跳过空白和注释）
    QList<SqlToken> tokenize();

    // 获取所有 token（包括空白和注释，用于格式化）
    QList<SqlToken> tokenizeAll();

private:
    QChar peek() const;
    QChar advance();
    bool isAtEnd() const;
    bool match(QChar expected);
    QChar peekNext() const;

    SqlToken makeToken(SqlTokenType type, int start) const;
    SqlToken scanString(QChar quote);
    SqlToken scanNumber();
    SqlToken scanIdentifier();
    SqlToken scanQuotedIdentifier(QChar quote);
    SqlToken scanLineComment();
    SqlToken scanBlockComment();
    SqlToken scanOperator();

    SqlTokenType identifierOrKeyword(const QString& text) const;

    QString m_source;
    int m_pos = 0;
    int m_line = 1;
    int m_column = 1;
    SqlDialect* m_dialect; // 不拥有所有权
    QHash<QString, SqlTokenType> m_keywords;
};
