#include <catch2/catch_test_macros.hpp>
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"
#include "finescript/interner.h"
#include "finescript/error.h"
#include "finescript/map_data.h"
#include <fstream>
#include <filesystem>

using namespace finescript;

// Helper: execute a command string and return the result
static FullScriptResult run(ScriptEngine& engine, ExecutionContext& ctx,
                            std::string_view code) {
    return engine.executeCommand(code, ctx);
}

// === Basic pipeline ===

TEST_CASE("Integration: parse and execute simple expression", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "(1 + 2)");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 3);
}

TEST_CASE("Integration: multi-statement script", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "set x 10\nset y 20\n(x + y)");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 30);
}

TEST_CASE("Integration: function definition and call", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "fn double [x] (x * 2)");
    auto result = run(engine, ctx, "double 21");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 42);
}

// === Native function registration ===

TEST_CASE("Integration: register and call native function", "[integration]") {
    ScriptEngine engine;
    engine.registerFunction("add_native",
        [](ExecutionContext&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) return Value::nil();
            return Value::integer(args[0].asInt() + args[1].asInt());
        });

    ExecutionContext ctx(engine);
    auto result = run(engine, ctx, "add_native 10 20");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 30);
}

TEST_CASE("Integration: native function with context access", "[integration]") {
    ScriptEngine engine;
    engine.registerFunction("get_player_name",
        [](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
            return ctx.get("player_name");
        });

    ExecutionContext ctx(engine);
    ctx.set("player_name", Value::string("Alice"));
    auto result = run(engine, ctx, "get_player_name");
    CHECK(result.success);
    CHECK(result.returnValue.asString() == "Alice");
}

// === Constant registration ===

TEST_CASE("Integration: register constant", "[integration]") {
    ScriptEngine engine;
    engine.registerConstant("MAX_HEALTH", Value::integer(100));

    ExecutionContext ctx(engine);
    auto result = run(engine, ctx, "MAX_HEALTH");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 100);
}

// === Context pre-binding ===

TEST_CASE("Integration: context variables", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);
    ctx.set("block_type", Value::string("stone"));
    ctx.set("block_x", Value::integer(10));

    auto result = run(engine, ctx, "block_type");
    CHECK(result.success);
    CHECK(result.returnValue.asString() == "stone");

    result = run(engine, ctx, "block_x");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 10);
}

TEST_CASE("Integration: context isolation", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx1(engine);
    ExecutionContext ctx2(engine);

    ctx1.set("x", Value::integer(1));
    ctx2.set("x", Value::integer(2));

    auto r1 = run(engine, ctx1, "x");
    auto r2 = run(engine, ctx2, "x");
    CHECK(r1.returnValue.asInt() == 1);
    CHECK(r2.returnValue.asInt() == 2);
}

// === Event handler collection ===

TEST_CASE("Integration: on event handler collection", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "on :interact do\n  42\nend");
    run(engine, ctx, "on :destroy do\n  0\nend");

    auto& handlers = ctx.eventHandlers();
    REQUIRE(handlers.size() == 2);
    CHECK(engine.lookupSymbol(handlers[0].eventSymbol) == "interact");
    CHECK(engine.lookupSymbol(handlers[1].eventSymbol) == "destroy");
    CHECK(handlers[0].handlerFunction.isClosure());
    CHECK(handlers[1].handlerFunction.isClosure());
}

// === Compiled script execution ===

TEST_CASE("Integration: parseString and execute", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto script = engine.parseString("(5 * 5)", "test_script");
    auto result = engine.execute(*script, ctx);
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 25);
}

TEST_CASE("Integration: compiled script reuse", "[integration]") {
    ScriptEngine engine;
    auto script = engine.parseString("(x * x)", "square_test");

    ExecutionContext ctx1(engine);
    ctx1.set("x", Value::integer(5));
    auto r1 = engine.execute(*script, ctx1);
    CHECK(r1.returnValue.asInt() == 25);

    ExecutionContext ctx2(engine);
    ctx2.set("x", Value::integer(7));
    auto r2 = engine.execute(*script, ctx2);
    CHECK(r2.returnValue.asInt() == 49);
}

// === Closures survive script execution ===

TEST_CASE("Integration: closure survives across commands", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, R"(
fn makeCounter [] do
    set count 0
    fn [] do
        set count (count + 1)
        count
    end
end
)");
    run(engine, ctx, "set counter {makeCounter}");

    auto r1 = run(engine, ctx, "counter");
    CHECK(r1.success);
    CHECK(r1.returnValue.asInt() == 1);

    auto r2 = run(engine, ctx, "counter");
    CHECK(r2.success);
    CHECK(r2.returnValue.asInt() == 2);
}

// === Error reporting ===

TEST_CASE("Integration: runtime error reporting", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "(1 / 0)");
    CHECK_FALSE(result.success);
    CHECK(result.error.find("Division by zero") != std::string::npos);
}

TEST_CASE("Integration: parse error reporting", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "(1 +)");
    CHECK_FALSE(result.success);
}

TEST_CASE("Integration: top-level return", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "return 42");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 42);
}

// === executeCommand ===

TEST_CASE("Integration: executeCommand with variables", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    engine.executeCommand("set greeting \"hello\"", ctx);
    auto result = engine.executeCommand("greeting", ctx);
    CHECK(result.success);
    CHECK(result.returnValue.asString() == "hello");
}

// === Source (file loading) ===

TEST_CASE("Integration: source loads and executes file", "[integration]") {
    // Write a temp script file
    auto tmpDir = std::filesystem::temp_directory_path();
    auto tmpFile = tmpDir / "test_source.script";
    {
        std::ofstream out(tmpFile);
        out << "set sourced_var 42\n";
    }

    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, "source \"" + tmpFile.string() + "\"");
    CHECK(result.success);

    // The sourced file should have set the variable in the current scope
    result = run(engine, ctx, "sourced_var");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 42);

    std::filesystem::remove(tmpFile);
}

TEST_CASE("Integration: source with function definition", "[integration]") {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto tmpFile = tmpDir / "test_source_fn.script";
    {
        std::ofstream out(tmpFile);
        out << "fn sourced_add [a b] (a + b)\n";
    }

    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "source \"" + tmpFile.string() + "\"");
    auto result = run(engine, ctx, "sourced_add 3 4");
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 7);

    std::filesystem::remove(tmpFile);
}

// === AST caching ===

TEST_CASE("Integration: loadScript caches by path", "[integration]") {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto tmpFile = tmpDir / "test_cache.script";
    {
        std::ofstream out(tmpFile);
        out << "42\n";
    }

    ScriptEngine engine;

    auto* first = engine.loadScript(tmpFile);
    auto* second = engine.loadScript(tmpFile);
    CHECK(first == second); // same pointer = cached

    std::filesystem::remove(tmpFile);
}

TEST_CASE("Integration: cache invalidation", "[integration]") {
    auto tmpDir = std::filesystem::temp_directory_path();
    auto tmpFile = tmpDir / "test_invalidate.script";
    {
        std::ofstream out(tmpFile);
        out << "42\n";
    }

    ScriptEngine engine;
    auto* first = engine.loadScript(tmpFile);
    // Without invalidation, same pointer
    auto* cached = engine.loadScript(tmpFile);
    CHECK(first == cached);

    // After invalidation, loadScript must re-parse (verify it works)
    engine.invalidateCache(tmpFile);

    // Rewrite file with different content
    {
        std::ofstream out(tmpFile);
        out << "99\n";
    }

    auto* second = engine.loadScript(tmpFile);
    // Verify the new content is used
    ExecutionContext ctx(engine);
    auto result = engine.execute(*second, ctx);
    CHECK(result.success);
    CHECK(result.returnValue.asInt() == 99);

    std::filesystem::remove(tmpFile);
}

// === Interner API ===

TEST_CASE("Integration: intern and lookupSymbol", "[integration]") {
    ScriptEngine engine;

    uint32_t id = engine.intern("test_symbol");
    CHECK(engine.lookupSymbol(id) == "test_symbol");

    // Same string gives same ID
    uint32_t id2 = engine.intern("test_symbol");
    CHECK(id == id2);
}

TEST_CASE("Integration: custom interner", "[integration]") {
    ScriptEngine engine;
    DefaultInterner customInterner;
    engine.setInterner(&customInterner);

    uint32_t id = engine.intern("custom_test");
    CHECK(engine.lookupSymbol(id) == "custom_test");
    CHECK(customInterner.lookup(id) == "custom_test");
}

// === Complex end-to-end ===

TEST_CASE("Integration: complex script with all features", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    auto result = run(engine, ctx, R"(
fn fibonacci [n] do
    if (n <= 1) {return n}
    ({fibonacci (n - 1)} + {fibonacci (n - 2)})
end

fn sumArray [arr] do
    set total 0
    for item in arr do
        set total (total + item)
    end
    total
end

set fibs [0 1 1 2 3 5 8]
set sum {sumArray fibs}
set fib10 {fibonacci 10}

(sum + fib10)
)");
    CHECK(result.success);
    // sum of [0,1,1,2,3,5,8] = 20, fib(10) = 55 → 75
    CHECK(result.returnValue.asInt() == 75);
}

TEST_CASE("Integration: string interpolation end-to-end", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set name \"world\"");
    run(engine, ctx, "set x 42");
    auto result = run(engine, ctx, "\"Hello {name}, x={x}\"");
    CHECK(result.success);
    CHECK(result.returnValue.asString() == "Hello world, x=42");
}

TEST_CASE("Integration: map operations end-to-end", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    // Create map via C++ and manipulate from script
    auto mapData = std::make_shared<MapData>();
    uint32_t nameSym = engine.intern("name");
    mapData->set(nameSym, Value::string("test_obj"));

    // Bind it in context
    ctx.set("obj", Value::map(mapData));

    auto result = run(engine, ctx, "obj.name");
    CHECK(result.success);
    CHECK(result.returnValue.asString() == "test_obj");

    run(engine, ctx, "set obj.name \"updated\"");
    result = run(engine, ctx, "obj.name");
    CHECK(result.returnValue.asString() == "updated");
}

TEST_CASE("Integration: match expression end-to-end", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, R"(
fn describe [x] do
    match x
        1 "one"
        2 "two"
        _ "other"
    end
end
)");
    auto r1 = run(engine, ctx, "describe 1");
    CHECK(r1.returnValue.asString() == "one");
    auto r2 = run(engine, ctx, "describe 2");
    CHECK(r2.returnValue.asString() == "two");
    auto r3 = run(engine, ctx, "describe 99");
    CHECK(r3.returnValue.asString() == "other");
}

TEST_CASE("Integration: user data on context", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    int myData = 42;
    ctx.setUserData(&myData);
    CHECK(ctx.userData() == &myData);
    CHECK(*static_cast<int*>(ctx.userData()) == 42);
}

// === callFunction ===

TEST_CASE("Integration: callFunction with script closure", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    // Define a function in script, get its value via ~
    run(engine, ctx, "fn double [x] (x * 2)");

    auto r = run(engine, ctx, "~double");
    REQUIRE(r.success);
    REQUIRE(r.returnValue.isClosure());

    // Call the closure from C++
    Value result = engine.callFunction(r.returnValue, {Value::integer(21)}, ctx);
    CHECK(result.asInt() == 42);
}

TEST_CASE("Integration: callFunction with native function", "[integration]") {
    ScriptEngine engine;
    engine.registerFunction("add_native",
        [](ExecutionContext&, const std::vector<Value>& args) -> Value {
            return Value::integer(args[0].asInt() + args[1].asInt());
        });

    ExecutionContext ctx(engine);

    // Get the native function value
    auto r = run(engine, ctx, "~add_native");
    REQUIRE(r.success);
    REQUIRE(r.returnValue.isNativeFunction());

    // Call it from C++
    Value result = engine.callFunction(r.returnValue,
        {Value::integer(10), Value::integer(32)}, ctx);
    CHECK(result.asInt() == 42);
}

TEST_CASE("Integration: callFunction with closure capturing state", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    // Create a counter closure
    run(engine, ctx, R"(
fn makeCounter [] do
    set n 0
    fn [] do
        set n (n + 1)
        n
    end
end
set counter {makeCounter}
)");

    auto r = run(engine, ctx, "~counter");
    REQUIRE(r.success);
    REQUIRE(r.returnValue.isClosure());

    // Call it repeatedly from C++ — state should persist
    Value v1 = engine.callFunction(r.returnValue, {}, ctx);
    Value v2 = engine.callFunction(r.returnValue, {}, ctx);
    Value v3 = engine.callFunction(r.returnValue, {}, ctx);
    CHECK(v1.asInt() == 1);
    CHECK(v2.asInt() == 2);
    CHECK(v3.asInt() == 3);
}

TEST_CASE("Integration: callFunction non-callable throws", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    CHECK_THROWS_AS(
        engine.callFunction(Value::integer(5), {}, ctx),
        std::runtime_error
    );
}

TEST_CASE("Integration: callFunction with method closure", "[integration]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    // Build an object with a method, extract the method closure
    run(engine, ctx, R"(
set obj {map :hp 100}
obj.setMethod :damage fn [self amount] do
    set self.hp (self.hp - amount)
    self.hp
end
)");

    // Get the raw method function (not via dot-call, so no self injection)
    auto r = run(engine, ctx, "{obj.get :damage}");
    REQUIRE(r.success);
    REQUIRE(r.returnValue.isClosure());

    // Call it from C++, manually passing self
    auto objVal = run(engine, ctx, "~obj");
    Value result = engine.callFunction(r.returnValue,
        {objVal.returnValue, Value::integer(30)}, ctx);
    CHECK(result.asInt() == 70);

    // Verify the mutation persisted
    auto hp = run(engine, ctx, "obj.hp");
    CHECK(hp.returnValue.asInt() == 70);
}

// === global scope access ===

TEST_CASE("Integration: global read from inner scope", "[integration][global]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set x 42");
    // Read x through global proxy from inside a function
    auto r = run(engine, ctx, R"(
        fn f [] global.x
        {f}
    )");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 42);
}

TEST_CASE("Integration: global write creates in global scope", "[integration][global]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    // Create a new variable in global scope from inside a function
    auto r = run(engine, ctx, R"(
        fn f [] do
            set global.newvar 99
        end
        {f}
    )");
    CHECK(r.success);
    // The variable should be accessible from the top level
    auto r2 = run(engine, ctx, "global.newvar");
    CHECK(r2.success);
    CHECK(r2.returnValue.asInt() == 99);
}

TEST_CASE("Integration: global disambiguates shadowed variable", "[integration][global]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set x 100");
    auto r = run(engine, ctx, R"(
        fn f [] do
            let x 5
            global.x
        end
        {f}
    )");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 100);
}

TEST_CASE("Integration: set global.x updates global from function", "[integration][global]") {
    ScriptEngine engine;
    ExecutionContext ctx(engine);

    run(engine, ctx, "set counter 0");
    run(engine, ctx, R"(
        fn increment [] do
            set global.counter (global.counter + 1)
        end
    )");
    run(engine, ctx, "{increment}");
    run(engine, ctx, "{increment}");
    run(engine, ctx, "{increment}");
    auto r = run(engine, ctx, "counter");
    CHECK(r.success);
    CHECK(r.returnValue.asInt() == 3);
}
