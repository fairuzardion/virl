#include <algorithm>
#include <cctype>

#include "builtins/builtins.hpp"
#include "runtime/errors.hpp"

namespace virl {

namespace {

std::string toUpperCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trimCopy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

} // namespace

void registerStringBuiltins(BuiltinRegistry& reg) {
    reg.add("upper", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("upper", args.size(), 1, line, column);
        return Value(toUpperCopy(argcheck::string("upper", args[0], 0, line, column)));
    });

    reg.add("lower", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("lower", args.size(), 1, line, column);
        return Value(toLowerCopy(argcheck::string("lower", args[0], 0, line, column)));
    });

    reg.add("trim", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("trim", args.size(), 1, line, column);
        return Value(trimCopy(argcheck::string("trim", args[0], 0, line, column)));
    });

    // split(s, delimiter) -- delimiter "" splits into individual
    // characters; otherwise splits on every occurrence of delimiter.
    reg.add("split", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("split", args.size(), 2, line, column);
        const std::string& s = argcheck::string("split", args[0], 0, line, column);
        const std::string& delim = argcheck::string("split", args[1], 1, line, column);

        auto arr = std::make_shared<ArrayObject>();
        if (delim.empty()) {
            for (char c : s) arr->elements.push_back(Value(std::string(1, c)));
            return Value(arr);
        }
        size_t pos = 0;
        while (true) {
            size_t next = s.find(delim, pos);
            if (next == std::string::npos) {
                arr->elements.push_back(Value(s.substr(pos)));
                break;
            }
            arr->elements.push_back(Value(s.substr(pos, next - pos)));
            pos = next + delim.size();
        }
        return Value(arr);
    });

    // join(array, delimiter) -- inverse of split(); elements are
    // rendered with valueToString() (numbers/booleans/etc. are
    // stringified automatically).
    reg.add("join", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("join", args.size(), 2, line, column);
        const auto& arr = argcheck::array("join", args[0], 0, line, column);
        const std::string& delim = argcheck::string("join", args[1], 1, line, column);

        std::string result;
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            if (i > 0) result += delim;
            result += valueToString(arr->elements[i]);
        }
        return Value(result);
    });

    // contains(container, item) -- substring search for strings,
    // element search (by value) for arrays.
    reg.add("contains", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("contains", args.size(), 2, line, column);
        if (isString(args[0])) {
            const std::string& needle = argcheck::string("contains", args[1], 1, line, column);
            return Value(asString(args[0]).find(needle) != std::string::npos);
        }
        if (isArray(args[0])) {
            for (const Value& element : asArray(args[0])->elements) {
                if (valuesEqual(element, args[1])) return Value(true);
            }
            return Value(false);
        }
        throw RuntimeError("contains() argument 1 must be a string or array (got " +
                                typeName(args[0]) + ")",
                            line, column);
    });

    // replace(s, target, replacement) -- replaces every occurrence.
    reg.add("replace", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::exactCount("replace", args.size(), 3, line, column);
        std::string s = argcheck::string("replace", args[0], 0, line, column);
        const std::string& target = argcheck::string("replace", args[1], 1, line, column);
        const std::string& replacement = argcheck::string("replace", args[2], 2, line, column);

        if (target.empty()) return Value(s); // avoid an infinite loop on an empty target
        size_t pos = 0;
        while ((pos = s.find(target, pos)) != std::string::npos) {
            s.replace(pos, target.size(), replacement);
            pos += replacement.size();
        }
        return Value(s);
    });

    // substring(s, start[, end]) -- end defaults to the string's
    // length; both bounds are clamped into range rather than erroring,
    // so callers don't need to pre-clamp themselves.
    reg.add("substring", [](std::vector<Value>& args, int line, int column) -> Value {
        argcheck::rangeCount("substring", args.size(), 2, 3, line, column);
        const std::string& s = argcheck::string("substring", args[0], 0, line, column);
        long long start = static_cast<long long>(argcheck::number("substring", args[1], 1, line, column));
        long long end = args.size() == 3
                             ? static_cast<long long>(argcheck::number("substring", args[2], 2, line, column))
                             : static_cast<long long>(s.size());

        start = std::clamp<long long>(start, 0, static_cast<long long>(s.size()));
        end = std::clamp<long long>(end, start, static_cast<long long>(s.size()));
        return Value(s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    });
}

} // namespace virl
