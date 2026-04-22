#pragma once

#include "sqlparser.h"

// ============================================================
// Oracle Parser — Oracle 方言（存根）
//
// TODO: ROWNUM, CONNECT BY, (+) outer join, NVL,
//       FETCH FIRST N ROWS ONLY, sequences, PL/SQL blocks
// ============================================================

class OracleParser : public SqlParser {
public:
    using SqlParser::SqlParser;
};
