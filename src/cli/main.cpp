#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "builtins/builtins.hpp"
#include "cli/colors.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/parser.hpp"
#include "runtime/interpreter.hpp"

namespace {

constexpr const char* VIRL_VERSION = "0.2.0";

// Reads an entire file into a string. Returns false (and prints an
// error) if the file cannot be opened.
bool readFile(const std::string& path, std::string& outContents) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: could not open file '" << path << "'" << std::endl;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    outContents = buffer.str();
    return true;
}

void printBanner(const virl::cli::Palette& pal) {
    // Built as plain ASCII first so padding is computed on real
    // character counts, then colorized afterward -- avoids the classic
    // "manually counted spaces next to a multi-byte UTF-8 character"
    // misalignment bug.
    const std::string title = "V I R L";
    const std::string tag = "v" + std::string(VIRL_VERSION);
    const std::string inner = "  " + title + "   -   " + tag + "  ";
    const size_t width = inner.size();
    const std::string dashes(width, '-');

    std::cout << pal.cyan("+" + dashes + "+") << "\n"
               << pal.cyan("|") << pal.dim("  ") << pal.boldCyan(title) << "   -   " << tag << "  "
               << pal.cyan("|") << "\n"
               << pal.cyan("+" + dashes + "+") << std::endl;
}

void printUsage(const char* argv0, const virl::cli::Palette& pal) {
    printBanner(pal);
    std::cerr << "\n" << pal.bold("Usage:") << "\n"
               << "  " << argv0 << " <file.virl>            Run a Virl program\n"
               << "  " << argv0 << " --tokens <file.virl>    Print the token stream (debug)\n"
               << "  " << argv0 << " --list-builtins         List every built-in function\n"
               << "  " << argv0 << " --version               Print the Virl version\n"
               << "  " << argv0 << " --help                  Show this message\n"
               << "\nAdd " << pal.dim("--no-color") << " to any command to disable colored output.\n";
}

// Returns the color to use for a given TokenType, so the --tokens
// debug view is easier to scan at a glance.
std::string colorForToken(const virl::cli::Palette& pal, virl::TokenType type, const std::string& text) {
    using virl::TokenType;
    switch (type) {
        case TokenType::Keyword:    return pal.blue(text);
        case TokenType::Identifier: return pal.bold(text);
        case TokenType::Integer:
        case TokenType::Float:      return pal.cyan(text);
        case TokenType::String:     return pal.green(text);
        case TokenType::Boolean:    return pal.magenta(text);
        case TokenType::Operator:   return pal.yellow(text);
        case TokenType::Symbol:     return pal.dim(text);
        case TokenType::EndOfFile:  return pal.dim(text);
    }
    return text;
}

void printToken(const virl::Token& token, const virl::cli::Palette& pal) {
    std::ostringstream location;
    location << "[" << token.line << ":" << token.column << "]";

    std::string typeName = virl::tokenTypeToString(token.type);
    std::string lexeme = token.lexeme.empty() ? pal.dim("<eof>") : colorForToken(pal, token.type, token.lexeme);

    std::cout << pal.dim(location.str())
               << std::string(std::max<int>(1, 11 - static_cast<int>(location.str().size())), ' ')
               << std::left << std::setw(14) << typeName
               << lexeme << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    bool noColor = false;
    for (auto it = args.begin(); it != args.end();) {
        if (*it == "--no-color") {
            noColor = true;
            it = args.erase(it);
        } else {
            ++it;
        }
    }

    virl::cli::Palette pal(!noColor && virl::cli::stdoutIsTerminal());

    if (args.empty()) {
        printUsage(argv[0], pal);
        return 1;
    }

    if (args[0] == "--help" || args[0] == "-h") {
        printUsage(argv[0], pal);
        return 0;
    }

    if (args[0] == "--version" || args[0] == "-v") {
        printBanner(pal);
        return 0;
    }

    if (args[0] == "--list-builtins") {
        std::cout << pal.bold("Built-in functions:") << std::endl;
        for (const std::string& name : virl::BuiltinRegistry::instance().names()) {
            std::cout << "  " << pal.cyan(name) << "()" << std::endl;
        }
        return 0;
    }

    bool tokensMode = false;
    std::string path;

    if (args.size() == 1) {
        path = args[0];
    } else if (args.size() == 2 && args[0] == "--tokens") {
        tokensMode = true;
        path = args[1];
    } else {
        printUsage(argv[0], pal);
        return 1;
    }

    std::string source;
    if (!readFile(path, source)) {
        return 1;
    }

    if (tokensMode) {
        virl::Lexer lexer(source);
        try {
            std::vector<virl::Token> tokens = lexer.tokenize();
            for (const virl::Token& token : tokens) {
                printToken(token, pal);
            }
        } catch (const virl::LexerError& e) {
            std::cerr << pal.red("Lexer error:") << " " << e.what() << std::endl;
            return 1;
        }
        return 0;
    }

    try {
        virl::Lexer lexer(source);
        std::vector<virl::Token> tokens = lexer.tokenize();

        virl::Parser parser(std::move(tokens));
        std::vector<virl::StmtPtr> program = parser.parseProgram();

        virl::Interpreter interpreter;
        interpreter.run(program);
    } catch (const virl::LexerError& e) {
        std::cerr << pal.red("Lexer error:") << " " << e.what() << std::endl;
        return 1;
    } catch (const virl::ParserError& e) {
        std::cerr << pal.red("Parser error:") << " " << e.what() << std::endl;
        return 1;
    } catch (const virl::RuntimeError& e) {
        std::cerr << pal.red("Runtime error:") << " " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
