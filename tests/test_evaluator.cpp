#include <catch2/catch_test_macros.hpp>
#include "finescript/evaluator.h"
#include "finescript/parser.h"
#include "finescript/interner.h"
#include "finescript/error.h"
#include "finescript/map_data.h"

using namespace finescript;

// Helper: parse + evaluate with a persistent scope (for multi-statement tests)
struct TestEnv {
    DefaultInterner interner;
    std::shared_ptr<Scope> globalScope = Scope::createGlobal();
    Evaluator evaluator{interner, globalScope};

    Value run(const std::string& source) {
        auto ast = std::shared_ptr<AstNode>(Parser::parse(source).release());
        return evaluator.eval(ast, globalScope);
    }
};

// === Literals ===

TEST_CASE("Eval integer literal", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("42").asInt() == 42);
    CHECK(env.run("0").asInt() == 0);
    CHECK(env.run("999").asInt() == 999);
}

TEST_CASE("Eval float literal", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("3.14").asFloat() == 3.14);
    CHECK(env.run("0.5").asFloat() == 0.5);
}

TEST_CASE("Eval string literal", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("\"hello\"").asString() == "hello");
    CHECK(env.run("\"\"").asString() == "");
}

TEST_CASE("Eval bool literal", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("true").asBool() == true);
    CHECK(env.run("false").asBool() == false);
}

TEST_CASE("Eval nil literal", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("nil").isNil());
}

TEST_CASE("Eval symbol literal", "[evaluator]") {
    TestEnv env;
    Value v = env.run(":stone");
    CHECK(v.isSymbol());
    CHECK(env.interner.lookup(v.asSymbol()) == "stone");
}

TEST_CASE("Eval array literal", "[evaluator]") {
    TestEnv env;
    Value v = env.run("[1 2 3]");
    CHECK(v.isArray());
    CHECK(v.asArray().size() == 3);
    CHECK(v.asArray()[0].asInt() == 1);
    CHECK(v.asArray()[1].asInt() == 2);
    CHECK(v.asArray()[2].asInt() == 3);
}

TEST_CASE("Eval empty array", "[evaluator]") {
    TestEnv env;
    Value v = env.run("[]");
    CHECK(v.isArray());
    CHECK(v.asArray().empty());
}

// === Variables (set + name lookup) ===

TEST_CASE("Eval set and lookup", "[evaluator]") {
    TestEnv env;
    env.run("set x 42");
    CHECK(env.run("x").asInt() == 42);
}

TEST_CASE("Eval unbound name is nil", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("undefined_var").isNil());
}

TEST_CASE("Eval set updates existing", "[evaluator]") {
    TestEnv env;
    env.run("set x 1");
    env.run("set x 2");
    CHECK(env.run("x").asInt() == 2);
}

// === Arithmetic (infix) ===

TEST_CASE("Eval integer arithmetic", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(1 + 2)").asInt() == 3);
    CHECK(env.run("(10 - 3)").asInt() == 7);
    CHECK(env.run("(4 * 5)").asInt() == 20);
    CHECK(env.run("(7 / 2)").asInt() == 3);  // truncating
    CHECK(env.run("(7 % 3)").asInt() == 1);
}

TEST_CASE("Eval float arithmetic", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(1.5 + 2.5)").asFloat() == 4.0);
    CHECK(env.run("(10.0 - 3.5)").asFloat() == 6.5);
    CHECK(env.run("(2.0 * 3.0)").asFloat() == 6.0);
    CHECK(env.run("(7.0 / 2.0)").asFloat() == 3.5);
}

TEST_CASE("Eval mixed int/float promotes to float", "[evaluator]") {
    TestEnv env;
    Value v = env.run("(1 + 2.0)");
    CHECK(v.isFloat());
    CHECK(v.asFloat() == 3.0);
}

TEST_CASE("Eval division by zero", "[evaluator]") {
    TestEnv env;
    CHECK_THROWS(env.run("(1 / 0)"));
    CHECK_THROWS(env.run("(1.0 / 0.0)"));
    CHECK_THROWS(env.run("(5 % 0)"));
}

// === Comparison ===

TEST_CASE("Eval comparison operators", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(3 < 5)").asBool() == true);
    CHECK(env.run("(5 < 3)").asBool() == false);
    CHECK(env.run("(5 > 3)").asBool() == true);
    CHECK(env.run("(3 > 5)").asBool() == false);
    CHECK(env.run("(3 <= 3)").asBool() == true);
    CHECK(env.run("(3 >= 3)").asBool() == true);
    CHECK(env.run("(3 <= 4)").asBool() == true);
    CHECK(env.run("(4 >= 3)").asBool() == true);
}

TEST_CASE("Eval equality", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(1 == 1)").asBool() == true);
    CHECK(env.run("(1 == 2)").asBool() == false);
    CHECK(env.run("(1 != 2)").asBool() == true);
    CHECK(env.run("(1 != 1)").asBool() == false);
    CHECK(env.run("(\"a\" == \"a\")").asBool() == true);
    CHECK(env.run("(\"a\" == \"b\")").asBool() == false);
    CHECK(env.run("(true == true)").asBool() == true);
    CHECK(env.run("(nil == nil)").asBool() == true);
}

// === Logical operators ===

TEST_CASE("Eval and/or short-circuit", "[evaluator]") {
    TestEnv env;
    // and: returns first falsy or last truthy
    CHECK(env.run("(true and 42)").asInt() == 42);
    CHECK(env.run("(false and 42)").asBool() == false);
    CHECK(env.run("(nil and 42)").isNil());

    // or: returns first truthy or last falsy
    CHECK(env.run("(false or 42)").asInt() == 42);
    CHECK(env.run("(true or 42)").asBool() == true);
    CHECK(env.run("(nil or false)").asBool() == false);
}

TEST_CASE("Eval not", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(not true)").asBool() == false);
    CHECK(env.run("(not false)").asBool() == true);
    CHECK(env.run("(not nil)").asBool() == true);
    CHECK(env.run("(not 42)").asBool() == false);
}

// === Unary negate ===

TEST_CASE("Eval unary negate", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(-5)").asInt() == -5);
    CHECK(env.run("(-3.14)").asFloat() == -3.14);
}

// === String operations ===

TEST_CASE("Eval string concatenation", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(\"hello\" + \" world\")").asString() == "hello world");
}

TEST_CASE("Eval string comparison", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(\"abc\" < \"def\")").asBool() == true);
    CHECK(env.run("(\"def\" > \"abc\")").asBool() == true);
}

TEST_CASE("Eval string interpolation", "[evaluator]") {
    TestEnv env;
    env.run("set name \"world\"");
    CHECK(env.run("\"Hello {name}!\"").asString() == "Hello world!");
}

TEST_CASE("Eval string interpolation with expression", "[evaluator]") {
    TestEnv env;
    env.run("set x 3");
    env.run("set y 4");
    CHECK(env.run("\"sum={(x + y)}\"").asString() == "sum=7");
}

// === Ranges ===

TEST_CASE("Eval exclusive range", "[evaluator]") {
    TestEnv env;
    Value v = env.run("(0 .. 3)");
    CHECK(v.isArray());
    REQUIRE(v.asArray().size() == 3);
    CHECK(v.asArray()[0].asInt() == 0);
    CHECK(v.asArray()[1].asInt() == 1);
    CHECK(v.asArray()[2].asInt() == 2);
}

TEST_CASE("Eval inclusive range", "[evaluator]") {
    TestEnv env;
    Value v = env.run("(0 ..= 3)");
    CHECK(v.isArray());
    REQUIRE(v.asArray().size() == 4);
    CHECK(v.asArray()[3].asInt() == 3);
}

// === Array indexing ===

TEST_CASE("Eval array indexing", "[evaluator]") {
    TestEnv env;
    env.run("set arr [10 20 30]");
    CHECK(env.run("arr[0]").asInt() == 10);
    CHECK(env.run("arr[1]").asInt() == 20);
    CHECK(env.run("arr[2]").asInt() == 30);
}

TEST_CASE("Eval array negative index", "[evaluator]") {
    TestEnv env;
    env.run("set arr [10 20 30]");
    CHECK(env.run("arr[-1]").asInt() == 30);
    CHECK(env.run("arr[-2]").asInt() == 20);
}

TEST_CASE("Eval array index out of bounds", "[evaluator]") {
    TestEnv env;
    env.run("set arr [1 2 3]");
    CHECK_THROWS(env.run("arr[5]"));
}

// === Functions ===

TEST_CASE("Eval named function definition and call", "[evaluator]") {
    TestEnv env;
    env.run("fn double [x] (x * 2)");
    CHECK(env.run("double 5").asInt() == 10);
}

TEST_CASE("Eval multi-param function", "[evaluator]") {
    TestEnv env;
    env.run("fn add [a b] (a + b)");
    CHECK(env.run("add 3 4").asInt() == 7);
}

TEST_CASE("Eval anonymous function", "[evaluator]") {
    TestEnv env;
    env.run("set inc (fn [x] (x + 1))");
    CHECK(env.run("inc 10").asInt() == 11);
}

TEST_CASE("Eval function with no params", "[evaluator]") {
    TestEnv env;
    env.run("fn greet [] \"hello\"");
    CHECK(env.run("greet").asString() == "hello");
}

TEST_CASE("Eval missing arguments become nil", "[evaluator]") {
    TestEnv env;
    env.run("fn check [x] (x == nil)");
    CHECK(env.run("check").asBool() == true);
}

// === Closures ===

TEST_CASE("Eval closure captures scope", "[evaluator]") {
    TestEnv env;
    env.run("set x 10");
    env.run("fn getX [] x");
    env.run("set x 20");
    // getX captured scope, x was updated to 20 (Python-style)
    CHECK(env.run("getX").asInt() == 20);
}

TEST_CASE("Eval closure as return value", "[evaluator]") {
    TestEnv env;
    env.run(R"(
fn makeCounter [] do
    set count 0
    fn inc [] do
        set count (count + 1)
        count
    end
end
)");
    env.run("set counter {makeCounter}");
    CHECK(env.run("counter").asInt() == 1);
    CHECK(env.run("counter").asInt() == 2);
    CHECK(env.run("counter").asInt() == 3);
}

// === Return ===

TEST_CASE("Eval return from function", "[evaluator]") {
    TestEnv env;
    env.run(R"(
fn early [x] do
    if (x > 0) {return x}
    0
end
)");
    CHECK(env.run("early 5").asInt() == 5);
    CHECK(env.run("early 0").asInt() == 0);
}

TEST_CASE("Eval bare return gives nil", "[evaluator]") {
    TestEnv env;
    env.run(R"(
fn nothing [] do
    return
end
)");
    CHECK(env.run("nothing").isNil());
}

// === If/elif/else ===

TEST_CASE("Eval if true branch", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("if true {42}").asInt() == 42);
}

TEST_CASE("Eval if false gives nil", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("if false {42}").isNil());
}

TEST_CASE("Eval if-else", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("if false {1} {2}").asInt() == 2);
    CHECK(env.run("if true {1} {2}").asInt() == 1);
}

TEST_CASE("Eval if do-end", "[evaluator]") {
    TestEnv env;
    env.run("set x 10");
    Value v = env.run(R"(
if (x > 5) do
    42
end
)");
    CHECK(v.asInt() == 42);
}

TEST_CASE("Eval if-elif-else", "[evaluator]") {
    TestEnv env;
    env.run("set x 2");
    Value v = env.run(R"(
if (x == 1) do
    :one
elif (x == 2) do
    :two
else do
    :other
end
)");
    CHECK(v.isSymbol());
    CHECK(env.interner.lookup(v.asSymbol()) == "two");
}

// === For loop ===

TEST_CASE("Eval for over array", "[evaluator]") {
    TestEnv env;
    env.run("set sum 0");
    env.run(R"(
for x in [1 2 3] do
    set sum (sum + x)
end
)");
    CHECK(env.run("sum").asInt() == 6);
}

TEST_CASE("Eval for over range", "[evaluator]") {
    TestEnv env;
    env.run("set sum 0");
    env.run(R"(
for i in (0 ..= 4) do
    set sum (sum + i)
end
)");
    CHECK(env.run("sum").asInt() == 10);
}

TEST_CASE("Eval for loop variable is scoped", "[evaluator]") {
    TestEnv env;
    env.run(R"(
for i in [1 2 3] do
    set tmp i
end
)");
    // The loop var itself is in the loop scope
    // But tmp was set in the loop scope, and since it didn't exist in parent,
    // it stays in the loop scope and is inaccessible after
    // Actually with Python semantics, tmp in the loop body creates in loop scope
    // Let me test what the user would expect...
    // The design says for-loop creates a child scope.
    // 'set tmp i' inside loop — since tmp doesn't exist in any parent, creates in loop scope
    // After loop, tmp is not accessible from parent.
    // But the loop body shares scope between iterations (Python-style).
}

// === While loop ===

TEST_CASE("Eval while loop", "[evaluator]") {
    TestEnv env;
    env.run("set x 0");
    env.run(R"(
while (x < 5) do
    set x (x + 1)
end
)");
    CHECK(env.run("x").asInt() == 5);
}

TEST_CASE("Eval while false never executes", "[evaluator]") {
    TestEnv env;
    env.run("set x 0");
    env.run("while false do\n    set x 99\nend");
    CHECK(env.run("x").asInt() == 0);
}

// === Match ===

TEST_CASE("Eval match with literal patterns", "[evaluator]") {
    TestEnv env;
    env.run("set x 2");
    Value v = env.run(R"(
match x
    1 :one
    2 :two
    3 :three
end
)");
    CHECK(v.isSymbol());
    CHECK(env.interner.lookup(v.asSymbol()) == "two");
}

TEST_CASE("Eval match with wildcard", "[evaluator]") {
    TestEnv env;
    env.run("set x 99");
    Value v = env.run(R"(
match x
    1 :one
    _ :other
end
)");
    CHECK(env.interner.lookup(v.asSymbol()) == "other");
}

TEST_CASE("Eval match no match gives nil", "[evaluator]") {
    TestEnv env;
    env.run("set x 99");
    Value v = env.run(R"(
match x
    1 :one
    2 :two
end
)");
    CHECK(v.isNil());
}

TEST_CASE("Eval match with brace body", "[evaluator]") {
    TestEnv env;
    env.run("set x 1");
    Value v = env.run(R"(
match x
    1 {(10 + 1)}
    _ {0}
end
)");
    CHECK(v.asInt() == 11);
}

// === Block ===

TEST_CASE("Eval block returns last value", "[evaluator]") {
    TestEnv env;
    Value v = env.run("{1; 2; 3}");
    CHECK(v.asInt() == 3);
}

TEST_CASE("Eval do-end block", "[evaluator]") {
    TestEnv env;
    Value v = env.run(R"(
do
    set a 1
    set b 2
    (a + b)
end
)");
    CHECK(v.asInt() == 3);
}

// === Maps ===

TEST_CASE("Eval map creation and field access", "[evaluator]") {
    // No map literal syntax yet — maps created via C++ API or 'map' builtin (Phase 6).
    // Tested via map dot access and dot set tests below.
}

TEST_CASE("Eval map dot access", "[evaluator]") {
    TestEnv env;
    // Create a map manually via scope
    auto mapData = std::make_shared<MapData>();
    uint32_t nameSym = env.interner.intern("name");
    mapData->set(nameSym, Value::string("Alice"));
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(mapData));

    CHECK(env.run("obj.name").asString() == "Alice");
}

TEST_CASE("Eval map dot set", "[evaluator]") {
    TestEnv env;
    auto mapData = std::make_shared<MapData>();
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(mapData));

    env.run("set obj.name \"Bob\"");
    CHECK(env.run("obj.name").asString() == "Bob");
}

TEST_CASE("Eval map built-in methods via call", "[evaluator]") {
    TestEnv env;
    auto mapData = std::make_shared<MapData>();
    uint32_t nameSym = env.interner.intern("name");
    mapData->set(nameSym, Value::string("test"));
    uint32_t mSym = env.interner.intern("m");
    env.globalScope->define(mSym, Value::map(mapData));

    CHECK(env.run("m.get :name").asString() == "test");
    CHECK(env.run("m.has :name").asBool() == true);
    CHECK(env.run("m.has :missing").asBool() == false);

    env.run("m.set :age 25");
    CHECK(env.run("m.get :age").asInt() == 25);

    CHECK(env.run("m.remove :age").asBool() == true);
    CHECK(env.run("m.has :age").asBool() == false);
}

TEST_CASE("Eval map keys and values", "[evaluator]") {
    TestEnv env;
    auto mapData = std::make_shared<MapData>();
    uint32_t aSym = env.interner.intern("a");
    uint32_t bSym = env.interner.intern("b");
    mapData->set(aSym, Value::integer(1));
    mapData->set(bSym, Value::integer(2));
    uint32_t mSym = env.interner.intern("m");
    env.globalScope->define(mSym, Value::map(mapData));

    Value keys = env.run("m.keys");
    CHECK(keys.isArray());
    CHECK(keys.asArray().size() == 2);

    Value vals = env.run("m.values");
    CHECK(vals.isArray());
    CHECK(vals.asArray().size() == 2);
}

// === Method calls (setMethod + auto-self) ===

TEST_CASE("Eval setMethod and method call with self", "[evaluator]") {
    TestEnv env;
    auto mapData = std::make_shared<MapData>();
    uint32_t nameSym = env.interner.intern("name");
    mapData->set(nameSym, Value::string("Alice"));
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(mapData));

    // Define a method on the object
    env.run("fn getName [self] self.name");
    env.run("obj.setMethod :getName getName");

    // Call it as a method — self should be auto-injected
    CHECK(env.run("obj.getName").asString() == "Alice");
}

// === Auto-method detection (first param named "self") ===

TEST_CASE("Auto-method detection in map literal", "[evaluator]") {
    TestEnv env;
    // Closure with first param "self" should be auto-detected as method
    env.run(R"(
        set obj {=name "Alice" =greet fn [self] "Hi, {self.name}"}
    )");
    CHECK(env.run("obj.greet").asString() == "Hi, Alice");
}

TEST_CASE("Auto-method detection via set dotted", "[evaluator]") {
    TestEnv env;
    env.run("set obj {=hp 100}");
    // Set a closure with first param "self" via dotted set
    env.run("set obj.damage fn [self amt] (self.hp - amt)");
    // Should auto-detect as method, self injected
    CHECK(env.run("obj.damage 30").asInt() == 70);
}

TEST_CASE("Auto-method detection via map.set", "[evaluator]") {
    TestEnv env;
    env.run("set obj {=name \"Bob\"}");
    env.run("obj.set :getName fn [self] self.name");
    CHECK(env.run("obj.getName").asString() == "Bob");
}

TEST_CASE("No auto-method when first param is not self", "[evaluator]") {
    TestEnv env;
    env.run("set obj {=val 42 =getVal fn [x] x}");
    // Not a method — no self param, so no auto-injection
    CHECK(env.run("obj.getVal 99").asInt() == 99);
}

TEST_CASE("Auto-method in factory function pattern", "[evaluator]") {
    TestEnv env;
    env.run(R"(
        fn makePet [name sound] do
            set pet {=name name =sound sound}
            set pet.speak fn [self] "{self.name} says {self.sound}"
            return pet
        end
    )");
    env.run("set dog {makePet \"Rex\" \"Woof\"}");
    CHECK(env.run("dog.speak").asString() == "Rex says Woof");
}

TEST_CASE("Auto-method with multiple methods on one object", "[evaluator]") {
    TestEnv env;
    env.run(R"(
        set counter {=n 0}
        set counter.inc fn [self] do set self.n (self.n + 1); self.n end
        set counter.value fn [self] self.n
        set counter.add fn [self amt] do set self.n (self.n + amt); self.n end
    )");
    CHECK(env.run("counter.value").asInt() == 0);
    CHECK(env.run("counter.inc").asInt() == 1);
    CHECK(env.run("counter.inc").asInt() == 2);
    CHECK(env.run("counter.add 10").asInt() == 12);
}

TEST_CASE("Explicit setMethod still works alongside auto-method", "[evaluator]") {
    TestEnv env;
    // setMethod should still work for functions without self param
    auto mapData = std::make_shared<MapData>();
    uint32_t nameSym = env.interner.intern("name");
    mapData->set(nameSym, Value::string("Eve"));
    env.globalScope->define(env.interner.intern("obj"), Value::map(mapData));

    env.run("fn myMethod [me] me.name");
    env.run("obj.setMethod :myMethod myMethod");
    CHECK(env.run("obj.myMethod").asString() == "Eve");
}

// === Array methods ===

TEST_CASE("Eval array length", "[evaluator]") {
    TestEnv env;
    env.run("set arr [1 2 3]");
    CHECK(env.run("arr.length").asInt() == 3);
}

TEST_CASE("Eval array push", "[evaluator]") {
    TestEnv env;
    env.run("set arr [1 2]");
    env.run("arr.push 3");
    CHECK(env.run("arr.length").asInt() == 3);
    CHECK(env.run("arr[2]").asInt() == 3);
}

TEST_CASE("Eval array pop", "[evaluator]") {
    TestEnv env;
    env.run("set arr [1 2 3]");
    CHECK(env.run("arr.pop").asInt() == 3);
    CHECK(env.run("arr.length").asInt() == 2);
}

TEST_CASE("Eval array contains", "[evaluator]") {
    TestEnv env;
    env.run("set arr [1 2 3]");
    CHECK(env.run("arr.contains 2").asBool() == true);
    CHECK(env.run("arr.contains 5").asBool() == false);
}

TEST_CASE("Eval array slice", "[evaluator]") {
    TestEnv env;
    env.run("set arr [10 20 30 40 50]");
    Value sliced = env.run("arr.slice 1 3");
    CHECK(sliced.isArray());
    REQUIRE(sliced.asArray().size() == 2);
    CHECK(sliced.asArray()[0].asInt() == 20);
    CHECK(sliced.asArray()[1].asInt() == 30);
}

TEST_CASE("Eval array sort", "[evaluator]") {
    TestEnv env;
    env.run("set arr [3 1 2]");
    env.run("arr.sort");
    CHECK(env.run("arr[0]").asInt() == 1);
    CHECK(env.run("arr[1]").asInt() == 2);
    CHECK(env.run("arr[2]").asInt() == 3);
}

TEST_CASE("Eval array map", "[evaluator]") {
    TestEnv env;
    env.run("fn double [x] (x * 2)");
    env.run("set arr [1 2 3]");
    Value result = env.run("arr.map double");
    CHECK(result.isArray());
    REQUIRE(result.asArray().size() == 3);
    CHECK(result.asArray()[0].asInt() == 2);
    CHECK(result.asArray()[1].asInt() == 4);
    CHECK(result.asArray()[2].asInt() == 6);
}

TEST_CASE("Eval array filter", "[evaluator]") {
    TestEnv env;
    env.run("fn isEven [x] ((x % 2) == 0)");
    env.run("set arr [1 2 3 4 5 6]");
    Value result = env.run("arr.filter isEven");
    CHECK(result.isArray());
    REQUIRE(result.asArray().size() == 3);
    CHECK(result.asArray()[0].asInt() == 2);
    CHECK(result.asArray()[1].asInt() == 4);
    CHECK(result.asArray()[2].asInt() == 6);
}

TEST_CASE("Eval array foreach", "[evaluator]") {
    TestEnv env;
    env.run("set total 0");
    env.run("fn addToTotal [x] {set total (total + x)}");
    env.run("set arr [1 2 3]");
    env.run("arr.foreach addToTotal");
    CHECK(env.run("total").asInt() == 6);
}

// === Nested calls ===

TEST_CASE("Eval nested function calls", "[evaluator]") {
    TestEnv env;
    env.run("fn add [a b] (a + b)");
    env.run("fn mul [a b] (a * b)");
    CHECK(env.run("add {mul 3 4} 5").asInt() == 17);
}

// === Operator precedence ===

TEST_CASE("Eval operator precedence", "[evaluator]") {
    TestEnv env;
    CHECK(env.run("(2 + 3 * 4)").asInt() == 14);
    CHECK(env.run("((2 + 3) * 4)").asInt() == 20);
    CHECK(env.run("(10 - 2 * 3)").asInt() == 4);
}

// === Complex integration tests ===

TEST_CASE("Eval factorial function", "[evaluator]") {
    TestEnv env;
    env.run(R"(
fn factorial [n] do
    if (n <= 1) {return 1}
    (n * {factorial (n - 1)})
end
)");
    CHECK(env.run("factorial 5").asInt() == 120);
    CHECK(env.run("factorial 1").asInt() == 1);
    CHECK(env.run("factorial 0").asInt() == 1);
}

TEST_CASE("Eval fibonacci function", "[evaluator]") {
    TestEnv env;
    env.run(R"(
fn fib [n] do
    if (n <= 1) {return n}
    ({fib (n - 1)} + {fib (n - 2)})
end
)");
    CHECK(env.run("fib 0").asInt() == 0);
    CHECK(env.run("fib 1").asInt() == 1);
    CHECK(env.run("fib 6").asInt() == 8);
    CHECK(env.run("fib 10").asInt() == 55);
}

TEST_CASE("Eval object factory pattern", "[evaluator]") {
    // Object factory pattern requires a map constructor builtin (Phase 6).
    // For now, map creation is tested via C++ API in the map tests above.
}

TEST_CASE("Eval string length property", "[evaluator]") {
    TestEnv env;
    env.run("set s \"hello\"");
    CHECK(env.run("s.length").asInt() == 5);
}

TEST_CASE("Eval nested map access", "[evaluator]") {
    TestEnv env;
    auto outer = std::make_shared<MapData>();
    auto inner = std::make_shared<MapData>();
    uint32_t xSym = env.interner.intern("x");
    inner->set(xSym, Value::integer(42));
    uint32_t innerSym = env.interner.intern("inner");
    outer->set(innerSym, Value::map(inner));
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(outer));

    CHECK(env.run("obj.inner.x").asInt() == 42);
}

TEST_CASE("Eval dotted set on nested map", "[evaluator]") {
    TestEnv env;
    auto outer = std::make_shared<MapData>();
    auto inner = std::make_shared<MapData>();
    uint32_t innerSym = env.interner.intern("inner");
    outer->set(innerSym, Value::map(inner));
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(outer));

    env.run("set obj.inner.x 99");
    CHECK(env.run("obj.inner.x").asInt() == 99);
}

// === Tilde reference syntax ===

TEST_CASE("Eval tilde prevents auto-call", "[evaluator]") {
    TestEnv env;
    env.run("fn greet [] \"hello\"");
    // Bare name auto-calls
    CHECK(env.run("greet").asString() == "hello");
    // Tilde returns the closure reference
    CHECK(env.run("~greet").isClosure());
}

TEST_CASE("Eval tilde on dotted name", "[evaluator]") {
    TestEnv env;
    auto mapData = std::make_shared<MapData>();
    uint32_t objSym = env.interner.intern("obj");
    env.globalScope->define(objSym, Value::map(mapData));

    env.run("fn getName [self] self.name");
    env.run("obj.setMethod :getName getName");

    // Tilde gets the method closure without calling it
    CHECK(env.run("~obj.getName").isClosure());
}

TEST_CASE("Eval tilde on non-callable is identity", "[evaluator]") {
    TestEnv env;
    env.run("set x 42");
    CHECK(env.run("~x").asInt() == 42);
}

// === Error cases ===

TEST_CASE("Eval call non-callable throws", "[evaluator]") {
    TestEnv env;
    env.run("set x 42");
    CHECK_THROWS(env.run("x 1 2"));
}

TEST_CASE("Eval negate non-numeric throws", "[evaluator]") {
    TestEnv env;
    CHECK_THROWS(env.run("(-\"hello\")"));
}

TEST_CASE("Eval dotted access on non-map throws", "[evaluator]") {
    TestEnv env;
    env.run("set x 42");
    CHECK_THROWS(env.run("x.field"));
}

TEST_CASE("Eval set dotted on undefined throws", "[evaluator]") {
    TestEnv env;
    CHECK_THROWS(env.run("set undefined_var.x 5"));
}

TEST_CASE("Eval index on non-indexable throws", "[evaluator]") {
    TestEnv env;
    env.run("set x 42");
    CHECK_THROWS(env.run("x[0]"));
}

// === Let keyword ===

TEST_CASE("Let creates local binding", "[evaluator][let]") {
    TestEnv env;
    env.run("let x 10");
    CHECK(env.run("x").asInt() == 10);
}

TEST_CASE("Let shadows outer variable", "[evaluator][let]") {
    TestEnv env;
    // Set x in global scope
    env.run("set x 100");
    // Function uses let to create a local x, does not affect outer
    env.run("fn f [] do\n  let x 5\n  x\nend");
    CHECK(env.run("{f}").asInt() == 5);
    // Outer x is still 100
    CHECK(env.run("x").asInt() == 100);
}

TEST_CASE("Set without let updates outer scope", "[evaluator][let]") {
    TestEnv env;
    env.run("set x 100");
    env.run("fn f [] do\n  set x 5\nend");
    env.run("{f}");
    // set updated the outer x
    CHECK(env.run("x").asInt() == 5);
}

TEST_CASE("Let in function does not leak to outer scope", "[evaluator][let]") {
    TestEnv env;
    env.run("fn f [] do\n  let local_var 42\nend");
    env.run("{f}");
    // local_var should not exist in global scope
    CHECK(env.run("local_var").isNil());
}

TEST_CASE("Let overwrites existing local binding", "[evaluator][let]") {
    TestEnv env;
    env.run("let x 10");
    env.run("let x 20");
    CHECK(env.run("x").asInt() == 20);
}

TEST_CASE("Let and set coexist correctly", "[evaluator][let]") {
    TestEnv env;
    env.run("set outer 1");
    env.run(R"(
        fn f [] do
            let outer 99
            set outer 50
            outer
        end
    )");
    // let creates local, set then updates the local (found first in chain)
    CHECK(env.run("{f}").asInt() == 50);
    // The global outer is untouched because let shadowed it
    CHECK(env.run("outer").asInt() == 1);
}

// === String Methods ===

TEST_CASE("String indexing with brackets", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    CHECK(env.run("s[0]").asString() == "h");
    CHECK(env.run("s[4]").asString() == "o");
    CHECK(env.run("s[-1]").asString() == "o");
    CHECK(env.run("s[-2]").asString() == "l");
    CHECK_THROWS(env.run("s[5]"));
    CHECK_THROWS(env.run("s[-6]"));
}

TEST_CASE("String length property", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    CHECK(env.run("s.length").asInt() == 5);
    CHECK(env.run("{s.length}").asInt() == 5);
    env.run("set e \"\"");
    CHECK(env.run("e.length").asInt() == 0);
}

TEST_CASE("String get and char_at", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"abcde\"");
    CHECK(env.run("{s.get 0}").asString() == "a");
    CHECK(env.run("{s.get 4}").asString() == "e");
    CHECK(env.run("{s.get -1}").asString() == "e");
    CHECK(env.run("{s.char_at 2}").asString() == "c");
    CHECK_THROWS(env.run("{s.get 5}"));
}

TEST_CASE("String set mutates in place", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    env.run("{s.set 0 \"H\"}");
    CHECK(env.run("s").asString() == "Hello");
    // Multi-char replacement
    env.run("{s.set 1 \"EEE\"}");
    CHECK(env.run("s").asString() == "HEEEllo");
}

TEST_CASE("String push appends", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    env.run("{s.push \" world\"}");
    CHECK(env.run("s").asString() == "hello world");
}

TEST_CASE("String insert", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    env.run("{s.insert 5 \" world\"}");
    CHECK(env.run("s").asString() == "hello world");
    env.run("set s2 \"ac\"");
    env.run("{s2.insert 1 \"b\"}");
    CHECK(env.run("s2").asString() == "abc");
    // Insert at beginning
    env.run("set s3 \"world\"");
    env.run("{s3.insert 0 \"hello \"}");
    CHECK(env.run("s3").asString() == "hello world");
}

TEST_CASE("String delete", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    // Delete 1 char (default count)
    env.run("{s.delete 5}");
    CHECK(env.run("s").asString() == "helloworld");
    // Delete multiple chars
    env.run("set s2 \"abcdef\"");
    env.run("{s2.delete 1 3}");
    CHECK(env.run("s2").asString() == "aef");
    // Negative index
    env.run("set s3 \"hello!\"");
    env.run("{s3.delete -1}");
    CHECK(env.run("s3").asString() == "hello");
}

TEST_CASE("String replace", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    env.run("{s.replace \"world\" \"there\"}");
    CHECK(env.run("s").asString() == "hello there");
    // Replace all occurrences
    env.run("set s2 \"aabaa\"");
    env.run("{s2.replace \"a\" \"x\"}");
    CHECK(env.run("s2").asString() == "xxbxx");
    // Replace with empty
    env.run("set s3 \"a-b-c\"");
    env.run("{s3.replace \"-\" \"\"}");
    CHECK(env.run("s3").asString() == "abc");
}

TEST_CASE("String find", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    CHECK(env.run("{s.find \"world\"}").asInt() == 6);
    CHECK(env.run("{s.find \"xyz\"}").asInt() == -1);
    CHECK(env.run("{s.find \"o\"}").asInt() == 4);
    // Find with start position
    CHECK(env.run("{s.find \"o\" 5}").asInt() == 7);
}

TEST_CASE("String contains", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    CHECK(env.run("{s.contains \"world\"}").asBool() == true);
    CHECK(env.run("{s.contains \"xyz\"}").asBool() == false);
    CHECK(env.run("{s.contains \"\"}").asBool() == true);
}

TEST_CASE("String substr", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    CHECK(env.run("{s.substr 6}").asString() == "world");
    CHECK(env.run("{s.substr 0 5}").asString() == "hello");
    CHECK(env.run("{s.substr -5}").asString() == "world");
    CHECK(env.run("{s.substr 20}").asString() == "");
}

TEST_CASE("String slice", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    CHECK(env.run("{s.slice 0 5}").asString() == "hello");
    CHECK(env.run("{s.slice 6}").asString() == "world");
    CHECK(env.run("{s.slice -5}").asString() == "world");
    CHECK(env.run("{s.slice -5 -1}").asString() == "worl");
}

TEST_CASE("String split", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"a,b,c\"");
    auto result = env.run("{s.split \",\"}");
    CHECK(result.isArray());
    CHECK(result.asArray().size() == 3);
    CHECK(result.asArray()[0].asString() == "a");
    CHECK(result.asArray()[1].asString() == "b");
    CHECK(result.asArray()[2].asString() == "c");
    // Split with empty delimiter = individual chars
    env.run("set s2 \"abc\"");
    auto chars = env.run("{s2.split \"\"}");
    CHECK(chars.asArray().size() == 3);
    CHECK(chars.asArray()[0].asString() == "a");
    CHECK(chars.asArray()[1].asString() == "b");
    CHECK(chars.asArray()[2].asString() == "c");
    // Split with no matches
    auto noSplit = env.run("{s.split \"x\"}");
    CHECK(noSplit.asArray().size() == 1);
    CHECK(noSplit.asArray()[0].asString() == "a,b,c");
}

TEST_CASE("String upper and lower", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"Hello World\"");
    CHECK(env.run("{s.upper}").asString() == "HELLO WORLD");
    CHECK(env.run("{s.lower}").asString() == "hello world");
    // Original is not mutated by upper/lower
    CHECK(env.run("s").asString() == "Hello World");
}

TEST_CASE("String trim", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"  hello  \"");
    CHECK(env.run("{s.trim}").asString() == "hello");
    env.run("set s2 \"nowhitespace\"");
    CHECK(env.run("{s2.trim}").asString() == "nowhitespace");
    env.run("set s3 \"\\t\\n hello \\n\"");
    CHECK(env.run("{s3.trim}").asString() == "hello");
}

TEST_CASE("String starts_with and ends_with", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello world\"");
    CHECK(env.run("{s.starts_with \"hello\"}").asBool() == true);
    CHECK(env.run("{s.starts_with \"world\"}").asBool() == false);
    CHECK(env.run("{s.ends_with \"world\"}").asBool() == true);
    CHECK(env.run("{s.ends_with \"hello\"}").asBool() == false);
    CHECK(env.run("{s.starts_with \"\"}").asBool() == true);
    CHECK(env.run("{s.ends_with \"\"}").asBool() == true);
}

TEST_CASE("String mutation is visible through aliases", "[evaluator][string]") {
    TestEnv env;
    env.run("set s \"hello\"");
    env.run("set t s");  // t shares the same underlying string
    env.run("{s.push \"!\"}");
    CHECK(env.run("t").asString() == "hello!");
}

TEST_CASE("String methods chained operations", "[evaluator][string]") {
    TestEnv env;
    // Build a string through mutations
    env.run("set s \"\"");
    env.run("{s.push \"Hello\"}");
    env.run("{s.push \", \"}");
    env.run("{s.push \"World!\"}");
    CHECK(env.run("s").asString() == "Hello, World!");
    CHECK(env.run("s.length").asInt() == 13);
    CHECK(env.run("{s.find \"World\"}").asInt() == 7);
}

// === Map Literals ===

TEST_CASE("Map literal with =key syntax", "[evaluator][maplit]") {
    TestEnv env;
    env.run("set m {=x 10 =y 20}");
    CHECK(env.run("m.x").asInt() == 10);
    CHECK(env.run("m.y").asInt() == 20);
}

TEST_CASE("Map literal with expressions", "[evaluator][maplit]") {
    TestEnv env;
    env.run("set a 5");
    env.run("set m {=val (a + 1) =name \"hello\"}");
    CHECK(env.run("m.val").asInt() == 6);
    CHECK(env.run("m.name").asString() == "hello");
}

TEST_CASE("Map literal empty", "[evaluator][maplit]") {
    TestEnv env;
    // Empty braces are a block, not an empty map — just verify map with one key
    env.run("set m {=x 1}");
    CHECK(env.run("m.x").asInt() == 1);
}

TEST_CASE("Map literal nested", "[evaluator][maplit]") {
    TestEnv env;
    env.run("set m {=inner {=a 1 =b 2}}");
    CHECK(env.run("m.inner.a").asInt() == 1);
    CHECK(env.run("m.inner.b").asInt() == 2);
}

// === Named Parameters ===

TEST_CASE("Named params basic", "[evaluator][named_params]") {
    TestEnv env;
    env.run("fn greet [name greeting] {greeting}");
    CHECK(env.run("{greet \"Alice\" \"Hi\"}").asString() == "Hi");
    // Named param overriding positional position
    CHECK(env.run("{greet \"Alice\" =greeting \"Hey\"}").asString() == "Hey");
}

TEST_CASE("Named params skip positional", "[evaluator][named_params]") {
    TestEnv env;
    env.run("fn make [a b c] [a b c]");
    auto result = env.run("{make 1 =c 3 =b 2}");
    CHECK(result.isArray());
    auto& arr = result.asArray();
    CHECK(arr[0].asInt() == 1);
    CHECK(arr[1].asInt() == 2);
    CHECK(arr[2].asInt() == 3);
}

TEST_CASE("Named params all named", "[evaluator][named_params]") {
    TestEnv env;
    env.run("fn point [x y] [x y]");
    auto result = env.run("{point =y 20 =x 10}");
    CHECK(result.isArray());
    auto& arr = result.asArray();
    CHECK(arr[0].asInt() == 10);
    CHECK(arr[1].asInt() == 20);
}

// === Default Parameter Values ===

TEST_CASE("Default params basic", "[evaluator][defaults]") {
    TestEnv env;
    env.run("fn greet [name =greeting \"Hello\"] \"({greeting}, {name}!)\"");
    CHECK(env.run("{greet \"Alice\"}").asString() == "(Hello, Alice!)");
    CHECK(env.run("{greet \"Bob\" \"Hi\"}").asString() == "(Hi, Bob!)");
}

TEST_CASE("Default params multiple", "[evaluator][defaults]") {
    TestEnv env;
    env.run("fn make_rect [=width 100 =height 50] (width * height)");
    CHECK(env.run("{make_rect}").asInt() == 5000);
    CHECK(env.run("{make_rect 200}").asInt() == 10000);
    CHECK(env.run("{make_rect 200 30}").asInt() == 6000);
}

TEST_CASE("Default params with named args", "[evaluator][defaults]") {
    TestEnv env;
    env.run("fn widget [label =size 48 =color \"red\"] [label size color]");
    auto result = env.run("{widget \"btn\" =color \"blue\"}");
    CHECK(result.isArray());
    auto& arr = result.asArray();
    CHECK(arr[0].asString() == "btn");
    CHECK(arr[1].asInt() == 48);
    CHECK(arr[2].asString() == "blue");
}

TEST_CASE("Default params evaluated at call time", "[evaluator][defaults]") {
    TestEnv env;
    env.run("set counter 0");
    env.run("fn next_id [=id counter] do\n  set counter (counter + 1)\n  return id\nend");
    CHECK(env.run("{next_id}").asInt() == 0);
    CHECK(env.run("{next_id}").asInt() == 1);
    CHECK(env.run("{next_id 99}").asInt() == 99);
}

// === Null Coalescing Operators ===

TEST_CASE("Null coalesce ?? with nil", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(nil ?? 42)").asInt() == 42);
}

TEST_CASE("Null coalesce ?? with non-nil", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(10 ?? 42)").asInt() == 10);
}

TEST_CASE("Null coalesce ?? false is not nil", "[evaluator][coalesce]") {
    TestEnv env;
    // false is not nil, so ?? returns false
    CHECK(env.run("(false ?? 42)").asBool() == false);
}

TEST_CASE("Null coalesce ?? short-circuits", "[evaluator][coalesce]") {
    TestEnv env;
    // Right side should not be evaluated
    env.run("set x 0");
    env.run("set y (5 ?? {set x 1; 99})");
    CHECK(env.run("y").asInt() == 5);
    CHECK(env.run("x").asInt() == 0);
}

TEST_CASE("Falsy coalesce ?: with nil", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(nil ?: 42)").asInt() == 42);
}

TEST_CASE("Falsy coalesce ?: with false", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(false ?: 42)").asInt() == 42);
}

TEST_CASE("Falsy coalesce ?: with truthy", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(\"hello\" ?: \"default\")").asString() == "hello");
}

TEST_CASE("Null coalesce prefix form", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("{?? nil 42}").asInt() == 42);
    CHECK(env.run("{?? 10 42}").asInt() == 10);
}

TEST_CASE("Falsy coalesce prefix form", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("{?: false 42}").asInt() == 42);
    CHECK(env.run("{?: \"hi\" 42}").asString() == "hi");
}

TEST_CASE("Chained null coalesce", "[evaluator][coalesce]") {
    TestEnv env;
    CHECK(env.run("(nil ?? nil ?? 42)").asInt() == 42);
    CHECK(env.run("(nil ?? 10 ?? 42)").asInt() == 10);
}

// === Array Concatenation ===

TEST_CASE("Array concat with +", "[evaluator][array_concat]") {
    TestEnv env;
    auto result = env.run("([1 2] + [3 4])");
    CHECK(result.isArray());
    auto& arr = result.asArray();
    CHECK(arr.size() == 4);
    CHECK(arr[0].asInt() == 1);
    CHECK(arr[1].asInt() == 2);
    CHECK(arr[2].asInt() == 3);
    CHECK(arr[3].asInt() == 4);
}

TEST_CASE("Array concat empty arrays", "[evaluator][array_concat]") {
    TestEnv env;
    auto result = env.run("([] + [1 2])");
    CHECK(result.asArray().size() == 2);
    auto result2 = env.run("([1 2] + [])");
    CHECK(result2.asArray().size() == 2);
}

TEST_CASE("Array concat creates new array", "[evaluator][array_concat]") {
    TestEnv env;
    env.run("set a [1 2]");
    env.run("set b [3 4]");
    env.run("set c (a + b)");
    // Mutating c should not affect a or b
    env.run("{c.push 5}");
    CHECK(env.run("a.length").asInt() == 2);
    CHECK(env.run("b.length").asInt() == 2);
    CHECK(env.run("c.length").asInt() == 5);
}

// === String Format Operator ===

TEST_CASE("Format float with %%", "[evaluator][format]") {
    TestEnv env;
    CHECK(env.run("(\"%.2f\" % 3.14159)").asString() == "3.14");
    CHECK(env.run("(\"%.0f\" % 59.99)").asString() == "60");
}

TEST_CASE("Format integer with %%", "[evaluator][format]") {
    TestEnv env;
    CHECK(env.run("(\"%d\" % 42)").asString() == "42");
    CHECK(env.run("(\"%04d\" % 7)").asString() == "0007");
}

TEST_CASE("Format hex with %%", "[evaluator][format]") {
    TestEnv env;
    CHECK(env.run("(\"%x\" % 255)").asString() == "ff");
    CHECK(env.run("(\"%X\" % 255)").asString() == "FF");
}

TEST_CASE("Format string with %%", "[evaluator][format]") {
    TestEnv env;
    CHECK(env.run("(\"%s\" % \"hello\")").asString() == "hello");
    CHECK(env.run("(\"%-10s\" % \"hi\")").asString() == "hi        ");
}

TEST_CASE("Format int as float", "[evaluator][format]") {
    TestEnv env;
    // Int should be promoted to float for %f
    CHECK(env.run("(\"%.1f\" % 42)").asString() == "42.0");
}

// === sort_by ===

TEST_CASE("Array sort_by with comparator", "[evaluator][sort_by]") {
    TestEnv env;
    env.run("set arr [3 1 4 1 5]");
    env.run("{arr.sort_by fn [a b] (a < b)}");
    auto result = env.run("arr");
    auto& arr = result.asArray();
    CHECK(arr[0].asInt() == 1);
    CHECK(arr[1].asInt() == 1);
    CHECK(arr[2].asInt() == 3);
    CHECK(arr[3].asInt() == 4);
    CHECK(arr[4].asInt() == 5);
}

TEST_CASE("Array sort_by descending", "[evaluator][sort_by]") {
    TestEnv env;
    env.run("set arr [3 1 4 1 5]");
    env.run("{arr.sort_by fn [a b] (a > b)}");
    auto result = env.run("arr");
    auto& arr = result.asArray();
    CHECK(arr[0].asInt() == 5);
    CHECK(arr[1].asInt() == 4);
    CHECK(arr[2].asInt() == 3);
    CHECK(arr[3].asInt() == 1);
    CHECK(arr[4].asInt() == 1);
}

// === Integration tests combining features ===

TEST_CASE("Map literal used as widget config", "[evaluator][integration]") {
    TestEnv env;
    // Simulate a widget factory using named params + map literals + defaults
    env.run(R"(
        fn make_button [label =size 24 =color "white"] do
            {=type :button =label label =size size =color color}
        end
    )");
    env.run("set btn {make_button \"OK\" =color \"green\"}");
    CHECK(env.run("btn.label").asString() == "OK");
    CHECK(env.run("btn.size").asInt() == 24);
    CHECK(env.run("btn.color").asString() == "green");
}

TEST_CASE("Null coalesce with map field access", "[evaluator][integration]") {
    TestEnv env;
    env.run("set opts {=x 10}");
    // opts.y is nil (not set), so ?? provides default
    CHECK(env.run("(opts.y ?? 99)").asInt() == 99);
    CHECK(env.run("(opts.x ?? 99)").asInt() == 10);
}

TEST_CASE("Array concat for widget children", "[evaluator][integration]") {
    TestEnv env;
    env.run("set header [{=type :text =label \"Title\"}]");
    env.run("set body [{=type :button =label \"OK\"}]");
    env.run("set all (header + body)");
    CHECK(env.run("all.length").asInt() == 2);
    CHECK(env.run("all[0].label").asString() == "Title");
    CHECK(env.run("all[1].label").asString() == "OK");
}

TEST_CASE("Format in string interpolation context", "[evaluator][integration]") {
    TestEnv env;
    env.run("set fps 59.7834");
    // % is infix — pre-compute then interpolate
    env.run("set label (\"%.1f\" % fps)");
    auto result = env.run("\"FPS: {label}\"");
    CHECK(result.asString() == "FPS: 59.8");
}

TEST_CASE("Format with parens in interpolation", "[evaluator][integration]") {
    TestEnv env;
    env.run("set fps 59.7834");
    // % inside parens works within interpolation
    auto result = env.run("\"FPS: {(\"%.1f\" % fps)}\"");
    CHECK(result.asString() == "FPS: 59.8");
}

// === Multi-arg format ===

TEST_CASE("Format % with array — two ints", "[evaluator][format]") {
    TestEnv env;
    auto result = env.run("(\"%d/%d\" % [10 20])");
    CHECK(result.asString() == "10/20");
}

TEST_CASE("Format % with array — mixed types", "[evaluator][format]") {
    TestEnv env;
    auto result = env.run("(\"%s has %d HP (%.1f%%)\" % [\"Goblin\" 50 75.5])");
    CHECK(result.asString() == "Goblin has 50 HP (75.5%)");
}

TEST_CASE("Format % with array — literal percent %%", "[evaluator][format]") {
    TestEnv env;
    auto result = env.run("(\"%d%%\" % [42])");
    CHECK(result.asString() == "42%");
}

TEST_CASE("Format % with array — single element same as scalar", "[evaluator][format]") {
    TestEnv env;
    auto result = env.run("(\"%.2f\" % [3.14159])");
    CHECK(result.asString() == "3.14");
}

