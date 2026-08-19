#pragma once

#include <memory>
#include <string>
#include <vector>

namespace virl {

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind {
    Literal,
    Variable,
    Assign,
    IndexAssign,
    Binary,
    Logical,
    Unary,
    Call,
    Index,
    Array
};

enum class LiteralKind { Number, String, Bool, Null };

// Base class for every expression node. Rather than a full visitor
// pattern (more machinery than Phase-1-style code needs), the
// interpreter switches on `kind` and static_casts to the matching
// subclass. This keeps each node a small, readable struct.
struct Expr {
    ExprKind kind;
    int line = 0;
    int column = 0;
    virtual ~Expr() = default;
protected:
    explicit Expr(ExprKind k) : kind(k) {}
};

// A literal number, string, boolean, or null. Numbers are stored as a
// single `double` at runtime -- Virl does not distinguish int vs float
// as separate runtime types (see README "Design decisions").
struct LiteralExpr : Expr {
    LiteralKind literalKind = LiteralKind::Null;
    double numberValue = 0.0;
    std::string stringValue;
    bool boolValue = false;
    LiteralExpr() : Expr(ExprKind::Literal) {}
};

struct VariableExpr : Expr {
    std::string name;
    VariableExpr() : Expr(ExprKind::Variable) {}
};

// name = value
struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
    AssignExpr() : Expr(ExprKind::Assign) {}
};

// target[index] = value
struct IndexAssignExpr : Expr {
    ExprPtr target;
    ExprPtr index;
    ExprPtr value;
    IndexAssignExpr() : Expr(ExprKind::IndexAssign) {}
};

// left OP right, for + - * / % == != > < >= <=
struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
    BinaryExpr() : Expr(ExprKind::Binary) {}
};

// left OP right, for && || (short-circuiting, so kept separate from
// BinaryExpr which always evaluates both sides)
struct LogicalExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
    LogicalExpr() : Expr(ExprKind::Logical) {}
};

// OP operand, for unary ! and unary -
struct UnaryExpr : Expr {
    std::string op;
    ExprPtr operand;
    UnaryExpr() : Expr(ExprKind::Unary) {}
};

// callee(args...)
struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    CallExpr() : Expr(ExprKind::Call) {}
};

// target[index]
struct IndexExpr : Expr {
    ExprPtr target;
    ExprPtr index;
    IndexExpr() : Expr(ExprKind::Index) {}
};

// [elem, elem, ...]
struct ArrayExpr : Expr {
    std::vector<ExprPtr> elements;
    ArrayExpr() : Expr(ExprKind::Array) {}
};

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
struct BlockStmt;

enum class StmtKind {
    Let,
    Const,
    Block,
    If,
    While,
    For,
    Function,
    Return,
    Break,
    Continue,
    Expression
};

struct Stmt {
    StmtKind kind;
    int line = 0;
    int column = 0;
    virtual ~Stmt() = default;
protected:
    explicit Stmt(StmtKind k) : kind(k) {}
};

// let name [: type] = initializer;
struct LetStmt : Stmt {
    std::string name;
    std::string typeAnnotation; // optional; parsed but not enforced yet
    ExprPtr initializer;        // may be null (defaults to `null`)
    LetStmt() : Stmt(StmtKind::Let) {}
};

// const name [: type] = initializer;
struct ConstStmt : Stmt {
    std::string name;
    std::string typeAnnotation;
    ExprPtr initializer; // required by the parser
    ConstStmt() : Stmt(StmtKind::Const) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    BlockStmt() : Stmt(StmtKind::Block) {}
};

// if condition { ... } [else (if ... | { ... })]
struct IfStmt : Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> thenBranch;
    StmtPtr elseBranch; // nullptr, or a BlockStmt, or a nested IfStmt
    IfStmt() : Stmt(StmtKind::If) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;
    WhileStmt() : Stmt(StmtKind::While) {}
};

// for variableName in iterable { ... }
struct ForStmt : Stmt {
    std::string variableName;
    ExprPtr iterable;
    std::unique_ptr<BlockStmt> body;
    ForStmt() : Stmt(StmtKind::For) {}
};

struct Param {
    std::string name;
    std::string typeAnnotation; // optional; parsed but not enforced yet
};

// fn name(params...) [-> type] { ... }
struct FunctionStmt : Stmt {
    std::string name;
    std::vector<Param> params;
    std::string returnType; // optional; parsed but not enforced yet
    // Stored as shared_ptr (rather than the unique_ptr every other node
    // uses) because a runtime FunctionValue needs to keep the body
    // alive for as long as the function might still be called --
    // independent of how long the enclosing FunctionStmt survives.
    std::shared_ptr<BlockStmt> body;
    FunctionStmt() : Stmt(StmtKind::Function) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value; // nullptr if bare `return;`
    ReturnStmt() : Stmt(StmtKind::Return) {}
};

struct BreakStmt : Stmt {
    BreakStmt() : Stmt(StmtKind::Break) {}
};

struct ContinueStmt : Stmt {
    ContinueStmt() : Stmt(StmtKind::Continue) {}
};

struct ExpressionStmt : Stmt {
    ExprPtr expression;
    ExpressionStmt() : Stmt(StmtKind::Expression) {}
};

} // namespace virl
