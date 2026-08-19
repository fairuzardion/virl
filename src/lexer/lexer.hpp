#pragma once

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "lexer/token.hpp"

namespace virl {

// LexerError is thrown for any malformed input the lexer encounters
// (unexpected character, unterminated string, invalid number). The
// message is already formatted to be readable on its own, e.g.:
//   "Unexpected character '$' at line 3, column 10"
class LexerError : public std::runtime_error {
public:
    explicit LexerError(const std::string& message) : std::runtime_error(message) {}
};

// Lexer converts raw Virl source text into a flat list of Tokens.
// It has no knowledge of grammar or meaning -- it only recognizes the
// smallest meaningful chunks of text (identifiers, numbers, strings,
// operators, symbols, keywords) and reports their type and location.
class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenizes the entire source and returns the resulting tokens.
    // The final token is always TokenType::EndOfFile. Throws
    // LexerError on the first malformed input encountered.
    std::vector<Token> tokenize();

private:
    std::string source_;
    size_t pos_;     // index of the next unread character in source_
    int line_;       // current line, 1-based
    int column_;     // current column, 1-based (column of the next
                      // unread character)

    // --- low-level cursor helpers ---
    bool isAtEnd() const;
    char peek(size_t offset = 0) const;   // lookahead without consuming
    char advance();                        // consume and return current char,
                                            // updating line_/column_
    bool match(char expected);             // consume current char if it
                                            // equals `expected`

    // --- scanning helpers ---
    void skipWhitespaceAndComments();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanOperatorOrSymbol();

    // The set of reserved words that are not "true"/"false". Boolean
    // literals are handled separately so they can carry TokenType::Boolean
    // instead of TokenType::Keyword.
    static const std::unordered_set<std::string> keywords_;
};

} // namespace virl
