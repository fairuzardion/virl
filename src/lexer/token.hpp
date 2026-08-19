#pragma once

#include <string>

namespace virl {

// TokenType enumerates every category of lexical token the Virl lexer
// can produce. This list intentionally stays small in Phase 1 -- new
// kinds can be added later (e.g. Null, if it ever needs to be
// distinguished from other keywords) without breaking existing code,
// since every consumer switches on this enum.
enum class TokenType {
    Identifier,   // names: x, my_variable, _private
    Keyword,      // let, const, fn, return, if, else, while, for, in,
                  // null, break, continue
    Integer,      // 123, 42
    Float,        // 3.14, 0.5
    String,       // "Hello world"
    Boolean,      // true, false
    Operator,     // + - * / % = == != > < >= <= && || !
    Symbol,       // { } ( ) [ ] ; , . :
    EndOfFile     // marks the end of the token stream
};

// A single lexical token, with enough source-location information for
// the future parser/compiler to produce precise error messages.
struct Token {
    TokenType type;
    std::string lexeme;  // the raw text of the token (or decoded value
                          // for strings)
    int line;             // 1-based line number
    int column;           // 1-based column number of the token's first
                          // character
};

// Returns a short, human-readable name for a TokenType, e.g.
// "Identifier", "Keyword". Used by the CLI printer and by error
// messages.
std::string tokenTypeToString(TokenType type);

} // namespace virl
