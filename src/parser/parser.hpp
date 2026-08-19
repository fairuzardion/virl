#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "parser/ast.hpp"
#include "lexer/token.hpp"

namespace virl {

// ParserError is thrown for any structurally invalid token sequence
// (missing semicolon, unexpected token, unterminated block, etc). Like
// LexerError, the message is already formatted for direct display.
class ParserError : public std::runtime_error {
public:
    explicit ParserError(const std::string& message) : std::runtime_error(message) {}
};

// Parser turns a flat token stream (as produced by Lexer::tokenize())
// into a program: a list of top-level statements. It uses plain
// recursive descent with a standard precedence-climbing expression
// parser -- no parser generator, no lookahead beyond one token.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parses the whole token stream into a program. Throws ParserError
    // on the first structural problem found.
    std::vector<StmtPtr> parseProgram();

private:
    std::vector<Token> tokens_;
    size_t current_;

    // --- cursor helpers ---
    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    const Token& advance();

    bool checkType(TokenType type) const;
    bool checkKeyword(const std::string& word) const;
    bool checkSymbol(const std::string& symbol) const;
    bool checkOperator(const std::string& op) const;

    bool matchKeyword(const std::string& word);
    bool matchSymbol(const std::string& symbol);
    bool matchOperator(const std::string& op);

    const Token& expectType(TokenType type, const std::string& what);
    void expectSymbol(const std::string& symbol);

    [[noreturn]] void error(const std::string& message) const;
    [[noreturn]] void errorAt(const Token& token, const std::string& message) const;

    // --- declarations & statements ---
    StmtPtr parseDeclaration();
    StmtPtr parseLetDecl();
    StmtPtr parseConstDecl();
    StmtPtr parseFunctionDecl();
    StmtPtr parseStatement();
    std::unique_ptr<BlockStmt> parseBlock();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseExpressionStmt();
    std::string parseTypeAnnotation(); // consumes ": TYPE", returns TYPE

    // --- expressions, lowest to highest precedence ---
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicOr();
    ExprPtr parseLogicAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
};

} // namespace virl
