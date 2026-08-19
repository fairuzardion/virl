#include <cmath>
#include <iostream>
#include <stdexcept>

#include "builtins/builtins.hpp"
#include "runtime/errors.hpp"

namespace virl {

void registerCoreBuiltins(BuiltinRegistry& reg) {
    // print(...) -- prints every argument separated by a space,
    // followed by a newline. Returns null.
    reg.add("print", [](std::vector<Value>& args, int, int) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << valueToString(args[i]);
        }
        std::cout << std::endl;
        return Value(std::monostate{});
    });

    // len(arrayOrString) -- element count / character count.
    reg.add("len", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("len", args.size(), 1, line, column);
        if (isArray(args[0])) return Value(static_cast<double>(asArray(args[0])->elements.size()));
        if (isString(args[0])) return Value(static_cast<double>(asString(args[0]).size()));
        throw RuntimeError("len() expects an array or string, got " + typeName(args[0]), line, column);
    });

    // typeof(x) -- runtime type name: "null", "bool", "number",
    // "string", "array", or "function".
    reg.add("typeof", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("typeof", args.size(), 1, line, column);
        return Value(typeName(args[0]));
    });

    // toString(x) -- the same rendering print() uses, as a string you
    // can store, concatenate, or pass along.
    reg.add("toString", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("toString", args.size(), 1, line, column);
        return Value(valueToString(args[0]));
    });

    // toNumber(x) -- parses a string to a number, passes numbers
    // through unchanged, and converts true/false to 1/0.
    reg.add("toNumber", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("toNumber", args.size(), 1, line, column);
        const Value& v = args[0];
        if (isNumber(v)) return v;
        if (isBool(v)) return Value(asBool(v) ? 1.0 : 0.0);
        if (isString(v)) {
            try {
                size_t consumed = 0;
                double parsed = std::stod(asString(v), &consumed);
                if (consumed != asString(v).size()) throw std::invalid_argument("trailing characters");
                return Value(parsed);
            } catch (const std::exception&) {
                throw RuntimeError("toNumber() could not convert \"" + asString(v) + "\" to a number",
                                    line, column);
            }
        }
        throw RuntimeError("toNumber() cannot convert " + typeName(v) + " to a number", line, column);
    });

    // input([prompt]) -- prints an optional prompt (no trailing
    // newline), reads one line from stdin, and returns it as a string.
    reg.add("input", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::rangeCount("input", args.size(), 0, 1, line, column);
        if (!args.empty()) {
            std::cout << argcheck::string("input", args[0], 0, line, column);
            std::cout.flush();
        }
        std::string result;
        if (!std::getline(std::cin, result)) {
            // EOF on stdin: return an empty string rather than
            // throwing, so scripts piping input don't crash on the
            // last read.
            result.clear();
        }
        return Value(result);
    });

    // range(end) / range(start, end) / range(start, end, step)
    // Produces an array of numbers. `step` may be negative to count
    // down, but may not be zero.
    reg.add("range", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::rangeCount("range", args.size(), 1, 3, line, column);

        double start = 0.0, end, step = 1.0;
        if (args.size() == 1) {
            end = argcheck::number("range", args[0], 0, line, column);
        } else {
            start = argcheck::number("range", args[0], 0, line, column);
            end = argcheck::number("range", args[1], 1, line, column);
            if (args.size() == 3) {
                step = argcheck::number("range", args[2], 2, line, column);
            }
        }
        if (step == 0.0) {
            throw RuntimeError("range() step cannot be 0", line, column);
        }

        auto arr = std::make_shared<ArrayObject>();
        if (step > 0) {
            for (double v = start; v < end; v += step) arr->elements.push_back(Value(v));
        } else {
            for (double v = start; v > end; v += step) arr->elements.push_back(Value(v));
        }
        return Value(arr);
    });
}

} // namespace virl
