#include <algorithm>
#include <cmath>
#include <random>

#include "builtins/builtins.hpp"
#include "runtime/errors.hpp"

namespace virl {

namespace {

// Collects the "list of numbers" a variadic math builtin should work
// over: either every argument directly (min(1, 2, 3)), or the single
// array argument's elements (min([1, 2, 3])). Shared by min()/max()
// so both accept either calling style.
std::vector<double> collectNumbers(const std::string& fnName, std::vector<Value>& args, int line, int column) {
    if (args.size() == 1 && isArray(args[0])) {
        const auto& elements = asArray(args[0])->elements;
        if (elements.empty()) {
            throw RuntimeError(fnName + "() called with an empty array", line, column);
        }
        std::vector<double> nums;
        nums.reserve(elements.size());
        for (size_t i = 0; i < elements.size(); ++i) {
            nums.push_back(argcheck::number(fnName, elements[i], static_cast<int>(i), line, column));
        }
        return nums;
    }
    if (args.empty()) {
        throw RuntimeError(fnName + "() requires at least 1 argument", line, column);
    }
    std::vector<double> nums;
    nums.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        nums.push_back(argcheck::number(fnName, args[i], static_cast<int>(i), line, column));
    }
    return nums;
}

// A single engine shared by random()/randomInt() for the process's
// lifetime, seeded once from a real entropy source.
std::mt19937& randomEngine() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

} // namespace

void registerMathBuiltins(BuiltinRegistry& reg) {
    reg.add("abs", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("abs", args.size(), 1, line, column);
        return Value(std::fabs(argcheck::number("abs", args[0], 0, line, column)));
    });

    reg.add("sqrt", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("sqrt", args.size(), 1, line, column);
        double n = argcheck::number("sqrt", args[0], 0, line, column);
        if (n < 0.0) throw RuntimeError("sqrt() of a negative number (" + std::to_string(n) + ")", line, column);
        return Value(std::sqrt(n));
    });

    reg.add("pow", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("pow", args.size(), 2, line, column);
        double base = argcheck::number("pow", args[0], 0, line, column);
        double exp = argcheck::number("pow", args[1], 1, line, column);
        return Value(std::pow(base, exp));
    });

    // min(a, b, c, ...) or min(array) -- at least one value required.
    reg.add("min", [](std::vector<Value>& args, int line, int column) -> Value {
        std::vector<double> nums = collectNumbers("min", args, line, column);
        return Value(*std::min_element(nums.begin(), nums.end()));
    });

    // max(a, b, c, ...) or max(array) -- at least one value required.
    reg.add("max", [](std::vector<Value>& args, int line, int column) -> Value {
        std::vector<double> nums = collectNumbers("max", args, line, column);
        return Value(*std::max_element(nums.begin(), nums.end()));
    });

    reg.add("floor", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("floor", args.size(), 1, line, column);
        return Value(std::floor(argcheck::number("floor", args[0], 0, line, column)));
    });

    reg.add("ceil", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("ceil", args.size(), 1, line, column);
        return Value(std::ceil(argcheck::number("ceil", args[0], 0, line, column)));
    });

    reg.add("round", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("round", args.size(), 1, line, column);
        return Value(std::round(argcheck::number("round", args[0], 0, line, column)));
    });

    // random() -- a float in [0, 1).
    reg.add("random", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("random", args.size(), 0, line, column);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value(dist(randomEngine()));
    });

    // randomInt(min, max) -- an integer in [min, max], inclusive on
    // both ends.
    reg.add("randomInt", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("randomInt", args.size(), 2, line, column);
        long long lo = static_cast<long long>(argcheck::number("randomInt", args[0], 0, line, column));
        long long hi = static_cast<long long>(argcheck::number("randomInt", args[1], 1, line, column));
        if (lo > hi) throw RuntimeError("randomInt() min must be <= max", line, column);
        std::uniform_int_distribution<long long> dist(lo, hi);
        return Value(static_cast<double>(dist(randomEngine())));
    });
}

} // namespace virl
