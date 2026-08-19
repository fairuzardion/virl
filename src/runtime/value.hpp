#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "parser/ast.hpp"

namespace virl {

class Environment;
struct ArrayObject;
struct FunctionValue;

// Every Virl runtime value is one of these six alternatives. Numbers
// are always `double` -- see the "Numbers are unified" note in the
// README for why Virl doesn't keep int and float as separate runtime
// types, even though the lexer tokenizes them differently.
using Value = std::variant<
    std::monostate,               // null
    bool,                          // true / false
    double,                        // all numbers
    std::string,                   // strings
    std::shared_ptr<ArrayObject>,  // arrays
    std::shared_ptr<FunctionValue> // functions
>;

struct ArrayObject {
    std::vector<Value> elements;
};

// A function value: the parameter list and body come from the
// FunctionStmt that declared it; `closure` is the environment that was
// active at the point of declaration, which is what lets Virl support
// (simple) closures and recursion.
struct FunctionValue {
    std::string name;
    std::vector<Param> params;
    std::shared_ptr<BlockStmt> body;
    std::shared_ptr<Environment> closure;
};

// A lexical scope: a name -> Value map, plus a pointer to the
// enclosing scope. Lookups and assignments walk up the parent chain,
// which is how inner blocks/functions can see outer variables.
class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
        : parent_(std::move(parent)) {}

    // Introduces a NEW binding in this scope. Shadows any binding of
    // the same name in an enclosing scope.
    void define(const std::string& name, Value value, bool isConst);

    // Looks up `name` in this scope, then enclosing scopes. Returns
    // false (leaving `out` untouched) if not found anywhere.
    bool tryGet(const std::string& name, Value& out) const;

    enum class AssignResult { Success, NotFound, ConstViolation };

    // Assigns to an EXISTING binding, walking up the scope chain to
    // find it. Does not create a new binding.
    AssignResult tryAssign(const std::string& name, const Value& value);

private:
    std::unordered_map<std::string, Value> values_;
    std::unordered_map<std::string, bool> consts_;
    std::shared_ptr<Environment> parent_;
};

// --- Value helpers -----------------------------------------------------

// Short name for a value's runtime type, used in error messages:
// "null", "bool", "number", "string", "array", "function".
std::string typeName(const Value& value);

// Renders a value the way `print()` and string concatenation do.
// Strings render without surrounding quotes; arrays render as
// "[a, b, c]" with their elements quoted if they're strings.
std::string valueToString(const Value& value);

// Virl's truthiness rule: null and false are falsy; 0 is falsy; an
// empty string is falsy; everything else (including empty arrays) is
// truthy. This mirrors common scripting-language conventions while
// staying simple and predictable.
bool isTruthy(const Value& value);

// Structural equality for null/bool/number/string; reference (pointer)
// equality for arrays and functions.
bool valuesEqual(const Value& a, const Value& b);

// Ordering for number-vs-number and string-vs-string only (used by
// sort() and comparison-style builtins). Returns negative/zero/positive
// like strcmp. Callers are responsible for checking both values are
// the same, comparable type first -- this does not throw.
int compareValues(const Value& a, const Value& b);

// --- type-check convenience wrappers ------------------------------------
// Thin wrappers around std::holds_alternative/std::get, added so
// builtins (and any future interpreter code) read as
// `isNumber(v)` / `asNumber(v)` instead of repeating the variant
// machinery at every call site. Purely a readability aid -- they do
// not change behavior.

inline bool isNull(const Value& v)     { return std::holds_alternative<std::monostate>(v); }
inline bool isBool(const Value& v)     { return std::holds_alternative<bool>(v); }
inline bool isNumber(const Value& v)   { return std::holds_alternative<double>(v); }
inline bool isString(const Value& v)   { return std::holds_alternative<std::string>(v); }
inline bool isArray(const Value& v)    { return std::holds_alternative<std::shared_ptr<ArrayObject>>(v); }
inline bool isFunction(const Value& v) { return std::holds_alternative<std::shared_ptr<FunctionValue>>(v); }

// Unchecked accessors -- only call these after confirming the type
// with the corresponding is*() check above (or after a caller has
// already validated the type, e.g. via requireNumber() in builtins.hpp).
inline bool asBool(const Value& v)                          { return std::get<bool>(v); }
inline double asNumber(const Value& v)                       { return std::get<double>(v); }
inline const std::string& asString(const Value& v)           { return std::get<std::string>(v); }
inline const std::shared_ptr<ArrayObject>& asArray(const Value& v) {
    return std::get<std::shared_ptr<ArrayObject>>(v);
}

} // namespace virl
