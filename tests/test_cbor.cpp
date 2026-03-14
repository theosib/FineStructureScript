#include <catch2/catch_test_macros.hpp>
#include "finescript/cbor.h"
#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/interner.h"
#include "finescript/error.h"
#include "finescript/native_function.h"
#include "finescript/execution_context.h"
#include "finescript/script_engine.h"
#include <cmath>
#include <limits>

using namespace finescript;

static DefaultInterner makeInterner() {
    return DefaultInterner{};
}

// ---- Primitive round-trips ----

TEST_CASE("CBOR nil round-trip", "[cbor]") {
    auto interner = makeInterner();
    auto encoded = cborEncode(Value::nil(), interner);
    REQUIRE(encoded.size() == 1);
    REQUIRE(encoded[0] == 0xF6);
    auto decoded = cborDecode(encoded, interner);
    REQUIRE(decoded.isNil());
}

TEST_CASE("CBOR bool round-trip", "[cbor]") {
    auto interner = makeInterner();

    auto encTrue = cborEncode(Value::boolean(true), interner);
    REQUIRE(encTrue.size() == 1);
    REQUIRE(encTrue[0] == 0xF5);
    REQUIRE(cborDecode(encTrue, interner).asBool() == true);

    auto encFalse = cborEncode(Value::boolean(false), interner);
    REQUIRE(encFalse.size() == 1);
    REQUIRE(encFalse[0] == 0xF4);
    REQUIRE(cborDecode(encFalse, interner).asBool() == false);
}

TEST_CASE("CBOR integer round-trip", "[cbor]") {
    auto interner = makeInterner();

    SECTION("zero") {
        auto enc = cborEncode(Value::integer(0), interner);
        REQUIRE(enc[0] == 0x00);
        REQUIRE(cborDecode(enc, interner).asInt() == 0);
    }

    SECTION("small positive") {
        auto enc = cborEncode(Value::integer(23), interner);
        REQUIRE(enc.size() == 1);
        REQUIRE(cborDecode(enc, interner).asInt() == 23);
    }

    SECTION("one-byte positive") {
        auto enc = cborEncode(Value::integer(200), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == 200);
    }

    SECTION("large positive") {
        int64_t big = 1000000000LL;
        auto enc = cborEncode(Value::integer(big), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == big);
    }

    SECTION("negative") {
        auto enc = cborEncode(Value::integer(-1), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == -1);
    }

    SECTION("large negative") {
        int64_t neg = -1000000000LL;
        auto enc = cborEncode(Value::integer(neg), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == neg);
    }

    SECTION("INT64_MIN") {
        int64_t minVal = std::numeric_limits<int64_t>::min();
        auto enc = cborEncode(Value::integer(minVal), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == minVal);
    }

    SECTION("INT64_MAX") {
        int64_t maxVal = std::numeric_limits<int64_t>::max();
        auto enc = cborEncode(Value::integer(maxVal), interner);
        REQUIRE(cborDecode(enc, interner).asInt() == maxVal);
    }
}

TEST_CASE("CBOR float round-trip", "[cbor]") {
    auto interner = makeInterner();

    SECTION("pi") {
        auto enc = cborEncode(Value::number(3.14159), interner);
        REQUIRE(enc[0] == 0xFB); // float64
        REQUIRE(cborDecode(enc, interner).asFloat() == 3.14159);
    }

    SECTION("negative") {
        auto enc = cborEncode(Value::number(-2.5), interner);
        REQUIRE(cborDecode(enc, interner).asFloat() == -2.5);
    }

    SECTION("zero") {
        auto enc = cborEncode(Value::number(0.0), interner);
        REQUIRE(cborDecode(enc, interner).asFloat() == 0.0);
    }
}

TEST_CASE("CBOR string round-trip", "[cbor]") {
    auto interner = makeInterner();

    SECTION("empty") {
        auto enc = cborEncode(Value::string(""), interner);
        auto dec = cborDecode(enc, interner);
        REQUIRE(dec.isString());
        REQUIRE(dec.asString().empty());
    }

    SECTION("hello") {
        auto enc = cborEncode(Value::string("hello"), interner);
        auto dec = cborDecode(enc, interner);
        REQUIRE(dec.asString() == "hello");
    }

    SECTION("embedded nulls") {
        std::string withNulls("ab\0cd", 5);
        auto enc = cborEncode(Value::string(withNulls), interner);
        auto dec = cborDecode(enc, interner);
        REQUIRE(dec.asString().size() == 5);
        REQUIRE(dec.asString() == withNulls);
    }
}

TEST_CASE("CBOR symbol round-trip", "[cbor]") {
    auto interner = makeInterner();
    uint32_t id = interner.intern("mySymbol");
    auto enc = cborEncode(Value::symbol(id), interner);

    // Should start with tag 22 (0xD6)
    REQUIRE(enc[0] == 0xD6);

    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.isSymbol());
    REQUIRE(dec.asSymbol() == id);
}

TEST_CASE("CBOR symbol decoded with different interner", "[cbor]") {
    auto interner1 = makeInterner();
    uint32_t id1 = interner1.intern("foo");
    auto enc = cborEncode(Value::symbol(id1), interner1);

    auto interner2 = makeInterner();
    auto dec = cborDecode(enc, interner2);
    REQUIRE(dec.isSymbol());
    // The symbol ID may differ, but the string should match
    REQUIRE(interner2.lookup(dec.asSymbol()) == "foo");
}

// ---- Array round-trips ----

TEST_CASE("CBOR empty array", "[cbor]") {
    auto interner = makeInterner();
    auto enc = cborEncode(Value::array(std::vector<Value>{}), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.isArray());
    REQUIRE(dec.asArray().empty());
}

TEST_CASE("CBOR mixed array", "[cbor]") {
    auto interner = makeInterner();
    std::vector<Value> elems;
    elems.push_back(Value::integer(42));
    elems.push_back(Value::string("test"));
    elems.push_back(Value::boolean(true));
    elems.push_back(Value::nil());

    auto enc = cborEncode(Value::array(std::move(elems)), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.isArray());
    REQUIRE(dec.asArray().size() == 4);
    REQUIRE(dec.asArray()[0].asInt() == 42);
    REQUIRE(dec.asArray()[1].asString() == "test");
    REQUIRE(dec.asArray()[2].asBool() == true);
    REQUIRE(dec.asArray()[3].isNil());
}

TEST_CASE("CBOR nested array", "[cbor]") {
    auto interner = makeInterner();
    std::vector<Value> inner;
    inner.push_back(Value::integer(1));
    inner.push_back(Value::integer(2));

    std::vector<Value> outer;
    outer.push_back(Value::array(std::move(inner)));
    outer.push_back(Value::integer(3));

    auto enc = cborEncode(Value::array(std::move(outer)), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.asArray()[0].isArray());
    REQUIRE(dec.asArray()[0].asArray().size() == 2);
    REQUIRE(dec.asArray()[1].asInt() == 3);
}

// ---- Map round-trips ----

TEST_CASE("CBOR empty map", "[cbor]") {
    auto interner = makeInterner();
    auto enc = cborEncode(Value::map(), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.isMap());
    REQUIRE(dec.asMap().keys().empty());
}

TEST_CASE("CBOR map with values", "[cbor]") {
    auto interner = makeInterner();
    auto mapData = std::make_shared<MapData>();
    mapData->set(interner.intern("x"), Value::integer(10));
    mapData->set(interner.intern("name"), Value::string("test"));
    mapData->set(interner.intern("flag"), Value::boolean(false));

    auto enc = cborEncode(Value::map(mapData), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.isMap());
    REQUIRE(dec.asMap().get(interner.intern("x")).asInt() == 10);
    REQUIRE(dec.asMap().get(interner.intern("name")).asString() == "test");
    REQUIRE(dec.asMap().get(interner.intern("flag")).asBool() == false);
}

TEST_CASE("CBOR nested map", "[cbor]") {
    auto interner = makeInterner();
    auto inner = std::make_shared<MapData>();
    inner->set(interner.intern("a"), Value::integer(1));

    auto outer = std::make_shared<MapData>();
    outer->set(interner.intern("child"), Value::map(inner));
    outer->set(interner.intern("b"), Value::integer(2));

    auto enc = cborEncode(Value::map(outer), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.asMap().get(interner.intern("child")).isMap());
    REQUIRE(dec.asMap().get(interner.intern("child")).asMap()
                .get(interner.intern("a")).asInt() == 1);
    REQUIRE(dec.asMap().get(interner.intern("b")).asInt() == 2);
}

// ---- Non-serializable types ----

TEST_CASE("CBOR closure encodes as null", "[cbor]") {
    auto interner = makeInterner();
    auto closure = std::make_shared<Closure>();
    closure->name = "testFunc";
    auto enc = cborEncode(Value::closure(closure), interner);
    REQUIRE(enc.size() == 1);
    REQUIRE(enc[0] == 0xF6); // null
}

TEST_CASE("CBOR native function encodes as null", "[cbor]") {
    auto interner = makeInterner();
    auto func = std::make_shared<SimpleLambdaFunction>(
        [](ExecutionContext&, const std::vector<Value>&) { return Value::nil(); });
    auto enc = cborEncode(Value::nativeFunction(func), interner);
    REQUIRE(enc.size() == 1);
    REQUIRE(enc[0] == 0xF6); // null
}

TEST_CASE("CBOR map with method entries encodes methods as null", "[cbor]") {
    auto interner = makeInterner();
    auto mapData = std::make_shared<MapData>();
    mapData->set(interner.intern("data"), Value::integer(42));

    auto closure = std::make_shared<Closure>();
    mapData->setMethod(interner.intern("doStuff"), Value::closure(closure));

    auto enc = cborEncode(Value::map(mapData), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.asMap().get(interner.intern("data")).asInt() == 42);
    REQUIRE(dec.asMap().get(interner.intern("doStuff")).isNil());
}

// ---- Cycle detection ----

TEST_CASE("CBOR array cycle throws", "[cbor]") {
    auto interner = makeInterner();
    auto arr = std::make_shared<std::vector<Value>>();
    arr->push_back(Value::integer(1));
    auto arrVal = Value::array(arr);
    arr->push_back(arrVal); // cycle: array contains itself

    REQUIRE_THROWS_AS(cborEncode(arrVal, interner), ScriptError);
}

TEST_CASE("CBOR map cycle throws", "[cbor]") {
    auto interner = makeInterner();
    auto mapData = std::make_shared<MapData>();
    auto mapVal = Value::map(mapData);
    mapData->set(interner.intern("self"), mapVal); // cycle: map contains itself

    REQUIRE_THROWS_AS(cborEncode(mapVal, interner), ScriptError);
}

TEST_CASE("CBOR diamond sharing is OK", "[cbor]") {
    auto interner = makeInterner();
    // Two arrays share the same inner array — not a cycle
    auto inner = std::make_shared<std::vector<Value>>();
    inner->push_back(Value::integer(99));
    auto shared = Value::array(inner);

    std::vector<Value> outer;
    outer.push_back(shared);
    outer.push_back(shared); // same array referenced twice

    auto enc = cborEncode(Value::array(std::move(outer)), interner);
    auto dec = cborDecode(enc, interner);
    REQUIRE(dec.asArray().size() == 2);
    REQUIRE(dec.asArray()[0].asArray()[0].asInt() == 99);
    REQUIRE(dec.asArray()[1].asArray()[0].asInt() == 99);
}

// ---- Decode errors ----

TEST_CASE("CBOR decode empty input throws", "[cbor]") {
    auto interner = makeInterner();
    REQUIRE_THROWS_AS(cborDecode(nullptr, 0, interner), ScriptError);
}

TEST_CASE("CBOR decode truncated string throws", "[cbor]") {
    auto interner = makeInterner();
    // Text string claiming length 10 but only 2 bytes of data
    std::vector<uint8_t> bad = {0x6A, 0x41, 0x42}; // major 3, len 10, "AB"
    REQUIRE_THROWS_AS(cborDecode(bad, interner), ScriptError);
}

// ---- Script builtins ----

TEST_CASE("CBOR script builtins round-trip", "[cbor]") {
    ScriptEngine engine;
    auto result = engine.executeCommand(
        R"(let encoded {cbor_encode 42}; cbor_decode encoded)",
        *([&]() {
            auto ctx = std::make_unique<ExecutionContext>(engine);
            return ctx;
        })());
    REQUIRE(result.success);
    REQUIRE(result.returnValue.asInt() == 42);
}
