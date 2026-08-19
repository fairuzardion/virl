#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/value.hpp"

namespace virl {

// Signature every builtin implements: it receives its already-evaluated
// arguments plus the call site's source location (purely so it can
// produce precise error messages), and returns a Value.
using BuiltinFn = std::function<Value(std::vector<Value>& args, int line, int column)>;

// Central registry mapping builtin names ("print", "sqrt", "push", ...)
// to their implementation. Builtins are grouped into separate .cpp
// files by category (core/math/string/array); each category file
// exposes one `registerXBuiltins()` function that adds its own
// builtins to the registry.
//
// Adding a brand new builtin therefore never touches the interpreter:
// write the function in the right category file (or a new one),
// register it, done.
class BuiltinRegistry {
public:
    static BuiltinRegistry& instance();

    void add(const std::string& name, BuiltinFn fn);
    bool has(const std::string& name) const;
    Value call(const std::string& name, std::vector<Value>& args, int line, int column) const;

    // Returns every registered builtin name, sorted, e.g. for a future
    // `--list-builtins` CLI flag or documentation generation.
    std::vector<std::string> names() const;

private:
    BuiltinRegistry();
    std::unordered_map<std::string, BuiltinFn> builtins_;
};

void registerCoreBuiltins(BuiltinRegistry& reg);
void registerMathBuiltins(BuiltinRegistry& reg);
void registerStringBuiltins(BuiltinRegistry& reg);
void registerArrayBuiltins(BuiltinRegistry& reg);

// --- shared argument-validation helpers ---------------------------------
// Every category file leans on these instead of hand-rolling the same
// "wrong number/type of arguments" checks. `argIndex` is 0-based and
// only used for the error message (so users see "argument 1", not
// "argument 0").

namespace argcheck {

void exactCount(const std::string& fnName, size_t got, size_t expected, int line, int column);
void rangeCount(const std::string& fnName, size_t got, size_t minArgs, size_t maxArgs, int line, int column);

double number(const std::string& fnName, const Value& v, int argIndex, int line, int column);
const std::string& string(const std::string& fnName, const Value& v, int argIndex, int line, int column);
const std::shared_ptr<ArrayObject>& array(const std::string& fnName, const Value& v, int argIndex, int line, int column);

} // namespace argcheck

} // namespace virl
