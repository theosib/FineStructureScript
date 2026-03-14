#include "finescript/config_builtins.h"
#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace finescript {
namespace {

// Trim leading and trailing whitespace
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

// Try to parse a value string into the most specific type
Value parseConfigValue(std::string_view val) {
    if (val == "true") return Value::boolean(true);
    if (val == "false") return Value::boolean(false);

    // Try integer
    {
        int64_t i = 0;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), i);
        if (ec == std::errc{} && ptr == val.data() + val.size()) {
            return Value::integer(i);
        }
    }

    // Try float
    {
        // from_chars for double may not be available on all platforms, use stod
        try {
            size_t pos = 0;
            double d = std::stod(std::string(val), &pos);
            if (pos == val.size()) {
                return Value::number(d);
            }
        } catch (...) {}
    }

    // Default: string
    return Value::string(std::string(val));
}

// Format a Value for config output
std::string formatConfigValue(const Value& val) {
    if (val.isBool()) return val.asBool() ? "true" : "false";
    if (val.isInt()) return std::to_string(val.asInt());
    if (val.isFloat()) {
        std::ostringstream oss;
        oss << val.asFloat();
        return oss.str();
    }
    if (val.isString()) return std::string(val.asString());
    return "";
}

}  // anonymous namespace

void registerConfigBuiltins(ScriptEngine& engine) {
    // config_parse(string) → map
    engine.registerFunction("config_parse",
        [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("config_parse: expected string argument");
            }

            const auto& input = args[0].asString();
            auto result = Value::map();
            auto& map = result.asMap();

            std::istringstream stream(input);
            std::string line;

            while (std::getline(stream, line)) {
                auto trimmed = trim(line);
                if (trimmed.empty() || trimmed.front() == '#') continue;

                // Find the colon separator
                auto colonPos = trimmed.find(':');
                if (colonPos == std::string_view::npos) continue;

                auto key = trim(trimmed.substr(0, colonPos));
                auto val = trim(trimmed.substr(colonPos + 1));

                if (key.empty()) continue;

                uint32_t keySym = ctx.engine().intern(key);
                map.set(keySym, parseConfigValue(val));
            }

            return result;
        });

    // config_encode(map) → string
    engine.registerFunction("config_encode",
        [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isMap()) {
                throw std::runtime_error("config_encode: expected map argument");
            }

            const auto& map = args[0].asMap();
            auto keys = map.keys();

            // Sort keys alphabetically by their string names
            std::sort(keys.begin(), keys.end(),
                [&ctx](uint32_t a, uint32_t b) {
                    return ctx.engine().lookupSymbol(a) < ctx.engine().lookupSymbol(b);
                });

            std::string output;
            for (auto key : keys) {
                auto keyName = ctx.engine().lookupSymbol(key);
                auto val = map.get(key);

                // Skip non-primitive values (closures, arrays, nested maps)
                if (val.isNil() || val.isCallable() || val.isArray() || val.isMap()) continue;

                output += keyName;
                output += ": ";
                output += formatConfigValue(val);
                output += '\n';
            }

            return Value::string(std::move(output));
        });
}

}  // namespace finescript
