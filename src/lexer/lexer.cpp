#include "lexer/lexer.hpp"

#include <cctype>
#include <sstream>

namespace virl {

const std::unordered_set<std::string> Lexer::keywords_ = {
    "let", "const", "fn", "return",
    "if", "else", "while", "for", "in",
    "null", "break", "continue"
    // Note: "true" and "false" are intentionally NOT in this set.
    // They are recognized as TokenType::Boolean literals rather than
    // generic keywords -- see scanIdentifierOrKeyword().
};

Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1) {}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

char Lexer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Single-line comment: consume until newline or EOF.
            while (!isAtEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();

        if (isAtEnd()) {
            tokens.push_back(Token{TokenType::EndOfFile, "", line_, column_});
            break;
        }

        char c = peek();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(scanIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(scanNumber());
        } else if (c == '"') {
            tokens.push_back(scanString());
        } else {
            tokens.push_back(scanOperatorOrSymbol());
        }
    }

    return tokens;
}

Token Lexer::scanIdentifierOrKeyword() {
    int startLine = line_;
    int startColumn = column_;
    size_t start = pos_;

    while (!isAtEnd() &&
           (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        advance();
    }

    std::string text = source_.substr(start, pos_ - start);

    if (text == "true" || text == "false") {
        return Token{TokenType::Boolean, text, startLine, startColumn};
    }
    if (keywords_.count(text) > 0) {
        return Token{TokenType::Keyword, text, startLine, startColumn};
    }
    return Token{TokenType::Identifier, text, startLine, startColumn};
}

Token Lexer::scanNumber() {
    int startLine = line_;
    int startColumn = column_;
    size_t start = pos_;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    bool isFloat = false;
    if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isFloat = true;
        advance(); // consume '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    } else if (!isAtEnd() && peek() == '.') {
        // A '.' immediately follows digits but is NOT followed by another
        // digit (e.g. "10." or a field access like "10.toString"). Phase 1
        // does not support trailing-dot floats, so we stop the number here
        // and let '.' be tokenized separately as a Symbol. This keeps
        // "10.field"-style syntax available to a future parser.
    }

    // Guard against a malformed literal like "3.14.15" -- if another
    // digit-dot sequence immediately follows, treat it as an invalid number
    // rather than silently emitting two separate numeric tokens.
    if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        // consume the rest of the malformed literal for a clearer error
        while (!isAtEnd() &&
               (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')) {
            advance();
        }
        std::ostringstream oss;
        oss << "Invalid number at line " << startLine << ", column " << startColumn;
        throw LexerError(oss.str());
    }

    std::string text = source_.substr(start, pos_ - start);
    return Token{isFloat ? TokenType::Float : TokenType::Integer, text, startLine, startColumn};
}

Token Lexer::scanString() {
    int startLine = line_;
    int startColumn = column_;
    advance(); // consume opening '"'

    std::string value;
    while (true) {
        if (isAtEnd()) {
            std::ostringstream oss;
            oss << "Unterminated string at line " << startLine << ", column " << startColumn;
            throw LexerError(oss.str());
        }

        char c = peek();

        if (c == '"') {
            advance(); // consume closing '"'
            break;
        }

        if (c == '\n') {
            // Strings cannot span lines in Phase 1; this is treated as
            // reaching the end of the string without a closing quote.
            std::ostringstream oss;
            oss << "Unterminated string at line " << startLine << ", column " << startColumn;
            throw LexerError(oss.str());
        }

        if (c == '\\') {
            advance(); // consume backslash
            if (isAtEnd()) {
                std::ostringstream oss;
                oss << "Unterminated string at line " << startLine << ", column " << startColumn;
                throw LexerError(oss.str());
            }
            char esc = advance();
            switch (esc) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case '"':  value += '"';  break;
                case '\\': value += '\\'; break;
                default: {
                    std::ostringstream oss;
                    oss << "Invalid escape sequence '\\" << esc << "' at line "
                        << line_ << ", column " << (column_ - 2);
                    throw LexerError(oss.str());
                }
            }
        } else {
            value += advance();
        }
    }

    return Token{TokenType::String, value, startLine, startColumn};
}

Token Lexer::scanOperatorOrSymbol() {
    int startLine = line_;
    int startColumn = column_;
    char c = advance();

    // Two-character operators first, falling back to their one-character
    // form when the second character doesn't match.
    switch (c) {
        case '=':
            if (match('=')) return Token{TokenType::Operator, "==", startLine, startColumn};
            return Token{TokenType::Operator, "=", startLine, startColumn};
        case '!':
            if (match('=')) return Token{TokenType::Operator, "!=", startLine, startColumn};
            return Token{TokenType::Operator, "!", startLine, startColumn};
        case '>':
            if (match('=')) return Token{TokenType::Operator, ">=", startLine, startColumn};
            return Token{TokenType::Operator, ">", startLine, startColumn};
        case '<':
            if (match('=')) return Token{TokenType::Operator, "<=", startLine, startColumn};
            return Token{TokenType::Operator, "<", startLine, startColumn};
        case '&':
            if (match('&')) return Token{TokenType::Operator, "&&", startLine, startColumn};
            break; // single '&' is not a supported operator in Phase 1
        case '|':
            if (match('|')) return Token{TokenType::Operator, "||", startLine, startColumn};
            break; // single '|' is not a supported operator in Phase 1
        case '+': return Token{TokenType::Operator, "+", startLine, startColumn};
        case '-': return Token{TokenType::Operator, "-", startLine, startColumn};
        case '*': return Token{TokenType::Operator, "*", startLine, startColumn};
        case '/': return Token{TokenType::Operator, "/", startLine, startColumn};
        case '%': return Token{TokenType::Operator, "%", startLine, startColumn};

        case '{': return Token{TokenType::Symbol, "{", startLine, startColumn};
        case '}': return Token{TokenType::Symbol, "}", startLine, startColumn};
        case '(': return Token{TokenType::Symbol, "(", startLine, startColumn};
        case ')': return Token{TokenType::Symbol, ")", startLine, startColumn};
        case '[': return Token{TokenType::Symbol, "[", startLine, startColumn};
        case ']': return Token{TokenType::Symbol, "]", startLine, startColumn};
        case ';': return Token{TokenType::Symbol, ";", startLine, startColumn};
        case ',': return Token{TokenType::Symbol, ",", startLine, startColumn};
        case '.': return Token{TokenType::Symbol, ".", startLine, startColumn};
        case ':': return Token{TokenType::Symbol, ":", startLine, startColumn};

        default:
            break;
    }

    std::ostringstream oss;
    oss << "Unexpected character '" << c << "' at line " << startLine
        << ", column " << startColumn;
    throw LexerError(oss.str());
}

} // namespace virl
