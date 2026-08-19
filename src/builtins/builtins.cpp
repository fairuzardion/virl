#include "builtins/builtins.hpp"

#include <algorithm>

#include "runtime/errors.hpp"

namespace virl {

BuiltinRegistry& BuiltinRegistry::instance() {
    static BuiltinRegistry registry;
    return registry;
}

BuiltinRegistry::BuiltinRegistry() {
    registerCoreBuiltins(*this);
    registerMathBuiltins(*this);
    registerStringBuiltins(*this);
    registerArrayBuiltins(*this);
}

void BuiltinRegistry::add(const std::string& name, BuiltinFn fn) {
    builtins_[name] = std::move(fn);
}

bool BuiltinRegistry::has(const std::string& name) const {
    return builtins_.find(name) != builtins_.end();
}

Value BuiltinRegistry::call(const std::string& name, std::vector<Value>& args, int line, int column) const {
    auto it = builtins_.find(name);
    if (it == builtins_.end()) {
        // The interpreter is expected to call has() before call(), so
        // this should be unreachable -- kept as a defensive guard.
        throw RuntimeError("Unknown builtin '" + name + "'", line, column);
    }
    return it->second(args, line, column);
}

std::vector<std::string> BuiltinRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(builtins_.size());
    for (const auto& [name, fn] : builtins_) result.push_back(name);
    std::sort(result.begin(), result.end());
    return result;
}

namespace argcheck {

void exactCount(const std::string& fnName, size_t got, size_t expected, int line, int column) {
    if (got != expected) {
        throw RuntimeError(
            fnName + "() expects " + std::to_string(expected) + " argument(s), got " + std::to_string(got),
            line, column);
    }
}

void rangeCount(const std::string& fnName, size_t got, size_t minArgs, size_t maxArgs, int line, int column) {
    if (got < minArgs || got > maxArgs) {
        throw RuntimeError(
            fnName + "() expects between " + std::to_string(minArgs) + " and " + std::to_string(maxArgs) +
                " argument(s), got " + std::to_string(got),
            line, column);
    }
}

double number(const std::string& fnName, const Value& v, int argIndex, int line, int column) {
    if (!isNumber(v)) {
        throw RuntimeError(
            fnName + "() argument " + std::to_string(argIndex + 1) + " must be a number (got " +
                typeName(v) + ")",
            line, column);
    }
    return asNumber(v);
}

const std::string& string(const std::string& fnName, const Value& v, int argIndex, int line, int column) {
    if (!isString(v)) {
        throw RuntimeError(
            fnName + "() argument " + std::to_string(argIndex + 1) + " must be a string (got " +
                typeName(v) + ")",
            line, column);
    }
    return asString(v);
}

const std::shared_ptr<ArrayObject>& array(const std::string& fnName, const Value& v, int argIndex, int line, int column) {
    if (!isArray(v)) {
        throw RuntimeError(
            fnName + "() argument " + std::to_string(argIndex + 1) + " must be an array (got " +
                typeName(v) + ")",
            line, column);
    }
    return asArray(v);
}

} // namespace argcheck

} // namespace virl
