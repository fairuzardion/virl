#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

namespace virl {

// RuntimeError is thrown for any problem discovered while executing an
// already-valid program: undefined variables, type mismatches, wrong
// argument counts, out-of-bounds indexing, division by zero, and so
// on. The message is pre-formatted with source location, same style
// as LexerError and ParserError.
//
// This lives in its own header (rather than inside interpreter.hpp)
// specifically so that src/builtins/*.cpp can throw it without needing
// to depend on the interpreter itself -- keeping the dependency
// direction one-way: builtins -> runtime, never runtime -> builtins.
class RuntimeError : public std::runtime_error {
public:
    RuntimeError(const std::string& message, int line, int column)
        : std::runtime_error(formatMessage(message, line, column)) {}

private:
    static std::string formatMessage(const std::string& message, int line, int column) {
        std::ostringstream oss;
        oss << message << " at line " << line << ", column " << column;
        return oss.str();
    }
};

} // namespace virl
