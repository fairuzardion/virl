#pragma once

#include <memory>
#include <string>
#include <vector>

#include "parser/ast.hpp"
#include "runtime/errors.hpp"
#include "runtime/value.hpp"

namespace virl {

// Interpreter walks the AST directly (no bytecode compilation step --
// see the project README for why that's a deliberate Phase choice)
// and executes it against a tree of Environments. All Virl builtins
// (print, sqrt, push, ...) live outside this class entirely, in
// src/builtins/ -- see BuiltinRegistry.
class Interpreter {
public:
    Interpreter();

    // Executes every top-level statement in `program`, in order. If a
    // function named "main" was defined anywhere at the top level, it
    // is then called automatically with no arguments -- this is Virl's
    // program entry-point convention.
    void run(const std::vector<StmtPtr>& program);

private:
    std::shared_ptr<Environment> globals_;

    void execute(Stmt* stmt, const std::shared_ptr<Environment>& env);
    void executeBlock(BlockStmt* block, const std::shared_ptr<Environment>& env);
    Value evaluate(Expr* expr, const std::shared_ptr<Environment>& env);

    Value callFunction(const std::shared_ptr<FunctionValue>& fn,
                        std::vector<Value>& args,
                        int callLine, int callColumn);
};

} // namespace virl
