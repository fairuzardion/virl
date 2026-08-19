#include "runtime/interpreter.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

#include "builtins/builtins.hpp"

namespace virl {

namespace {

// Control-flow is implemented with small "signal" exceptions rather
// than threading a status code through every execute() call. This
// keeps the tree-walker's control-flow code close to how someone
// would describe it in English: "return throws its value up to the
// nearest function call; break/continue throw up to the nearest loop."
struct ReturnSignal {
    Value value;
};
struct BreakSignal {};
struct ContinueSignal {};

} // namespace

Interpreter::Interpreter() : globals_(std::make_shared<Environment>()) {}

void Interpreter::run(const std::vector<StmtPtr>& program) {
    for (const StmtPtr& stmt : program) {
        execute(stmt.get(), globals_);
    }

    // Entry-point convention: if a `main` function was declared at the
    // top level, run it automatically after all top-level declarations
    // have executed.
    Value mainValue;
    if (globals_->tryGet("main", mainValue) &&
        std::holds_alternative<std::shared_ptr<FunctionValue>>(mainValue)) {
        std::vector<Value> noArgs;
        callFunction(std::get<std::shared_ptr<FunctionValue>>(mainValue), noArgs, 0, 0);
    }
}

void Interpreter::executeBlock(BlockStmt* block, const std::shared_ptr<Environment>& env) {
    for (const StmtPtr& stmt : block->statements) {
        execute(stmt.get(), env);
    }
}

void Interpreter::execute(Stmt* stmt, const std::shared_ptr<Environment>& env) {
    switch (stmt->kind) {
        case StmtKind::Let: {
            auto* letStmt = static_cast<LetStmt*>(stmt);
            Value value = letStmt->initializer ? evaluate(letStmt->initializer.get(), env)
                                                 : Value(std::monostate{});
            env->define(letStmt->name, value, false);
            break;
        }
        case StmtKind::Const: {
            auto* constStmt = static_cast<ConstStmt*>(stmt);
            Value value = evaluate(constStmt->initializer.get(), env);
            env->define(constStmt->name, value, true);
            break;
        }
        case StmtKind::Block: {
            auto* blockStmt = static_cast<BlockStmt*>(stmt);
            executeBlock(blockStmt, std::make_shared<Environment>(env));
            break;
        }
        case StmtKind::If: {
            auto* ifStmt = static_cast<IfStmt*>(stmt);
            Value cond = evaluate(ifStmt->condition.get(), env);
            if (isTruthy(cond)) {
                executeBlock(ifStmt->thenBranch.get(), std::make_shared<Environment>(env));
            } else if (ifStmt->elseBranch) {
                execute(ifStmt->elseBranch.get(), env);
            }
            break;
        }
        case StmtKind::While: {
            auto* whileStmt = static_cast<WhileStmt*>(stmt);
            while (isTruthy(evaluate(whileStmt->condition.get(), env))) {
                try {
                    executeBlock(whileStmt->body.get(), std::make_shared<Environment>(env));
                } catch (BreakSignal&) {
                    break;
                } catch (ContinueSignal&) {
                    continue;
                }
            }
            break;
        }
        case StmtKind::For: {
            auto* forStmt = static_cast<ForStmt*>(stmt);
            Value iterableValue = evaluate(forStmt->iterable.get(), env);
            if (!std::holds_alternative<std::shared_ptr<ArrayObject>>(iterableValue)) {
                throw RuntimeError(
                    "for-in requires an array (got " + typeName(iterableValue) + ")",
                    forStmt->line, forStmt->column);
            }
            auto arr = std::get<std::shared_ptr<ArrayObject>>(iterableValue);
            // Copy the element list before iterating so mutating the
            // source array inside the loop body doesn't invalidate
            // iteration (simple and predictable, if not maximally
            // efficient -- fine for Phase-current scope).
            std::vector<Value> elements = arr->elements;
            for (const Value& element : elements) {
                auto loopEnv = std::make_shared<Environment>(env);
                loopEnv->define(forStmt->variableName, element, false);
                try {
                    executeBlock(forStmt->body.get(), loopEnv);
                } catch (BreakSignal&) {
                    break;
                } catch (ContinueSignal&) {
                    continue;
                }
            }
            break;
        }
        case StmtKind::Function: {
            auto* fnStmt = static_cast<FunctionStmt*>(stmt);
            auto fnValue = std::make_shared<FunctionValue>();
            fnValue->name = fnStmt->name;
            fnValue->params = fnStmt->params;
            fnValue->body = fnStmt->body;
            fnValue->closure = env;
            // Functions are bound as constants: reassigning a function
            // name (`main = 5;`) is rejected, the same way most
            // scripting languages protect top-level declarations.
            env->define(fnStmt->name, Value(fnValue), true);
            break;
        }
        case StmtKind::Return: {
            auto* returnStmt = static_cast<ReturnStmt*>(stmt);
            Value value = returnStmt->value ? evaluate(returnStmt->value.get(), env)
                                              : Value(std::monostate{});
            throw ReturnSignal{value};
        }
        case StmtKind::Break:
            throw BreakSignal{};
        case StmtKind::Continue:
            throw ContinueSignal{};
        case StmtKind::Expression: {
            auto* exprStmt = static_cast<ExpressionStmt*>(stmt);
            evaluate(exprStmt->expression.get(), env);
            break;
        }
    }
}

Value Interpreter::evaluate(Expr* expr, const std::shared_ptr<Environment>& env) {
    switch (expr->kind) {
        case ExprKind::Literal: {
            auto* lit = static_cast<LiteralExpr*>(expr);
            switch (lit->literalKind) {
                case LiteralKind::Number: return Value(lit->numberValue);
                case LiteralKind::String: return Value(lit->stringValue);
                case LiteralKind::Bool:   return Value(lit->boolValue);
                case LiteralKind::Null:   return Value(std::monostate{});
            }
            return Value(std::monostate{});
        }

        case ExprKind::Variable: {
            auto* varExpr = static_cast<VariableExpr*>(expr);
            Value value;
            if (!env->tryGet(varExpr->name, value)) {
                throw RuntimeError("Undefined variable '" + varExpr->name + "'",
                                    varExpr->line, varExpr->column);
            }
            return value;
        }

        case ExprKind::Assign: {
            auto* assignExpr = static_cast<AssignExpr*>(expr);
            Value value = evaluate(assignExpr->value.get(), env);
            auto result = env->tryAssign(assignExpr->name, value);
            if (result == Environment::AssignResult::NotFound) {
                throw RuntimeError("Undefined variable '" + assignExpr->name + "'",
                                    assignExpr->line, assignExpr->column);
            }
            if (result == Environment::AssignResult::ConstViolation) {
                throw RuntimeError("Cannot assign to constant '" + assignExpr->name + "'",
                                    assignExpr->line, assignExpr->column);
            }
            return value;
        }

        case ExprKind::IndexAssign: {
            auto* idxAssign = static_cast<IndexAssignExpr*>(expr);
            Value targetValue = evaluate(idxAssign->target.get(), env);
            if (!std::holds_alternative<std::shared_ptr<ArrayObject>>(targetValue)) {
                throw RuntimeError("Cannot index into a " + typeName(targetValue),
                                    idxAssign->line, idxAssign->column);
            }
            auto arr = std::get<std::shared_ptr<ArrayObject>>(targetValue);

            Value indexValue = evaluate(idxAssign->index.get(), env);
            if (!std::holds_alternative<double>(indexValue)) {
                throw RuntimeError("Array index must be a number", idxAssign->line, idxAssign->column);
            }
            double rawIndex = std::get<double>(indexValue);
            long long idx = static_cast<long long>(rawIndex);
            if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size()) {
                throw RuntimeError("Array index out of bounds", idxAssign->line, idxAssign->column);
            }

            Value value = evaluate(idxAssign->value.get(), env);
            arr->elements[static_cast<size_t>(idx)] = value;
            return value;
        }

        case ExprKind::Binary: {
            auto* bin = static_cast<BinaryExpr*>(expr);
            Value left = evaluate(bin->left.get(), env);
            Value right = evaluate(bin->right.get(), env);
            const std::string& op = bin->op;

            if (op == "==") return Value(valuesEqual(left, right));
            if (op == "!=") return Value(!valuesEqual(left, right));

            if (op == "+") {
                // Numeric addition if both sides are numbers; otherwise
                // fall back to string concatenation (using each side's
                // display form) if at least one side is a string. This
                // is what makes `"Hello " + name` work without an
                // explicit cast, while keeping `+` on two numbers exact.
                if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right)) {
                    return Value(std::get<double>(left) + std::get<double>(right));
                }
                if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
                    return Value(valueToString(left) + valueToString(right));
                }
                throw RuntimeError(
                    "Cannot add " + typeName(left) + " and " + typeName(right),
                    bin->line, bin->column);
            }

            // Every remaining binary operator requires two numbers.
            if (!std::holds_alternative<double>(left) || !std::holds_alternative<double>(right)) {
                throw RuntimeError(
                    "Operator '" + op + "' requires two numbers (got " +
                        typeName(left) + " and " + typeName(right) + ")",
                    bin->line, bin->column);
            }
            double l = std::get<double>(left);
            double r = std::get<double>(right);

            if (op == "-") return Value(l - r);
            if (op == "*") return Value(l * r);
            if (op == "/") {
                if (r == 0.0) throw RuntimeError("Division by zero", bin->line, bin->column);
                return Value(l / r);
            }
            if (op == "%") {
                if (r == 0.0) throw RuntimeError("Division by zero", bin->line, bin->column);
                return Value(std::fmod(l, r));
            }
            if (op == ">")  return Value(l > r);
            if (op == "<")  return Value(l < r);
            if (op == ">=") return Value(l >= r);
            if (op == "<=") return Value(l <= r);

            throw RuntimeError("Unknown operator '" + op + "'", bin->line, bin->column);
        }

        case ExprKind::Logical: {
            auto* logical = static_cast<LogicalExpr*>(expr);
            Value left = evaluate(logical->left.get(), env);
            if (logical->op == "&&") {
                if (!isTruthy(left)) return Value(false);
                return Value(isTruthy(evaluate(logical->right.get(), env)));
            }
            // "||"
            if (isTruthy(left)) return Value(true);
            return Value(isTruthy(evaluate(logical->right.get(), env)));
        }

        case ExprKind::Unary: {
            auto* unary = static_cast<UnaryExpr*>(expr);
            Value operand = evaluate(unary->operand.get(), env);
            if (unary->op == "!") return Value(!isTruthy(operand));
            // "-"
            if (!std::holds_alternative<double>(operand)) {
                throw RuntimeError("Unary '-' requires a number (got " + typeName(operand) + ")",
                                    unary->line, unary->column);
            }
            return Value(-std::get<double>(operand));
        }

        case ExprKind::Call: {
            auto* call = static_cast<CallExpr*>(expr);

            std::vector<Value> args;
            args.reserve(call->args.size());
            for (const ExprPtr& argExpr : call->args) {
                args.push_back(evaluate(argExpr.get(), env));
            }

            // If the callee is a bare name, resolve it as: (1) a
            // user-defined variable/function first -- so a program can
            // shadow a builtin name like `range` if it really wants to
            // -- and only (2) fall back to the builtin registry if no
            // such name exists in scope.
            if (call->callee->kind == ExprKind::Variable) {
                const std::string& name = static_cast<VariableExpr*>(call->callee.get())->name;

                Value userValue;
                if (env->tryGet(name, userValue)) {
                    if (!std::holds_alternative<std::shared_ptr<FunctionValue>>(userValue)) {
                        throw RuntimeError("Value of type " + typeName(userValue) + " is not callable",
                                            call->line, call->column);
                    }
                    return callFunction(std::get<std::shared_ptr<FunctionValue>>(userValue), args,
                                          call->line, call->column);
                }

                if (BuiltinRegistry::instance().has(name)) {
                    return BuiltinRegistry::instance().call(name, args, call->line, call->column);
                }

                throw RuntimeError("Undefined function '" + name + "'", call->line, call->column);
            }

            // General case: the callee is itself an expression (e.g. a
            // function returned from another call). Builtins are not
            // reachable this way -- see README "Design decisions".
            Value calleeValue = evaluate(call->callee.get(), env);
            if (!std::holds_alternative<std::shared_ptr<FunctionValue>>(calleeValue)) {
                throw RuntimeError("Value of type " + typeName(calleeValue) + " is not callable",
                                    call->line, call->column);
            }
            return callFunction(std::get<std::shared_ptr<FunctionValue>>(calleeValue), args,
                                  call->line, call->column);
        }

        case ExprKind::Index: {
            auto* idx = static_cast<IndexExpr*>(expr);
            Value targetValue = evaluate(idx->target.get(), env);
            if (!std::holds_alternative<std::shared_ptr<ArrayObject>>(targetValue)) {
                throw RuntimeError("Cannot index into a " + typeName(targetValue),
                                    idx->line, idx->column);
            }
            auto arr = std::get<std::shared_ptr<ArrayObject>>(targetValue);

            Value indexValue = evaluate(idx->index.get(), env);
            if (!std::holds_alternative<double>(indexValue)) {
                throw RuntimeError("Array index must be a number", idx->line, idx->column);
            }
            long long i = static_cast<long long>(std::get<double>(indexValue));
            if (i < 0 || static_cast<size_t>(i) >= arr->elements.size()) {
                throw RuntimeError("Array index out of bounds", idx->line, idx->column);
            }
            return arr->elements[static_cast<size_t>(i)];
        }

        case ExprKind::Array: {
            auto* arrExpr = static_cast<ArrayExpr*>(expr);
            auto arr = std::make_shared<ArrayObject>();
            arr->elements.reserve(arrExpr->elements.size());
            for (const ExprPtr& elemExpr : arrExpr->elements) {
                arr->elements.push_back(evaluate(elemExpr.get(), env));
            }
            return Value(arr);
        }
    }

    throw RuntimeError("Internal error: unhandled expression kind", expr->line, expr->column);
}

Value Interpreter::callFunction(const std::shared_ptr<FunctionValue>& fn, std::vector<Value>& args,
                                  int callLine, int callColumn) {
    if (args.size() != fn->params.size()) {
        throw RuntimeError(
            "Function '" + fn->name + "' expects " + std::to_string(fn->params.size()) +
                " argument(s), got " + std::to_string(args.size()),
            callLine, callColumn);
    }

    auto callEnv = std::make_shared<Environment>(fn->closure);
    for (size_t i = 0; i < fn->params.size(); ++i) {
        callEnv->define(fn->params[i].name, args[i], false);
    }

    try {
        executeBlock(fn->body.get(), callEnv);
    } catch (ReturnSignal& ret) {
        return ret.value;
    }
    // No explicit `return` reached: functions implicitly return null.
    return Value(std::monostate{});
}

} // namespace virl
