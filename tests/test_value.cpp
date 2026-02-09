#include <catch2/catch_test_macros.hpp>
#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/interner.h"
#include "finescript/native_function.h"

using namespace finescript;

TEST_CASE("Value default is nil", "[value]") {
    Value v;
    CHECK(v.isNil());
    CHECK(v.type() == Value::Type::Nil);
    CHECK_FALSE(v.truthy());
}

TEST_CASE("Value nil factory", "[value]") {
    auto v = Value::nil();
    CHECK(v.isNil());
    CHECK(v.toString() == "nil");
}

TEST_CASE("Value boolean", "[value]") {
    auto t = Value::boolean(true);
    auto f = Value::boolean(false);

    CHECK(t.isBool());
    CHECK(t.asBool() == true);
    CHECK(t.truthy());
    CHECK(t.toString() == "true");

    CHECK(f.isBool());
    CHECK(f.asBool() == false);
    CHECK_FALSE(f.truthy());
    CHECK(f.toString() == "false");
}

TEST_CASE("Value integer", "[value]") {
    auto v = Value::integer(42);
    CHECK(v.isInt());
    CHECK(v.isNumeric());
    CHECK(v.asInt() == 42);
    CHECK(v.asNumber() == 42.0);
    CHECK(v.truthy());
    CHECK(v.toString() == "42");

    auto zero = Value::integer(0);
    CHECK(zero.truthy()); // 0 is truthy (only nil and false are falsy)

    auto neg = Value::integer(-7);
    CHECK(neg.asInt() == -7);
}

TEST_CASE("Value float", "[value]") {
    auto v = Value::number(3.14);
    CHECK(v.isFloat());
    CHECK(v.isNumeric());
    CHECK(v.asFloat() == 3.14);
    CHECK(v.asNumber() == 3.14);
    CHECK(v.truthy());

    auto zero = Value::number(0.0);
    CHECK(zero.truthy()); // 0.0 is truthy
}

TEST_CASE("Value symbol", "[value]") {
    auto v = Value::symbol(42);
    CHECK(v.isSymbol());
    CHECK(v.asSymbol() == 42);
    CHECK(v.truthy());

    // Without interner, renders as :<id>
    CHECK(v.toString() == ":<42>");

    // With interner, renders as :name
    DefaultInterner interner;
    interner.intern("stone"); // id 0
    auto v2 = Value::symbol(0);
    CHECK(v2.toString(&interner) == ":stone");
}

TEST_CASE("Value string", "[value]") {
    auto v = Value::string("hello");
    CHECK(v.isString());
    CHECK(v.asString() == "hello");
    CHECK(v.truthy());
    CHECK(v.toString() == "hello");

    auto empty = Value::string("");
    CHECK(empty.truthy()); // empty string is truthy
    CHECK(empty.asString() == "");
}

TEST_CASE("Value array", "[value]") {
    auto v = Value::array({Value::integer(1), Value::integer(2), Value::integer(3)});
    CHECK(v.isArray());
    CHECK(v.asArray().size() == 3);
    CHECK(v.asArray()[0].asInt() == 1);
    CHECK(v.asArray()[2].asInt() == 3);
    CHECK(v.truthy());

    auto empty = Value::array(std::vector<Value>{});
    CHECK(empty.isArray());
    CHECK(empty.asArray().empty());
    CHECK(empty.truthy()); // empty array is truthy
}

TEST_CASE("Value map", "[value]") {
    auto v = Value::map();
    CHECK(v.isMap());
    CHECK(v.truthy());

    v.asMap().set(1, Value::integer(10));
    CHECK(v.asMap().has(1));
    CHECK(v.asMap().get(1).asInt() == 10);

    v.asMap().remove(1);
    CHECK_FALSE(v.asMap().has(1));
    CHECK(v.asMap().get(1).isNil());
}

TEST_CASE("Value closure", "[value]") {
    auto c = std::make_shared<Closure>();
    c->name = "test_fn";
    auto v = Value::closure(c);
    CHECK(v.isClosure());
    CHECK(v.isCallable());
    CHECK(v.truthy());
    CHECK(v.asClosure().name == "test_fn");
    CHECK(v.toString() == "<fn:test_fn>");
}

TEST_CASE("Value native function", "[value]") {
    auto fn = std::make_shared<SimpleLambdaFunction>(
        [](ExecutionContext&, const std::vector<Value>&) { return Value::nil(); }
    );
    auto v = Value::nativeFunction(fn);
    CHECK(v.isNativeFunction());
    CHECK(v.isCallable());
    CHECK(v.truthy());
    CHECK(v.toString() == "<native-fn>");
}

TEST_CASE("Value type mismatch throws", "[value]") {
    auto v = Value::nil();
    CHECK_THROWS(v.asInt());
    CHECK_THROWS(v.asBool());
    CHECK_THROWS(v.asFloat());
    CHECK_THROWS(v.asString());
    CHECK_THROWS(v.asSymbol());

    auto i = Value::integer(5);
    CHECK_THROWS(i.asBool());
    CHECK_THROWS(i.asFloat()); // int is not float specifically
    CHECK_NOTHROW(i.asNumber()); // but asNumber() works for int
}

TEST_CASE("Value equality", "[value]") {
    // Same types
    CHECK(Value::nil() == Value::nil());
    CHECK(Value::boolean(true) == Value::boolean(true));
    CHECK(Value::boolean(false) == Value::boolean(false));
    CHECK_FALSE(Value::boolean(true) == Value::boolean(false));
    CHECK(Value::integer(5) == Value::integer(5));
    CHECK_FALSE(Value::integer(5) == Value::integer(6));
    CHECK(Value::number(3.14) == Value::number(3.14));
    CHECK(Value::symbol(1) == Value::symbol(1));
    CHECK_FALSE(Value::symbol(1) == Value::symbol(2));
    CHECK(Value::string("hello") == Value::string("hello"));
    CHECK_FALSE(Value::string("hello") == Value::string("world"));

    // Different types are never equal
    CHECK_FALSE(Value::integer(5) == Value::number(5.0));
    CHECK_FALSE(Value::nil() == Value::boolean(false));
    CHECK_FALSE(Value::integer(0) == Value::nil());

    // Array equality by content
    auto a1 = Value::array({Value::integer(1), Value::integer(2)});
    auto a2 = Value::array({Value::integer(1), Value::integer(2)});
    auto a3 = Value::array({Value::integer(1), Value::integer(3)});
    CHECK(a1 == a2);
    CHECK_FALSE(a1 == a3);

    // Map equality by identity
    auto m1 = Value::map();
    auto m2 = Value::map();
    auto m3 = m1; // same shared_ptr
    CHECK_FALSE(m1 == m2);
    CHECK(m1 == m3);
}

TEST_CASE("Value truthiness summary", "[value]") {
    CHECK_FALSE(Value::nil().truthy());
    CHECK_FALSE(Value::boolean(false).truthy());
    CHECK(Value::boolean(true).truthy());
    CHECK(Value::integer(0).truthy());
    CHECK(Value::integer(1).truthy());
    CHECK(Value::number(0.0).truthy());
    CHECK(Value::string("").truthy());
    CHECK(Value::array(std::vector<Value>{}).truthy());
    CHECK(Value::map().truthy());
    CHECK(Value::symbol(0).truthy());
}

TEST_CASE("Value type names", "[value]") {
    CHECK(Value::nil().typeName() == "nil");
    CHECK(Value::boolean(true).typeName() == "bool");
    CHECK(Value::integer(0).typeName() == "int");
    CHECK(Value::number(0.0).typeName() == "float");
    CHECK(Value::symbol(0).typeName() == "symbol");
    CHECK(Value::string("").typeName() == "string");
    CHECK(Value::array(std::vector<Value>{}).typeName() == "array");
    CHECK(Value::map().typeName() == "map");
}

TEST_CASE("MapData method flags", "[value][map]") {
    auto m = Value::map();
    auto fn = std::make_shared<SimpleLambdaFunction>(
        [](ExecutionContext&, const std::vector<Value>&) { return Value::nil(); }
    );

    // Regular set — not a method
    m.asMap().set(1, Value::nativeFunction(fn));
    CHECK_FALSE(m.asMap().isMethod(1));

    // setMethod — marks as method
    m.asMap().setMethod(2, Value::nativeFunction(fn));
    CHECK(m.asMap().isMethod(2));

    // remove clears method flag
    m.asMap().remove(2);
    CHECK_FALSE(m.asMap().isMethod(2));
}

TEST_CASE("MapData keys", "[value][map]") {
    auto m = Value::map();
    m.asMap().set(10, Value::integer(1));
    m.asMap().set(20, Value::integer(2));
    m.asMap().set(30, Value::integer(3));

    auto keys = m.asMap().keys();
    CHECK(keys.size() == 3);
    // Keys may be in any order
    std::sort(keys.begin(), keys.end());
    CHECK(keys[0] == 10);
    CHECK(keys[1] == 20);
    CHECK(keys[2] == 30);
}
