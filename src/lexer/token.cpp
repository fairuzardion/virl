#include "lexer/token.hpp"

namespace virl {

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "Identifier";
        case TokenType::Keyword:    return "Keyword";
        case TokenType::Integer:    return "Integer";
        case TokenType::Float:      return "Float";
        case TokenType::String:     return "String";
        case TokenType::Boolean:    return "Boolean";
        case TokenType::Operator:   return "Operator";
        case TokenType::Symbol:     return "Symbol";
        case TokenType::EndOfFile:  return "EndOfFile";
    }
    // Unreachable as long as every enumerator above is handled, but
    // kept for compilers that warn about missing returns.
    return "Unknown";
}

} // namespace virl
