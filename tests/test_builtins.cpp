#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"
#include "finescript/value.h"
#include "finescript/map_data.h"
#include <cmath>

using namespace finescript;

static FullScriptResult run(ScriptEngine& engine, ExecutionContext& ctx,
                            std::string_view code) {
    return engine.executeCommand(code, ctx);
}

// ============================================================
// Math builtins
// ============================================================

TEST_CASE("Builtins: abs integer", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "abs -5");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 5);
}

TEST_CASE("Builtins: abs float", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "abs -3.14");
    CHECK(r.success);
    CHECK(r.returnValue.asFloat() == Catch::Approx(3.14));
}

TEST_CASE("Builtins: min integer", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "min 10 3");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 3);
}

TEST_CASE("Builtins: max integer", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "max 10 3");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 10);
}

TEST_CASE("Builtins: min with float promotion", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "min 2.5 3");
    CHECK(r.success);
    CHECK(r.returnValue.asFloat() == Catch::Approx(2.5));
}

TEST_CASE("Builtins: floor", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "floor 3.7");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 3);
}

TEST_CASE("Builtins: ceil", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "ceil 3.2");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 4);
}

TEST_CASE("Builtins: round", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "round 3.5");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 4);
}

TEST_CASE("Builtins: floor/ceil on int is identity", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "floor 5").returnValue.asInt() == 5);
    CHECK(run(engine, ctx, "ceil 5").returnValue.asInt() == 5);
}

TEST_CASE("Builtins: sqrt", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "sqrt 16");
    CHECK(r.success);
    CHECK(r.returnValue.asFloat() == Catch::Approx(4.0));
}

TEST_CASE("Builtins: pow integer", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "pow 2 10");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 1024);
}

TEST_CASE("Builtins: pow with float", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "pow 2.0 0.5");
    CHECK(r.success);
    CHECK(r.returnValue.asFloat() == Catch::Approx(std::sqrt(2.0)));
}

TEST_CASE("Builtins: sin cos tan", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "sin 0").returnValue.asFloat() == Catch::Approx(0.0));
    CHECK(run(engine, ctx, "cos 0").returnValue.asFloat() == Catch::Approx(1.0));
    CHECK(run(engine, ctx, "tan 0").returnValue.asFloat() == Catch::Approx(0.0));
}

TEST_CASE("Builtins: random_range returns in range", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    for (int i = 0; i < 20; ++i) {
        auto r = run(engine, ctx, "random_range 1 10");
        CHECK(r.success);
        CHECK(r.returnValue.asInt() >= 1);
        CHECK(r.returnValue.asInt() <= 10);
    }
}

TEST_CASE("Builtins: random_float returns in [0,1)", "[builtins][math]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    for (int i = 0; i < 20; ++i) {
        auto r = run(engine, ctx, "random_float");
        CHECK(r.success);
        CHECK(r.returnValue.asFloat() >= 0.0);
        CHECK(r.returnValue.asFloat() < 1.0);
    }
}

// ============================================================
// Comparison builtins
// ============================================================

TEST_CASE("Builtins: eq", "[builtins][comparison]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "eq 5 5").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "eq 5 6").returnValue.asBool() == false);
    CHECK(run(engine, ctx, "eq \"hello\" \"hello\"").returnValue.asBool() == true);
}

TEST_CASE("Builtins: ne", "[builtins][comparison]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "ne 5 6").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "ne 5 5").returnValue.asBool() == false);
}

TEST_CASE("Builtins: lt gt le ge", "[builtins][comparison]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "lt 3 5").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "lt 5 3").returnValue.asBool() == false);
    CHECK(run(engine, ctx, "gt 5 3").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "le 5 5").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "le 6 5").returnValue.asBool() == false);
    CHECK(run(engine, ctx, "ge 5 5").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "ge 4 5").returnValue.asBool() == false);
}

TEST_CASE("Builtins: comparison with float", "[builtins][comparison]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "lt 2.5 3").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "gt 3.5 3").returnValue.asBool() == true);
}

// ============================================================
// String builtins
// ============================================================

TEST_CASE("Builtins: str_length", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "str_length \"hello\"");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 5);
}

TEST_CASE("Builtins: str_length empty", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "str_length \"\"").returnValue.asInt() == 0);
}

TEST_CASE("Builtins: str_concat", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "str_concat \"hello\" \" \" \"world\"");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "hello world");
}

TEST_CASE("Builtins: str_concat mixed types", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "str_concat \"x=\" 42");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "x=42");
}

TEST_CASE("Builtins: str_substr", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "str_substr \"hello world\" 6");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "world");
}

TEST_CASE("Builtins: str_substr with length", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "str_substr \"hello world\" 0 5");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "hello");
}

TEST_CASE("Builtins: str_find", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "str_find \"hello world\" \"world\"").returnValue.asInt() == 6);
    CHECK(run(engine, ctx, "str_find \"hello world\" \"xyz\"").returnValue.asInt() == -1);
}

TEST_CASE("Builtins: str_upper", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "str_upper \"hello\"").returnValue.asString() == "HELLO");
}

TEST_CASE("Builtins: str_lower", "[builtins][string]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "str_lower \"HELLO\"").returnValue.asString() == "hello");
}

// ============================================================
// Type conversion builtins
// ============================================================

TEST_CASE("Builtins: to_int from float", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_int 3.7").returnValue.asInt() == 3);
}

TEST_CASE("Builtins: to_int from string", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_int \"42\"").returnValue.asInt() == 42);
}

TEST_CASE("Builtins: to_int from bool", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_int true").returnValue.asInt() == 1);
    CHECK(run(engine, ctx, "to_int false").returnValue.asInt() == 0);
}

TEST_CASE("Builtins: to_int from bad string", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_int \"abc\"").returnValue.isNil());
}

TEST_CASE("Builtins: to_float from int", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "to_float 5");
    CHECK(r.returnValue.isFloat());
    CHECK(r.returnValue.asFloat() == 5.0);
}

TEST_CASE("Builtins: to_float from string", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_float \"3.14\"").returnValue.asFloat() == Catch::Approx(3.14));
}

TEST_CASE("Builtins: to_str", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_str 42").returnValue.asString() == "42");
    CHECK(run(engine, ctx, "to_str true").returnValue.asString() == "true");
}

TEST_CASE("Builtins: to_bool", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "to_bool 1").returnValue.asBool() == true);
    CHECK(run(engine, ctx, "to_bool 0").returnValue.asBool() == true);  // 0 is truthy in finescript
    CHECK(run(engine, ctx, "to_bool nil").returnValue.asBool() == false);
    CHECK(run(engine, ctx, "to_bool false").returnValue.asBool() == false);
    CHECK(run(engine, ctx, "to_bool \"hello\"").returnValue.asBool() == true);
}

TEST_CASE("Builtins: type", "[builtins][type]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    CHECK(run(engine, ctx, "type 42").returnValue.asString() == "int");
    CHECK(run(engine, ctx, "type 3.14").returnValue.asString() == "float");
    CHECK(run(engine, ctx, "type \"hello\"").returnValue.asString() == "string");
    CHECK(run(engine, ctx, "type true").returnValue.asString() == "bool");
    CHECK(run(engine, ctx, "type nil").returnValue.asString() == "nil");
}

// ============================================================
// I/O builtins
// ============================================================

TEST_CASE("Builtins: print returns nil", "[builtins][io]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "print \"hello\" \"world\"");
    CHECK(r.success);
    CHECK(r.returnValue.isNil());
}

// ============================================================
// Map constructor
// ============================================================

TEST_CASE("Builtins: map constructor", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto r = run(engine, ctx, "set m {map :name \"Alice\" :age 30}\nm.name");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "Alice");
}

TEST_CASE("Builtins: map constructor empty", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto r = run(engine, ctx, "set m {map}\ntype m");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "map");
}

TEST_CASE("Builtins: map field access", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set m {map :x 10 :y 20}");
    CHECK(run(engine, ctx, "m.x").returnValue.asInt() == 10);
    CHECK(run(engine, ctx, "m.y").returnValue.asInt() == 20);
}

TEST_CASE("Builtins: map constructor with named args", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto r = run(engine, ctx, "set m {map =name \"Alice\" =age 30}\nm.name");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "Alice");
}

TEST_CASE("Builtins: map constructor named args field access", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set m {map =x 10 =y 20}");
    CHECK(run(engine, ctx, "m.x").returnValue.asInt() == 10);
    CHECK(run(engine, ctx, "m.y").returnValue.asInt() == 20);
}

TEST_CASE("Builtins: map constructor mixed positional and named args", "[builtins][map]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set m {map :a 1 =b 2}");
    CHECK(run(engine, ctx, "m.a").returnValue.asInt() == 1);
    CHECK(run(engine, ctx, "m.b").returnValue.asInt() == 2);
}

// ============================================================
// End-to-end with builtins
// ============================================================

TEST_CASE("Builtins: combined math and type", "[builtins][e2e]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto r = run(engine, ctx, R"(
set x 3.7
set y {floor x}
set z {to_str y}
str_concat "result=" z
)");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "result=3");
}

TEST_CASE("Builtins: math in expressions", "[builtins][e2e]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto r = run(engine, ctx, "({abs -5} + {min 3 7})");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 8);
}

// ============================================================
// Format builtin
// ============================================================

TEST_CASE("Builtins: format basic two ints", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"%d/%d\" 10 20");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "10/20");
}

TEST_CASE("Builtins: format mixed types", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"%s: %d\" \"HP\" 100");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "HP: 100");
}

TEST_CASE("Builtins: format float precision", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"%.2f x %.2f\" 1.5 2.75");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "1.50 x 2.75");
}

TEST_CASE("Builtins: format with literal text", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"(%d, %d)\" 10 20");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "(10, 20)");
}

TEST_CASE("Builtins: format single arg", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"%04x\" 255");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "00ff");
}

TEST_CASE("Builtins: format escaped percent", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    auto r = run(engine, ctx, "format \"%d%%\" 42");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "42%");
}

TEST_CASE("Builtins: format in string interpolation", "[builtins][format]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    run(engine, ctx, "set hp 50");
    run(engine, ctx, "set max_hp 100");
    auto r = run(engine, ctx, "\"Health: {format \"%d/%d\" hp max_hp}\"");
    CHECK(r.success);
    CHECK(r.returnValue.asString() == "Health: 50/100");
}
