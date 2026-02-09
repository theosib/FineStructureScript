#include <catch2/catch_test_macros.hpp>
#include "finescript/scope.h"
#include "finescript/interner.h"

using namespace finescript;

TEST_CASE("Scope createGlobal", "[scope]") {
    auto global = Scope::createGlobal();
    REQUIRE(global != nullptr);
    CHECK(global->parent() == nullptr);
}

TEST_CASE("Scope createChild", "[scope]") {
    auto global = Scope::createGlobal();
    auto child = global->createChild();
    REQUIRE(child != nullptr);
    CHECK(child->parent() == global);
}

TEST_CASE("Scope define and lookup", "[scope]") {
    DefaultInterner interner;
    auto scope = Scope::createGlobal();
    uint32_t sym = interner.intern("x");

    scope->define(sym, Value::integer(42));

    Value* v = scope->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->isInt());
    CHECK(v->asInt() == 42);
}

TEST_CASE("Scope lookup not found returns nullptr", "[scope]") {
    DefaultInterner interner;
    auto scope = Scope::createGlobal();
    uint32_t sym = interner.intern("unknown");

    CHECK(scope->lookup(sym) == nullptr);
}

TEST_CASE("Scope lookup walks parent chain", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    uint32_t sym = interner.intern("x");
    global->define(sym, Value::integer(10));

    auto child = global->createChild();
    Value* v = child->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 10);
}

TEST_CASE("Scope child shadows parent", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    uint32_t sym = interner.intern("x");
    global->define(sym, Value::integer(10));

    auto child = global->createChild();
    child->define(sym, Value::integer(20));

    Value* v = child->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 20);

    v = global->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 10);
}

TEST_CASE("Scope set creates in current scope if new", "[scope]") {
    DefaultInterner interner;
    auto scope = Scope::createGlobal();
    uint32_t sym = interner.intern("x");

    scope->set(sym, Value::integer(5));

    Value* v = scope->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 5);
}

TEST_CASE("Scope set updates existing in parent (Python semantics)", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    uint32_t sym = interner.intern("x");
    global->define(sym, Value::integer(10));

    auto child = global->createChild();
    child->set(sym, Value::integer(20));

    Value* v = global->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 20);

    CHECK_FALSE(child->hasLocal(sym));
}

TEST_CASE("Scope set creates locally when not in any parent", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    auto child = global->createChild();
    uint32_t sym = interner.intern("newvar");

    child->set(sym, Value::integer(99));

    CHECK(child->hasLocal(sym));
    CHECK_FALSE(global->hasLocal(sym));
    CHECK(global->lookup(sym) == nullptr);
}

TEST_CASE("Scope hasLocal", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    uint32_t sym = interner.intern("x");
    uint32_t sym2 = interner.intern("y");

    global->define(sym, Value::integer(1));

    CHECK(global->hasLocal(sym));
    CHECK_FALSE(global->hasLocal(sym2));
}

TEST_CASE("Scope define overwrites existing local", "[scope]") {
    DefaultInterner interner;
    auto scope = Scope::createGlobal();
    uint32_t sym = interner.intern("x");

    scope->define(sym, Value::integer(1));
    scope->define(sym, Value::integer(2));

    Value* v = scope->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 2);
}

TEST_CASE("Scope deep parent chain", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    uint32_t sym = interner.intern("deep");
    global->define(sym, Value::string("found"));

    auto l1 = global->createChild();
    auto l2 = l1->createChild();
    auto l3 = l2->createChild();

    Value* v = l3->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asString() == "found");
}

TEST_CASE("Scope set through deep chain updates correct level", "[scope]") {
    DefaultInterner interner;
    auto global = Scope::createGlobal();
    auto child = global->createChild();
    auto grandchild = child->createChild();

    uint32_t sym = interner.intern("x");
    child->define(sym, Value::integer(10));

    grandchild->set(sym, Value::integer(99));

    CHECK_FALSE(grandchild->hasLocal(sym));
    Value* v = child->lookup(sym);
    REQUIRE(v != nullptr);
    CHECK(v->asInt() == 99);
}

TEST_CASE("Scope multiple bindings", "[scope]") {
    DefaultInterner interner;
    auto scope = Scope::createGlobal();
    uint32_t a = interner.intern("a");
    uint32_t b = interner.intern("b");
    uint32_t c = interner.intern("c");

    scope->define(a, Value::integer(1));
    scope->define(b, Value::string("two"));
    scope->define(c, Value::boolean(true));

    CHECK(scope->lookup(a)->asInt() == 1);
    CHECK(scope->lookup(b)->asString() == "two");
    CHECK(scope->lookup(c)->asBool() == true);
}
