#include "runtime/value.hpp"

#include <cmath>
#include <sstream>

namespace virl {

void Environment::define(const std::string& name, Value value, bool isConst) {
    values_[name] = std::move(value);
    consts_[name] = isConst;
}

bool Environment::tryGet(const std::string& name, Value& out) const {
    auto it = values_.find(name);
    if (it != values_.end()) {
        out = it->second;
        return true;
    }
    if (parent_) return parent_->tryGet(name, out);
    return false;
}

Environment::AssignResult Environment::tryAssign(const std::string& name, const Value& value) {
    auto it = values_.find(name);
    if (it != values_.end()) {
        if (consts_[name]) return AssignResult::ConstViolation;
        it->second = value;
        return AssignResult::Success;
    }
    if (parent_) return parent_->tryAssign(name, value);
    return AssignResult::NotFound;
}

std::string typeName(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return "null";
    if (std::holds_alternative<bool>(value)) return "bool";
    if (std::holds_alternative<double>(value)) return "number";
    if (std::holds_alternative<std::string>(value)) return "string";
    if (std::holds_alternative<std::shared_ptr<ArrayObject>>(value)) return "array";
    if (std::holds_alternative<std::shared_ptr<FunctionValue>>(value)) return "function";
    return "unknown";
}

namespace {

// Formats a double the way Virl source code would write it: whole
// numbers print without a trailing ".0" (so `let x = 10;` prints as
// "10", not "10.000000"), while non-whole numbers print with just
// enough precision to round-trip.
std::string formatNumber(double n) {
    if (std::isfinite(n) && n == std::floor(n) &&
        std::abs(n) < 1e15) {
        std::ostringstream oss;
        oss << static_cast<long long>(n);
        return oss.str();
    }
    std::ostringstream oss;
    oss.precision(15);
    oss << n;
    return oss.str();
}

// Renders a value for use INSIDE a larger structure (currently: array
// elements), where strings need quotes to stay distinguishable from
// other elements. Top-level valueToString() does not quote strings,
// since `print("hi")` should print `hi`, not `"hi"`.
std::string valueToDisplayString(const Value& value) {
    if (std::holds_alternative<std::string>(value)) {
        return "\"" + std::get<std::string>(value) + "\"";
    }
    return valueToString(value);
}

} // namespace

std::string valueToString(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return "null";
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
    if (std::holds_alternative<double>(value)) return formatNumber(std::get<double>(value));
    if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);

    if (std::holds_alternative<std::shared_ptr<ArrayObject>>(value)) {
        const auto& arr = std::get<std::shared_ptr<ArrayObject>>(value);
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << valueToDisplayString(arr->elements[i]);
        }
        oss << "]";
        return oss.str();
    }

    if (std::holds_alternative<std::shared_ptr<FunctionValue>>(value)) {
        const auto& fn = std::get<std::shared_ptr<FunctionValue>>(value);
        return "<fn " + fn->name + ">";
    }

    return "null";
}

bool isTruthy(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return false;
    if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
    if (std::holds_alternative<double>(value)) return std::get<double>(value) != 0.0;
    if (std::holds_alternative<std::string>(value)) return !std::get<std::string>(value).empty();
    // Arrays and functions are always truthy (their pointer is never
    // null when this variant alternative is active).
    return true;
}

bool valuesEqual(const Value& a, const Value& b) {
    if (a.index() != b.index()) return false;

    if (std::holds_alternative<std::monostate>(a)) return true;
    if (std::holds_alternative<bool>(a)) return std::get<bool>(a) == std::get<bool>(b);
    if (std::holds_alternative<double>(a)) return std::get<double>(a) == std::get<double>(b);
    if (std::holds_alternative<std::string>(a)) return std::get<std::string>(a) == std::get<std::string>(b);
    if (std::holds_alternative<std::shared_ptr<ArrayObject>>(a)) {
        return std::get<std::shared_ptr<ArrayObject>>(a).get() ==
               std::get<std::shared_ptr<ArrayObject>>(b).get();
    }
    if (std::holds_alternative<std::shared_ptr<FunctionValue>>(a)) {
        return std::get<std::shared_ptr<FunctionValue>>(a).get() ==
               std::get<std::shared_ptr<FunctionValue>>(b).get();
    }
    return false;
}

int compareValues(const Value& a, const Value& b) {
    if (isNumber(a) && isNumber(b)) {
        double da = asNumber(a), db = asNumber(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (isString(a) && isString(b)) {
        const std::string& sa = asString(a);
        const std::string& sb = asString(b);
        if (sa < sb) return -1;
        if (sa > sb) return 1;
        return 0;
    }
    // Not comparable -- caller should have validated types first.
    return 0;
}

} // namespace virl
