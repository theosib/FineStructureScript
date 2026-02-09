#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace finescript {

// Forward declarations
class MapData;
class Interner;
class NativeFunctionObject;

/// Script closure (function + captured scope).
struct Closure {
    std::vector<uint32_t> paramIds;           // interned parameter names
    size_t numRequired = 0;                   // number of required params (without defaults)
    std::vector<const struct AstNode*> defaultExprs; // defaults for params[numRequired..]
    const struct AstNode* body = nullptr;     // raw pointer into AST
    std::shared_ptr<const struct AstNode> astRoot; // keeps AST alive
    std::shared_ptr<class Scope> capturedScope;
    std::string name;                         // empty if anonymous
};

/// The universal value type in finescript.
class Value {
public:
    enum class Type : std::size_t {
        Nil = 0,
        Bool,
        Int,
        Float,
        Symbol,
        String,
        Array,
        Map,
        Closure,
        NativeFunction
    };

    /// Default constructs nil.
    Value() : data_(std::monostate{}) {}

    // -- Static factories --
    static Value nil();
    static Value boolean(bool b);
    static Value integer(int64_t i);
    static Value number(double d);
    static Value symbol(uint32_t id);
    static Value string(std::string s);
    static Value string(std::shared_ptr<std::string> s);
    static Value array(std::vector<Value> elems);
    static Value array(std::shared_ptr<std::vector<Value>> a);
    static Value map();
    static Value map(std::shared_ptr<MapData> data);
    static Value proxyMap(std::shared_ptr<class ProxyMap> proxy);
    static Value closure(std::shared_ptr<finescript::Closure> c);
    static Value nativeFunction(std::shared_ptr<NativeFunctionObject> f);

    // -- Type queries --
    Type type() const { return static_cast<Type>(data_.index()); }
    bool isNil() const { return type() == Type::Nil; }
    bool isBool() const { return type() == Type::Bool; }
    bool isInt() const { return type() == Type::Int; }
    bool isFloat() const { return type() == Type::Float; }
    bool isNumeric() const { return isInt() || isFloat(); }
    bool isString() const { return type() == Type::String; }
    bool isSymbol() const { return type() == Type::Symbol; }
    bool isArray() const { return type() == Type::Array; }
    bool isMap() const { return type() == Type::Map; }
    bool isClosure() const { return type() == Type::Closure; }
    bool isNativeFunction() const { return type() == Type::NativeFunction; }
    bool isCallable() const { return isClosure() || isNativeFunction(); }

    // -- Accessors (throw ScriptError on type mismatch) --
    bool asBool() const;
    int64_t asInt() const;
    double asFloat() const;
    double asNumber() const;  // works for int or float
    uint32_t asSymbol() const;
    const std::string& asString() const;
    std::string& asStringMut();
    const std::vector<Value>& asArray() const;
    std::vector<Value>& asArrayMut();
    MapData& asMap();
    const MapData& asMap() const;
    finescript::Closure& asClosure();
    const finescript::Closure& asClosure() const;
    NativeFunctionObject& asNativeFunction();

    // -- Shared pointer access (for reference sharing) --
    std::shared_ptr<std::string>& stringPtr();
    std::shared_ptr<std::vector<Value>>& arrayPtr();
    std::shared_ptr<MapData>& mapPtr();

    // -- Truthiness: nil and false are falsy, everything else truthy --
    bool truthy() const;

    // -- Equality --
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    // -- Display --
    std::string toString(Interner* interner = nullptr) const;
    std::string typeName() const;

private:
    using Variant = std::variant<
        std::monostate,                              // Nil
        bool,                                        // Bool
        int64_t,                                     // Int
        double,                                      // Float
        uint32_t,                                    // Symbol
        std::shared_ptr<std::string>,                // String
        std::shared_ptr<std::vector<Value>>,         // Array
        std::shared_ptr<MapData>,                    // Map
        std::shared_ptr<finescript::Closure>,         // Closure
        std::shared_ptr<NativeFunctionObject>        // NativeFunction
    >;
    Variant data_;
};

} // namespace finescript
