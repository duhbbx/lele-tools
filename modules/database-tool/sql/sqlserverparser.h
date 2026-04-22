#pragma once

#include "sqlparser.h"

// ============================================================
// SQL Server Parser — MSSQL 方言（存根）
//
// TODO: TOP, NOLOCK, WITH(NOLOCK), IDENTITY,
//       square bracket identifiers, OFFSET FETCH, OUTPUT clause
// ============================================================

class SqlServerParser : public SqlParser {
public:
    using SqlParser::SqlParser;
};
