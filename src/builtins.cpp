#include "finescript/builtins.h"
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"
#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/interner.h"
#include "finescript/format_util.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <cctype>

namespace finescript {

// ---- Helpers ----

static std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

// ---- Math builtins ----

void registerMathBuiltins(ScriptEngine& engine) {
    engine.registerFunction("abs", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        if (args[0].isInt()) return Value::integer(std::abs(args[0].asInt()));
        if (args[0].isFloat()) return Value::number(std::abs(args[0].asFloat()));
        return Value::nil();
    });

    engine.registerFunction("min", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return args.empty() ? Value::nil() : args[0];
        bool anyFloat = args[0].isFloat() || args[1].isFloat();
        if (anyFloat) {
            return Value::number(std::min(args[0].asNumber(), args[1].asNumber()));
        }
        return Value::integer(std::min(args[0].asInt(), args[1].asInt()));
    });

    engine.registerFunction("max", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return args.empty() ? Value::nil() : args[0];
        bool anyFloat = args[0].isFloat() || args[1].isFloat();
        if (anyFloat) {
            return Value::number(std::max(args[0].asNumber(), args[1].asNumber()));
        }
        return Value::integer(std::max(args[0].asInt(), args[1].asInt()));
    });

    engine.registerFunction("floor", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        if (args[0].isInt()) return args[0];
        return Value::integer(static_cast<int64_t>(std::floor(args[0].asNumber())));
    });

    engine.registerFunction("ceil", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        if (args[0].isInt()) return args[0];
        return Value::integer(static_cast<int64_t>(std::ceil(args[0].asNumber())));
    });

    engine.registerFunction("round", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        if (args[0].isInt()) return args[0];
        return Value::integer(static_cast<int64_t>(std::round(args[0].asNumber())));
    });

    engine.registerFunction("sqrt", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        return Value::number(std::sqrt(args[0].asNumber()));
    });

    engine.registerFunction("pow", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        bool anyFloat = args[0].isFloat() || args[1].isFloat();
        double result = std::pow(args[0].asNumber(), args[1].asNumber());
        if (!anyFloat && result == std::floor(result) &&
            result >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
            result <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
            return Value::integer(static_cast<int64_t>(result));
        }
        return Value::number(result);
    });

    engine.registerFunction("sin", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        return Value::number(std::sin(args[0].asNumber()));
    });

    engine.registerFunction("cos", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        return Value::number(std::cos(args[0].asNumber()));
    });

    engine.registerFunction("tan", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        return Value::number(std::tan(args[0].asNumber()));
    });

    engine.registerFunction("random", [](ExecutionContext&, const std::vector<Value>&) -> Value {
        return Value::integer(static_cast<int64_t>(rng()()));
    });

    engine.registerFunction("random_range", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        int64_t lo = args[0].asInt();
        int64_t hi = args[1].asInt();
        std::uniform_int_distribution<int64_t> dist(lo, hi);
        return Value::integer(dist(rng()));
    });

    engine.registerFunction("random_float", [](ExecutionContext&, const std::vector<Value>&) -> Value {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value::number(dist(rng()));
    });
}

// ---- Comparison builtins ----

void registerComparisonBuiltins(ScriptEngine& engine) {
    engine.registerFunction("eq", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(false);
        return Value::boolean(args[0] == args[1]);
    });

    engine.registerFunction("ne", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(true);
        return Value::boolean(args[0] != args[1]);
    });

    engine.registerFunction("lt", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(false);
        return Value::boolean(args[0].asNumber() < args[1].asNumber());
    });

    engine.registerFunction("gt", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(false);
        return Value::boolean(args[0].asNumber() > args[1].asNumber());
    });

    engine.registerFunction("le", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(false);
        return Value::boolean(args[0].asNumber() <= args[1].asNumber());
    });

    engine.registerFunction("ge", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::boolean(false);
        return Value::boolean(args[0].asNumber() >= args[1].asNumber());
    });
}

// ---- String builtins ----

void registerStringBuiltins(ScriptEngine& engine) {
    engine.registerFunction("str_length", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) return Value::integer(0);
        return Value::integer(static_cast<int64_t>(args[0].asString().size()));
    });

    engine.registerFunction("str_concat", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        std::string result;
        for (auto& a : args) {
            result += a.toString();
        }
        return Value::string(std::move(result));
    });

    engine.registerFunction("str_substr", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString()) return Value::nil();
        const auto& s = args[0].asString();
        auto start = static_cast<size_t>(args[1].asInt());
        if (start >= s.size()) return Value::string("");
        if (args.size() >= 3) {
            auto len = static_cast<size_t>(args[2].asInt());
            return Value::string(std::string(s.substr(start, len)));
        }
        return Value::string(std::string(s.substr(start)));
    });

    engine.registerFunction("str_find", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString() || !args[1].isString()) return Value::integer(-1);
        auto pos = args[0].asString().find(args[1].asString());
        if (pos == std::string::npos) return Value::integer(-1);
        return Value::integer(static_cast<int64_t>(pos));
    });

    engine.registerFunction("str_upper", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) return Value::nil();
        std::string result = args[0].asString();
        for (auto& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Value::string(std::move(result));
    });

    engine.registerFunction("str_lower", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) return Value::nil();
        std::string result = args[0].asString();
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value::string(std::move(result));
    });

    // format "fmt" arg1 arg2 ... — multi-arg printf-style formatting
    engine.registerFunction("format", [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) return Value::nil();
        const auto& fmt = args[0].asString();
        std::vector<Value> fmtArgs(args.begin() + 1, args.end());
        return Value::string(formatMulti(fmt, fmtArgs, &ctx.engine().interner()));
    });
}

// ---- Type conversion builtins ----

void registerTypeBuiltins(ScriptEngine& engine) {
    engine.registerFunction("to_int", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto& v = args[0];
        if (v.isInt()) return v;
        if (v.isFloat()) return Value::integer(static_cast<int64_t>(v.asFloat()));
        if (v.isBool()) return Value::integer(v.asBool() ? 1 : 0);
        if (v.isString()) {
            try { return Value::integer(std::stoll(v.asString())); }
            catch (...) { return Value::nil(); }
        }
        return Value::nil();
    });

    engine.registerFunction("to_float", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto& v = args[0];
        if (v.isFloat()) return v;
        if (v.isInt()) return Value::number(static_cast<double>(v.asInt()));
        if (v.isBool()) return Value::number(v.asBool() ? 1.0 : 0.0);
        if (v.isString()) {
            try { return Value::number(std::stod(v.asString())); }
            catch (...) { return Value::nil(); }
        }
        return Value::nil();
    });

    engine.registerFunction("to_str", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::string("");
        return Value::string(args[0].toString());
    });

    engine.registerFunction("to_bool", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::boolean(false);
        return Value::boolean(args[0].truthy());
    });

    engine.registerFunction("type", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::string("nil");
        return Value::string(args[0].typeName());
    });
}

// ---- I/O builtins ----

void registerIOBuiltins(ScriptEngine& engine) {
    engine.registerFunction("print", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << args[i].toString();
        }
        std::cout << '\n';
        return Value::nil();
    });
}

// ---- Map constructor ----

static void registerMapConstructor(ScriptEngine& engine) {
    engine.registerFunction("map", [](ExecutionContext&, const std::vector<Value>& args) -> Value {
        auto mapData = std::make_shared<MapData>();
        // Positional pairs: :key1 val1 :key2 val2 ...
        size_t end = args.size();
        // If last arg is a map (kwargs from named args), pull its entries
        if (!args.empty() && args.back().isMap()) {
            auto& kwargsMap = const_cast<Value&>(args.back()).asMap();
            for (auto key : kwargsMap.keys()) {
                mapData->set(key, kwargsMap.get(key));
            }
            end--;  // don't process kwargs map as positional pair
        }
        for (size_t i = 0; i + 1 < end; i += 2) {
            if (!args[i].isSymbol()) continue;
            mapData->set(args[i].asSymbol(), args[i + 1]);
        }
        return Value::map(std::move(mapData));
    });
}

// ---- Master registration ----

void registerBuiltins(ScriptEngine& engine) {
    registerMathBuiltins(engine);
    registerComparisonBuiltins(engine);
    registerStringBuiltins(engine);
    registerTypeBuiltins(engine);
    registerIOBuiltins(engine);
    registerMapConstructor(engine);
}

} // namespace finescript
