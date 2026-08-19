#include "parser/parser.hpp"

#include <sstream>

namespace virl {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {}

// --- cursor helpers -----------------------------------------------------

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token& Parser::advance() {
    if (!isAtEnd()) current_++;
    return previous();
}

bool Parser::checkType(TokenType type) const {
    if (isAtEnd() && type != TokenType::EndOfFile) return false;
    return peek().type == type;
}

bool Parser::checkKeyword(const std::string& word) const {
    return peek().type == TokenType::Keyword && peek().lexeme == word;
}

bool Parser::checkSymbol(const std::string& symbol) const {
    return peek().type == TokenType::Symbol && peek().lexeme == symbol;
}

bool Parser::checkOperator(const std::string& op) const {
    return peek().type == TokenType::Operator && peek().lexeme == op;
}

bool Parser::matchKeyword(const std::string& word) {
    if (!checkKeyword(word)) return false;
    advance();
    return true;
}

bool Parser::matchSymbol(const std::string& symbol) {
    if (!checkSymbol(symbol)) return false;
    advance();
    return true;
}

bool Parser::matchOperator(const std::string& op) {
    if (!checkOperator(op)) return false;
    advance();
    return true;
}

const Token& Parser::expectType(TokenType type, const std::string& what) {
    if (checkType(type)) return advance();
    errorAt(peek(), "Expected " + what + " but found '" + peek().lexeme + "'");
}

void Parser::expectSymbol(const std::string& symbol) {
    if (checkSymbol(symbol)) {
        advance();
        return;
    }
    errorAt(peek(), "Expected '" + symbol + "' but found '" + peek().lexeme + "'");
}

void Parser::error(const std::string& message) const { errorAt(peek(), message); }

void Parser::errorAt(const Token& token, const std::string& message) const {
    std::ostringstream oss;
    oss << message << " at line " << token.line << ", column " << token.column;
    throw ParserError(oss.str());
}

// --- program / declarations ---------------------------------------------

std::vector<StmtPtr> Parser::parseProgram() {
    std::vector<StmtPtr> statements;
    while (!isAtEnd()) {
        statements.push_back(parseDeclaration());
    }
    return statements;
}

StmtPtr Parser::parseDeclaration() {
    if (checkKeyword("let")) return parseLetDecl();
    if (checkKeyword("const")) return parseConstDecl();
    if (checkKeyword("fn")) return parseFunctionDecl();
    return parseStatement();
}

std::string Parser::parseTypeAnnotation() {
    // Called only after confirming a ':' is present.
    expectSymbol(":");
    const Token& typeTok = expectType(TokenType::Identifier, "a type name");
    return typeTok.lexeme;
}

StmtPtr Parser::parseLetDecl() {
    const Token& letTok = advance(); // consume 'let'
    auto stmt = std::make_unique<LetStmt>();
    stmt->line = letTok.line;
    stmt->column = letTok.column;

    const Token& nameTok = expectType(TokenType::Identifier, "a variable name");
    stmt->name = nameTok.lexeme;

    if (checkSymbol(":")) {
        stmt->typeAnnotation = parseTypeAnnotation();
    }

    if (matchOperator("=")) {
        stmt->initializer = parseExpression();
    }

    expectSymbol(";");
    return stmt;
}

StmtPtr Parser::parseConstDecl() {
    const Token& constTok = advance(); // consume 'const'
    auto stmt = std::make_unique<ConstStmt>();
    stmt->line = constTok.line;
    stmt->column = constTok.column;

    const Token& nameTok = expectType(TokenType::Identifier, "a constant name");
    stmt->name = nameTok.lexeme;

    if (checkSymbol(":")) {
        stmt->typeAnnotation = parseTypeAnnotation();
    }

    if (!checkOperator("=")) {
        errorAt(peek(), "Constant '" + stmt->name + "' must be initialized with '='");
    }
    advance(); // consume '='
    stmt->initializer = parseExpression();

    expectSymbol(";");
    return stmt;
}

StmtPtr Parser::parseFunctionDecl() {
    const Token& fnTok = advance(); // consume 'fn'
    auto stmt = std::make_unique<FunctionStmt>();
    stmt->line = fnTok.line;
    stmt->column = fnTok.column;

    const Token& nameTok = expectType(TokenType::Identifier, "a function name");
    stmt->name = nameTok.lexeme;

    expectSymbol("(");
    if (!checkSymbol(")")) {
        do {
            const Token& paramTok = expectType(TokenType::Identifier, "a parameter name");
            Param param;
            param.name = paramTok.lexeme;
            if (checkSymbol(":")) {
                param.typeAnnotation = parseTypeAnnotation();
            }
            stmt->params.push_back(std::move(param));
        } while (matchSymbol(","));
    }
    expectSymbol(")");

    // Lexer produces '-' and '>' as separate one-character operator
    // tokens (Phase 1 has no '->' operator), so '->' is recognized here
    // as two adjacent tokens. Both are checked BEFORE consuming either,
    // so a lone '-' (not followed by '>') is left untouched.
    if (checkOperator("-") && current_ + 1 < tokens_.size() &&
        tokens_[current_ + 1].type == TokenType::Operator && tokens_[current_ + 1].lexeme == ">") {
        advance(); // '-'
        advance(); // '>'
        const Token& retTok = expectType(TokenType::Identifier, "a return type");
        stmt->returnType = retTok.lexeme;
    }

    auto block = parseBlock();
    stmt->body = std::shared_ptr<BlockStmt>(std::move(block));
    return stmt;
}

// --- statements -----------------------------------------------------------

StmtPtr Parser::parseStatement() {
    if (checkSymbol("{")) {
        auto block = parseBlock();
        return block; // unique_ptr<BlockStmt> converts to StmtPtr
    }
    if (checkKeyword("if")) return parseIfStmt();
    if (checkKeyword("while")) return parseWhileStmt();
    if (checkKeyword("for")) return parseForStmt();
    if (checkKeyword("return")) return parseReturnStmt();
    if (checkKeyword("break")) {
        const Token& tok = advance();
        expectSymbol(";");
        auto stmt = std::make_unique<BreakStmt>();
        stmt->line = tok.line;
        stmt->column = tok.column;
        return stmt;
    }
    if (checkKeyword("continue")) {
        const Token& tok = advance();
        expectSymbol(";");
        auto stmt = std::make_unique<ContinueStmt>();
        stmt->line = tok.line;
        stmt->column = tok.column;
        return stmt;
    }
    return parseExpressionStmt();
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    const Token& openTok = peek();
    expectSymbol("{");
    auto block = std::make_unique<BlockStmt>();
    block->line = openTok.line;
    block->column = openTok.column;

    while (!checkSymbol("}") && !isAtEnd()) {
        block->statements.push_back(parseDeclaration());
    }
    expectSymbol("}");
    return block;
}

StmtPtr Parser::parseIfStmt() {
    const Token& ifTok = advance(); // consume 'if'
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = ifTok.line;
    stmt->column = ifTok.column;

    stmt->condition = parseExpression();
    stmt->thenBranch = parseBlock();

    if (matchKeyword("else")) {
        if (checkKeyword("if")) {
            stmt->elseBranch = parseIfStmt();
        } else {
            stmt->elseBranch = parseBlock();
        }
    }
    return stmt;
}

StmtPtr Parser::parseWhileStmt() {
    const Token& whileTok = advance(); // consume 'while'
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = whileTok.line;
    stmt->column = whileTok.column;

    stmt->condition = parseExpression();
    stmt->body = parseBlock();
    return stmt;
}

StmtPtr Parser::parseForStmt() {
    const Token& forTok = advance(); // consume 'for'
    auto stmt = std::make_unique<ForStmt>();
    stmt->line = forTok.line;
    stmt->column = forTok.column;

    const Token& nameTok = expectType(TokenType::Identifier, "a loop variable name");
    stmt->variableName = nameTok.lexeme;

    if (!matchKeyword("in")) {
        errorAt(peek(), "Expected 'in' after for-loop variable");
    }

    stmt->iterable = parseExpression();
    stmt->body = parseBlock();
    return stmt;
}

StmtPtr Parser::parseReturnStmt() {
    const Token& returnTok = advance(); // consume 'return'
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->line = returnTok.line;
    stmt->column = returnTok.column;

    if (!checkSymbol(";")) {
        stmt->value = parseExpression();
    }
    expectSymbol(";");
    return stmt;
}

StmtPtr Parser::parseExpressionStmt() {
    const Token& startTok = peek();
    auto expr = parseExpression();
    expectSymbol(";");
    auto stmt = std::make_unique<ExpressionStmt>();
    stmt->line = startTok.line;
    stmt->column = startTok.column;
    stmt->expression = std::move(expr);
    return stmt;
}

// --- expressions ------------------------------------------------------
// Precedence, lowest to highest:
//   assignment  ( = )
//   logic-or    ( || )
//   logic-and   ( && )
//   equality    ( == != )
//   comparison  ( > >= < <= )
//   term        ( + - )
//   factor      ( * / % )
//   unary       ( ! - )
//   call        ( f(...) , arr[...] )
//   primary     ( literals, identifiers, ( ), [ ] )

ExprPtr Parser::parseExpression() { return parseAssignment(); }

ExprPtr Parser::parseAssignment() {
    ExprPtr expr = parseLogicOr();

    if (checkOperator("=")) {
        const Token& eqTok = advance();
        ExprPtr value = parseAssignment(); // right-associative

        if (expr->kind == ExprKind::Variable) {
            auto* varExpr = static_cast<VariableExpr*>(expr.get());
            auto assign = std::make_unique<AssignExpr>();
            assign->line = eqTok.line;
            assign->column = eqTok.column;
            assign->name = varExpr->name;
            assign->value = std::move(value);
            return assign;
        }
        if (expr->kind == ExprKind::Index) {
            auto* idxExpr = static_cast<IndexExpr*>(expr.get());
            auto assign = std::make_unique<IndexAssignExpr>();
            assign->line = eqTok.line;
            assign->column = eqTok.column;
            assign->target = std::move(idxExpr->target);
            assign->index = std::move(idxExpr->index);
            assign->value = std::move(value);
            return assign;
        }
        errorAt(eqTok, "Invalid assignment target");
    }

    return expr;
}

ExprPtr Parser::parseLogicOr() {
    ExprPtr left = parseLogicAnd();
    while (checkOperator("||")) {
        const Token& opTok = advance();
        ExprPtr right = parseLogicAnd();
        auto expr = std::make_unique<LogicalExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = "||";
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseLogicAnd() {
    ExprPtr left = parseEquality();
    while (checkOperator("&&")) {
        const Token& opTok = advance();
        ExprPtr right = parseEquality();
        auto expr = std::make_unique<LogicalExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = "&&";
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();
    while (checkOperator("==") || checkOperator("!=")) {
        const Token& opTok = advance();
        ExprPtr right = parseComparison();
        auto expr = std::make_unique<BinaryExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = opTok.lexeme;
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    ExprPtr left = parseTerm();
    while (checkOperator(">") || checkOperator(">=") || checkOperator("<") || checkOperator("<=")) {
        const Token& opTok = advance();
        ExprPtr right = parseTerm();
        auto expr = std::make_unique<BinaryExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = opTok.lexeme;
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseTerm() {
    ExprPtr left = parseFactor();
    while (checkOperator("+") || checkOperator("-")) {
        const Token& opTok = advance();
        ExprPtr right = parseFactor();
        auto expr = std::make_unique<BinaryExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = opTok.lexeme;
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseFactor() {
    ExprPtr left = parseUnary();
    while (checkOperator("*") || checkOperator("/") || checkOperator("%")) {
        const Token& opTok = advance();
        ExprPtr right = parseUnary();
        auto expr = std::make_unique<BinaryExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = opTok.lexeme;
        expr->left = std::move(left);
        expr->right = std::move(right);
        left = std::move(expr);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (checkOperator("!") || checkOperator("-")) {
        const Token& opTok = advance();
        ExprPtr operand = parseUnary();
        auto expr = std::make_unique<UnaryExpr>();
        expr->line = opTok.line;
        expr->column = opTok.column;
        expr->op = opTok.lexeme;
        expr->operand = std::move(operand);
        return expr;
    }
    return parseCall();
}

ExprPtr Parser::parseCall() {
    ExprPtr expr = parsePrimary();

    while (true) {
        if (checkSymbol("(")) {
            const Token& openTok = advance();
            auto call = std::make_unique<CallExpr>();
            call->line = openTok.line;
            call->column = openTok.column;
            call->callee = std::move(expr);

            if (!checkSymbol(")")) {
                do {
                    call->args.push_back(parseExpression());
                } while (matchSymbol(","));
            }
            expectSymbol(")");
            expr = std::move(call);
        } else if (checkSymbol("[")) {
            const Token& openTok = advance();
            auto index = std::make_unique<IndexExpr>();
            index->line = openTok.line;
            index->column = openTok.column;
            index->target = std::move(expr);
            index->index = parseExpression();
            expectSymbol("]");
            expr = std::move(index);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    const Token& tok = peek();

    if (checkType(TokenType::Integer) || checkType(TokenType::Float)) {
        advance();
        auto expr = std::make_unique<LiteralExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        expr->literalKind = LiteralKind::Number;
        expr->numberValue = std::stod(tok.lexeme);
        return expr;
    }

    if (checkType(TokenType::String)) {
        advance();
        auto expr = std::make_unique<LiteralExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        expr->literalKind = LiteralKind::String;
        expr->stringValue = tok.lexeme;
        return expr;
    }

    if (checkType(TokenType::Boolean)) {
        advance();
        auto expr = std::make_unique<LiteralExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        expr->literalKind = LiteralKind::Bool;
        expr->boolValue = (tok.lexeme == "true");
        return expr;
    }

    if (checkKeyword("null")) {
        advance();
        auto expr = std::make_unique<LiteralExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        expr->literalKind = LiteralKind::Null;
        return expr;
    }

    if (checkType(TokenType::Identifier)) {
        advance();
        auto expr = std::make_unique<VariableExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        expr->name = tok.lexeme;
        return expr;
    }

    if (matchSymbol("(")) {
        ExprPtr expr = parseExpression();
        expectSymbol(")");
        return expr;
    }

    if (checkSymbol("[")) {
        advance();
        auto expr = std::make_unique<ArrayExpr>();
        expr->line = tok.line;
        expr->column = tok.column;
        if (!checkSymbol("]")) {
            do {
                expr->elements.push_back(parseExpression());
            } while (matchSymbol(","));
        }
        expectSymbol("]");
        return expr;
    }

    errorAt(tok, "Unexpected token '" + tok.lexeme + "'");
}

} // namespace virl
