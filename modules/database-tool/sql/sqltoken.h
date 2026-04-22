#pragma once

#include <QString>
#include "sqltypes.h"

// ============================================================
// SQL Token — 词法分析产物
// ============================================================

struct SqlToken {
    SqlTokenType type = SqlTokenType::Eof;
    QString text;           // 原始文本
    int offset = 0;         // 在源文本中的偏移
    int length = 0;         // 长度
    int line = 1;           // 行号
    int column = 1;         // 列号

    SqlToken() = default;
    SqlToken(SqlTokenType t, const QString& txt, int off, int len, int ln, int col)
        : type(t), text(txt), offset(off), length(len), line(ln), column(col) {}

    bool is(SqlTokenType t) const { return type == t; }
    bool isKeyword() const { return type >= SqlTokenType::KW_SELECT && type <= SqlTokenType::KW_MAX; }
    bool isLiteral() const { return type >= SqlTokenType::Integer && type <= SqlTokenType::False; }
    bool isOperator() const { return type >= SqlTokenType::Plus && type <= SqlTokenType::RightShift; }
    bool isEof() const { return type == SqlTokenType::Eof; }
    bool isError() const { return type == SqlTokenType::Error; }
};
