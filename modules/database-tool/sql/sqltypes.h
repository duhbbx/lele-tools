#pragma once

// ============================================================
// SQL Token Types - 所有 SQL 方言共享的 Token 类型
// ============================================================

enum class SqlTokenType {
    // 字面量
    Integer,          // 123
    Float,            // 3.14
    String,           // 'hello'
    HexString,        // X'FF' / 0xFF
    BitString,        // B'1010'
    Null,             // NULL
    True,             // TRUE
    False,            // FALSE

    // 标识符
    Identifier,       // column_name
    QuotedIdentifier, // `name` / "name" / [name]
    Parameter,        // ? / :name / $1 / @var

    // 运算符
    Plus,             // +
    Minus,            // -
    Star,             // *
    Slash,            // /
    Percent,          // %
    Eq,               // =
    Neq,              // != / <>
    Lt,               // <
    Gt,               // >
    LtEq,             // <=
    GtEq,             // >=
    Concat,           // ||
    BitAnd,           // &
    BitOr,            // |
    BitXor,           // ^
    BitNot,           // ~
    LeftShift,        // <<
    RightShift,       // >>

    // 分隔符
    LParen,           // (
    RParen,           // )
    Comma,            // ,
    Semicolon,        // ;
    Dot,              // .
    Colon,            // :
    DoubleColon,      // :: (PostgreSQL cast)
    Arrow,            // -> (JSON)
    DoubleArrow,      // ->> (JSON)
    AtSign,           // @ (MySQL variable)
    Hash,             // #

    // 关键字（仅列出 ANSI SQL 核心关键字，方言特有的在 Dialect 中扩展）
    // DML
    KW_SELECT, KW_FROM, KW_WHERE, KW_INSERT, KW_INTO, KW_VALUES,
    KW_UPDATE, KW_SET, KW_DELETE, KW_MERGE,

    // Clauses
    KW_AND, KW_OR, KW_NOT, KW_IN, KW_BETWEEN, KW_LIKE, KW_IS,
    KW_AS, KW_ON, KW_USING, KW_CASE, KW_WHEN, KW_THEN, KW_ELSE, KW_END,
    KW_EXISTS, KW_ANY, KW_ALL, KW_SOME, KW_DISTINCT, KW_HAVING,

    // JOIN
    KW_JOIN, KW_INNER, KW_LEFT, KW_RIGHT, KW_FULL, KW_OUTER, KW_CROSS, KW_NATURAL,

    // ORDER / GROUP / LIMIT
    KW_ORDER, KW_BY, KW_GROUP, KW_ASC, KW_DESC, KW_LIMIT, KW_OFFSET,
    KW_FETCH, KW_FIRST, KW_NEXT, KW_ROWS, KW_ONLY,

    // UNION
    KW_UNION, KW_INTERSECT, KW_EXCEPT,

    // DDL
    KW_CREATE, KW_ALTER, KW_DROP, KW_TABLE, KW_INDEX, KW_VIEW,
    KW_DATABASE, KW_SCHEMA, KW_TRIGGER, KW_PROCEDURE, KW_FUNCTION,
    KW_IF, KW_CASCADE, KW_RESTRICT, KW_PRIMARY, KW_KEY, KW_FOREIGN,
    KW_REFERENCES, KW_UNIQUE, KW_CHECK, KW_DEFAULT, KW_CONSTRAINT,
    KW_AUTO_INCREMENT, KW_NOT_NULL,

    // 类型关键字
    KW_INT, KW_INTEGER, KW_BIGINT, KW_SMALLINT, KW_TINYINT,
    KW_FLOAT, KW_DOUBLE, KW_DECIMAL, KW_NUMERIC,
    KW_CHAR, KW_VARCHAR, KW_TEXT, KW_BLOB, KW_BOOLEAN,
    KW_DATE, KW_TIME, KW_DATETIME, KW_TIMESTAMP,

    // 其他
    KW_NULL_KW, // NULL as keyword (distinct from Null literal)
    KW_TRUE_KW, KW_FALSE_KW,
    KW_WITH, KW_RECURSIVE,
    KW_EXPLAIN, KW_ANALYZE, KW_SHOW, KW_DESCRIBE, KW_USE,
    KW_BEGIN, KW_COMMIT, KW_ROLLBACK, KW_SAVEPOINT,
    KW_GRANT, KW_REVOKE,
    KW_COMMENT,

    // 聚合函数（作为关键字识别，简化解析）
    KW_COUNT, KW_SUM, KW_AVG, KW_MIN, KW_MAX,

    // 方言扩展关键字（由 Dialect 定义具体含义）
    KW_DIALECT_0, KW_DIALECT_1, KW_DIALECT_2, KW_DIALECT_3,
    KW_DIALECT_4, KW_DIALECT_5, KW_DIALECT_6, KW_DIALECT_7,
    KW_DIALECT_8, KW_DIALECT_9,

    // 注释
    LineComment,      // -- ...
    BlockComment,     // /* ... */

    // 特殊
    Whitespace,
    Newline,
    Eof,
    Error
};

// ============================================================
// AST Node Types - 统一的 AST 节点类型（类似 LLVM IR 指令类型）
// ============================================================

enum class AstNodeType {
    // 语句级
    SelectStmt,
    InsertStmt,
    UpdateStmt,
    DeleteStmt,
    CreateTableStmt,
    CreateDatabaseStmt,
    CreateIndexStmt,
    CreateViewStmt,
    AlterTableStmt,
    DropStmt,
    UseStmt,
    ExplainStmt,
    ShowStmt,
    BeginStmt,
    CommitStmt,
    RollbackStmt,

    // 子句级
    SelectClause,     // SELECT col1, col2
    FromClause,       // FROM table
    WhereClause,      // WHERE condition
    JoinClause,       // JOIN table ON ...
    GroupByClause,    // GROUP BY ...
    HavingClause,     // HAVING ...
    OrderByClause,    // ORDER BY ...
    LimitClause,      // LIMIT N OFFSET M
    SetClause,        // SET col = val
    ValuesClause,     // VALUES (...)

    // 表达式级
    BinaryExpr,       // a + b, a AND b
    UnaryExpr,        // NOT a, -a
    FunctionCall,     // func(args)
    ColumnRef,        // table.column
    TableRef,         // schema.table AS alias
    SubQuery,         // (SELECT ...)
    CaseExpr,         // CASE WHEN ... END
    CastExpr,         // CAST(x AS type)
    InExpr,           // x IN (...)
    BetweenExpr,      // x BETWEEN a AND b
    LikeExpr,         // x LIKE pattern
    ExistsExpr,       // EXISTS (SELECT ...)
    IsNullExpr,       // x IS NULL

    // 字面量
    Literal,          // 123, 'hello', NULL
    Star,             // *

    // DDL 组件
    ColumnDef,        // name TYPE constraints
    TypeName,         // VARCHAR(255)
    Constraint,       // PRIMARY KEY, UNIQUE, etc.
    IndexDef,         // INDEX idx_name (col)

    // 列表
    ExprList,         // 表达式列表
    IdentList,        // 标识符列表
    AssignList,       // 赋值列表 SET a=1, b=2
    OrderByItem,      // col ASC/DESC

    // 集合操作
    UnionStmt,        // SELECT ... UNION SELECT ...

    // 通用
    Program,          // 顶层：多条语句
    RawSql,           // 无法解析的原始 SQL 片段
};

// SQL 方言类型
enum class SqlDialectType {
    ANSI,       // ANSI SQL 标准
    MySQL,
    PostgreSQL,
    SQLite,
    Oracle,
    SQLServer,
    Redis       // Redis 命令（非 SQL，但统一接口）
};
