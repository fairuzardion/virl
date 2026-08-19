#include <algorithm>

#include "builtins/builtins.hpp"
#include "runtime/errors.hpp"

namespace virl {

void registerArrayBuiltins(BuiltinRegistry& reg) {
    // push(array, value) -- mutates the array in place, returns the
    // new length. Mutating in place (rather than returning a new
    // array) matches how index-assignment (`arr[0] = x`) already
    // behaves, and avoids an O(n) copy on every push.
    reg.add("push", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("push", args.size(), 2, line, column);
        const auto& arr = argcheck::array("push", args[0], 0, line, column);
        arr->elements.push_back(args[1]);
        return Value(static_cast<double>(arr->elements.size()));
    });

    // pop(array) -- removes and returns the last element.
    reg.add("pop", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("pop", args.size(), 1, line, column);
        const auto& arr = argcheck::array("pop", args[0], 0, line, column);
        if (arr->elements.empty()) throw RuntimeError("pop() called on an empty array", line, column);
        Value last = arr->elements.back();
        arr->elements.pop_back();
        return last;
    });

    reg.add("first", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("first", args.size(), 1, line, column);
        const auto& arr = argcheck::array("first", args[0], 0, line, column);
        if (arr->elements.empty()) throw RuntimeError("first() called on an empty array", line, column);
        return arr->elements.front();
    });

    reg.add("last", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("last", args.size(), 1, line, column);
        const auto& arr = argcheck::array("last", args[0], 0, line, column);
        if (arr->elements.empty()) throw RuntimeError("last() called on an empty array", line, column);
        return arr->elements.back();
    });

    // reverse(array) -- mutates in place and returns the same array,
    // so calls can be chained (e.g. `print(reverse(numbers));`).
    reg.add("reverse", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("reverse", args.size(), 1, line, column);
        const auto& arr = argcheck::array("reverse", args[0], 0, line, column);
        std::reverse(arr->elements.begin(), arr->elements.end());
        return args[0];
    });

    // sort(array) -- ascending order. Requires every element to be the
    // SAME comparable type (all numbers or all strings); mixed-type
    // arrays are rejected up front with a clear error rather than
    // producing an arbitrary ordering.
    reg.add("sort", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("sort", args.size(), 1, line, column);
        const auto& arr = argcheck::array("sort", args[0], 0, line, column);

        bool allNumbers = true, allStrings = true;
        for (const Value& v : arr->elements) {
            if (!isNumber(v)) allNumbers = false;
            if (!isString(v)) allStrings = false;
        }
        if (!allNumbers && !allStrings) {
            throw RuntimeError("sort() requires an array of only numbers or only strings", line, column);
        }
        std::sort(arr->elements.begin(), arr->elements.end(),
                  [](const Value& a, const Value& b) { return compareValues(a, b) < 0; });
        return args[0];
    });

    // slice(array, start[, end]) -- returns a NEW array (unlike
    // push/pop/reverse/sort, which mutate); end defaults to the
    // array's length. Bounds are clamped, matching substring().
    reg.add("slice", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::rangeCount("slice", args.size(), 2, 3, line, column);
        const auto& arr = argcheck::array("slice", args[0], 0, line, column);
        long long size = static_cast<long long>(arr->elements.size());

        long long start = static_cast<long long>(argcheck::number("slice", args[1], 1, line, column));
        long long end = args.size() == 3
                             ? static_cast<long long>(argcheck::number("slice", args[2], 2, line, column))
                             : size;

        start = std::clamp<long long>(start, 0, size);
        end = std::clamp<long long>(end, start, size);

        auto result = std::make_shared<ArrayObject>();
        result->elements.assign(arr->elements.begin() + start, arr->elements.begin() + end);
        return Value(result);
    });

    // indexOf(array, value) -- first matching index, or -1.
    reg.add("indexOf", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("indexOf", args.size(), 2, line, column);
        const auto& arr = argcheck::array("indexOf", args[0], 0, line, column);
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            if (valuesEqual(arr->elements[i], args[1])) return Value(static_cast<double>(i));
        }
        return Value(-1.0);
    });
}

} // namespace virl
