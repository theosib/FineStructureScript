#pragma once

#include "value.h"
#include "source_location.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cctype>
#include <stdexcept>

namespace finescript {

class Interner;

// Format a single value with a single printf-style specifier (e.g. "%.2f")
inline std::string formatOneValue(const std::string& spec, const Value& val, Interner* interner) {
    char buf[256];
    int n = 0;
    char convChar = spec.back();
    switch (convChar) {
        case 'd': case 'i': {
            int64_t v = val.isInt() ? val.asInt() :
                        val.isFloat() ? static_cast<int64_t>(val.asFloat()) : 0;
            n = snprintf(buf, sizeof(buf), spec.c_str(), v);
            break;
        }
        case 'f': case 'e': case 'g':
        case 'F': case 'E': case 'G': {
            double v = val.isFloat() ? val.asFloat() :
                       val.isInt() ? static_cast<double>(val.asInt()) : 0.0;
            n = snprintf(buf, sizeof(buf), spec.c_str(), v);
            break;
        }
        case 'x': case 'X': case 'o': {
            int64_t v = val.isInt() ? val.asInt() :
                        val.isFloat() ? static_cast<int64_t>(val.asFloat()) : 0;
            n = snprintf(buf, sizeof(buf), spec.c_str(), v);
            break;
        }
        case 's': {
            std::string s = val.isString() ? val.asString() : val.toString(interner);
            n = snprintf(buf, sizeof(buf), spec.c_str(), s.c_str());
            break;
        }
        default:
            return "%" + std::string(1, convChar);
    }
    if (n < 0) return "";
    return std::string(buf, static_cast<size_t>(std::min(n, (int)(sizeof(buf) - 1))));
}

// Format a string with multiple specifiers and multiple values.
// %% produces a literal %. Each other %... specifier consumes one arg.
inline std::string formatMulti(const std::string& fmt, const std::vector<Value>& args,
                               Interner* interner) {
    std::string result;
    size_t argIdx = 0;
    size_t i = 0;
    while (i < fmt.size()) {
        if (fmt[i] != '%') {
            result += fmt[i++];
            continue;
        }
        // Found '%'
        if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
            result += '%';
            i += 2;
            continue;
        }
        // Extract specifier: %[flags][width][.precision]conversion
        size_t specStart = i;
        i++; // skip '%'
        // Flags: -, +, space, 0, #
        while (i < fmt.size() && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' ||
                                   fmt[i] == '0' || fmt[i] == '#')) {
            i++;
        }
        // Width
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
            i++;
        }
        // Precision
        if (i < fmt.size() && fmt[i] == '.') {
            i++;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                i++;
            }
        }
        // Conversion character
        if (i >= fmt.size()) {
            throw std::runtime_error("Incomplete format specifier in format string");
        }
        i++; // include conversion char
        std::string spec = fmt.substr(specStart, i - specStart);

        if (argIdx >= args.size()) {
            throw std::runtime_error("Not enough arguments for format string");
        }
        result += formatOneValue(spec, args[argIdx], interner);
        argIdx++;
    }
    return result;
}

} // namespace finescript
