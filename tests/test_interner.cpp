#include <catch2/catch_test_macros.hpp>
#include "finescript/interner.h"

using namespace finescript;

TEST_CASE("DefaultInterner basic intern and lookup", "[interner]") {
    DefaultInterner interner;

    uint32_t id = interner.intern("hello");
    CHECK(interner.lookup(id) == "hello");
}

TEST_CASE("DefaultInterner deduplication", "[interner]") {
    DefaultInterner interner;

    uint32_t id1 = interner.intern("stone");
    uint32_t id2 = interner.intern("stone");
    CHECK(id1 == id2);
}

TEST_CASE("DefaultInterner distinct strings get distinct IDs", "[interner]") {
    DefaultInterner interner;

    uint32_t a = interner.intern("stone");
    uint32_t b = interner.intern("dirt");
    uint32_t c = interner.intern("air");
    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);
}

TEST_CASE("DefaultInterner empty string", "[interner]") {
    DefaultInterner interner;

    uint32_t id = interner.intern("");
    CHECK(interner.lookup(id) == "");
    CHECK(interner.intern("") == id);
}

TEST_CASE("DefaultInterner many strings", "[interner]") {
    DefaultInterner interner;

    // Intern 1000 unique strings, verify all round-trip
    for (int i = 0; i < 1000; i++) {
        std::string s = "str_" + std::to_string(i);
        uint32_t id = interner.intern(s);
        CHECK(interner.lookup(id) == s);
    }

    // Verify dedup still works
    for (int i = 0; i < 1000; i++) {
        std::string s = "str_" + std::to_string(i);
        uint32_t id1 = interner.intern(s);
        uint32_t id2 = interner.intern(s);
        CHECK(id1 == id2);
    }
}

TEST_CASE("DefaultInterner sequential IDs", "[interner]") {
    DefaultInterner interner;

    CHECK(interner.intern("first") == 0);
    CHECK(interner.intern("second") == 1);
    CHECK(interner.intern("third") == 2);
    CHECK(interner.intern("first") == 0); // still 0
}

TEST_CASE("DefaultInterner invalid lookup throws", "[interner]") {
    DefaultInterner interner;
    CHECK_THROWS(interner.lookup(999));
}
