#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/interner.h"
#include "finescript/native_function.h"
#include "finescript/proxy_map.h"
#include "finescript/error.h"
#include <sstream>
#include <stdexcept>

namespace finescript {

// -- ReturnSignal implementation (needs complete Value) --

struct ReturnSignal::Impl {
    Value val;
    Impl() = default;
    explicit Impl(Value v) : val(std::move(v)) {}
};

ReturnSignal::ReturnSignal() : impl_(std::make_unique<Impl>()) {}
ReturnSignal::~ReturnSignal() = default;
ReturnSignal::ReturnSignal(ReturnSignal&& other) noexcept = default;
ReturnSignal& ReturnSignal::operator=(ReturnSignal&& other) noexcept = default;
ReturnSignal::ReturnSignal(Value value) : impl_(std::make_unique<Impl>(std::move(value))) {}
Value& ReturnSignal::value() { return impl_->val; }
const Value& ReturnSignal::value() const { return impl_->val; }

// -- SourceLocation --

std::string SourceLocation::toString() const {
    return "<file:" + std::to_string(fileId) + ">:" +
           std::to_string(line) + ":" + std::to_string(column);
}

// -- Value static factories --

Value Value::nil() { return Value(); }

Value Value::boolean(bool b) {
    Value v;
    v.data_ = b;
    return v;
}

Value Value::integer(int64_t i) {
    Value v;
    v.data_ = i;
    return v;
}

Value Value::number(double d) {
    Value v;
    v.data_ = d;
    return v;
}

Value Value::symbol(uint32_t id) {
    Value v;
    v.data_ = id;
    return v;
}

Value Value::string(std::string s) {
    Value v;
    v.data_ = std::make_shared<std::string>(std::move(s));
    return v;
}

Value Value::string(std::shared_ptr<std::string> s) {
    Value v;
    v.data_ = std::move(s);
    return v;
}

Value Value::array(std::vector<Value> elems) {
    Value v;
    v.data_ = std::make_shared<std::vector<Value>>(std::move(elems));
    return v;
}

Value Value::array(std::shared_ptr<std::vector<Value>> a) {
    Value v;
    v.data_ = std::move(a);
    return v;
}

Value Value::map() {
    Value v;
    v.data_ = std::make_shared<MapData>();
    return v;
}

Value Value::map(std::shared_ptr<MapData> data) {
    Value v;
    v.data_ = std::move(data);
    return v;
}

Value Value::proxyMap(std::shared_ptr<ProxyMap> proxy) {
    Value v;
    v.data_ = std::make_shared<MapData>(std::move(proxy));
    return v;
}

Value Value::closure(std::shared_ptr<Closure> c) {
    Value v;
    v.data_ = std::move(c);
    return v;
}

Value Value::nativeFunction(std::shared_ptr<NativeFunctionObject> f) {
    Value v;
    v.data_ = std::move(f);
    return v;
}

// -- Accessors --

bool Value::asBool() const {
    if (auto* p = std::get_if<bool>(&data_)) return *p;
    throw std::runtime_error("Value is not a bool, got " + typeName());
}

int64_t Value::asInt() const {
    if (auto* p = std::get_if<int64_t>(&data_)) return *p;
    throw std::runtime_error("Value is not an int, got " + typeName());
}

double Value::asFloat() const {
    if (auto* p = std::get_if<double>(&data_)) return *p;
    throw std::runtime_error("Value is not a float, got " + typeName());
}

double Value::asNumber() const {
    if (auto* p = std::get_if<int64_t>(&data_)) return static_cast<double>(*p);
    if (auto* p = std::get_if<double>(&data_)) return *p;
    throw std::runtime_error("Value is not numeric, got " + typeName());
}

uint32_t Value::asSymbol() const {
    if (auto* p = std::get_if<uint32_t>(&data_)) return *p;
    throw std::runtime_error("Value is not a symbol, got " + typeName());
}

const std::string& Value::asString() const {
    if (auto* p = std::get_if<std::shared_ptr<std::string>>(&data_)) return **p;
    throw std::runtime_error("Value is not a string, got " + typeName());
}

std::string& Value::asStringMut() {
    if (auto* p = std::get_if<std::shared_ptr<std::string>>(&data_)) return **p;
    throw std::runtime_error("Value is not a string, got " + typeName());
}

const std::vector<Value>& Value::asArray() const {
    if (auto* p = std::get_if<std::shared_ptr<std::vector<Value>>>(&data_)) return **p;
    throw std::runtime_error("Value is not an array, got " + typeName());
}

std::vector<Value>& Value::asArrayMut() {
    if (auto* p = std::get_if<std::shared_ptr<std::vector<Value>>>(&data_)) return **p;
    throw std::runtime_error("Value is not an array, got " + typeName());
}

MapData& Value::asMap() {
    if (auto* p = std::get_if<std::shared_ptr<MapData>>(&data_)) return **p;
    throw std::runtime_error("Value is not a map, got " + typeName());
}

const MapData& Value::asMap() const {
    if (auto* p = std::get_if<std::shared_ptr<MapData>>(&data_)) return **p;
    throw std::runtime_error("Value is not a map, got " + typeName());
}

Closure& Value::asClosure() {
    if (auto* p = std::get_if<std::shared_ptr<Closure>>(&data_)) return **p;
    throw std::runtime_error("Value is not a closure, got " + typeName());
}

const Closure& Value::asClosure() const {
    if (auto* p = std::get_if<std::shared_ptr<Closure>>(&data_)) return **p;
    throw std::runtime_error("Value is not a closure, got " + typeName());
}

NativeFunctionObject& Value::asNativeFunction() {
    if (auto* p = std::get_if<std::shared_ptr<NativeFunctionObject>>(&data_)) return **p;
    throw std::runtime_error("Value is not a native function, got " + typeName());
}

std::shared_ptr<std::string>& Value::stringPtr() {
    return std::get<std::shared_ptr<std::string>>(data_);
}

std::shared_ptr<std::vector<Value>>& Value::arrayPtr() {
    return std::get<std::shared_ptr<std::vector<Value>>>(data_);
}

std::shared_ptr<MapData>& Value::mapPtr() {
    return std::get<std::shared_ptr<MapData>>(data_);
}

// -- Truthiness --

bool Value::truthy() const {
    if (isNil()) return false;
    if (isBool()) return asBool();
    return true;
}

// -- Equality --

bool Value::operator==(const Value& other) const {
    if (type() != other.type()) return false;

    switch (type()) {
        case Type::Nil: return true;
        case Type::Bool: return asBool() == other.asBool();
        case Type::Int: return asInt() == other.asInt();
        case Type::Float: return asFloat() == other.asFloat();
        case Type::Symbol: return asSymbol() == other.asSymbol();
        case Type::String: return asString() == other.asString();
        case Type::Array: {
            auto& a = asArray();
            auto& b = other.asArray();
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++) {
                if (a[i] != b[i]) return false;
            }
            return true;
        }
        // Maps, closures, native functions compared by identity
        case Type::Map:
            return std::get<std::shared_ptr<MapData>>(data_).get() ==
                   std::get<std::shared_ptr<MapData>>(other.data_).get();
        case Type::Closure:
            return std::get<std::shared_ptr<Closure>>(data_).get() ==
                   std::get<std::shared_ptr<Closure>>(other.data_).get();
        case Type::NativeFunction:
            return std::get<std::shared_ptr<NativeFunctionObject>>(data_).get() ==
                   std::get<std::shared_ptr<NativeFunctionObject>>(other.data_).get();
    }
    return false;
}

// -- Display --

std::string Value::typeName() const {
    switch (type()) {
        case Type::Nil: return "nil";
        case Type::Bool: return "bool";
        case Type::Int: return "int";
        case Type::Float: return "float";
        case Type::Symbol: return "symbol";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Map: return "map";
        case Type::Closure: return "function";
        case Type::NativeFunction: return "function";
    }
    return "unknown";
}

std::string Value::toString(Interner* interner) const {
    switch (type()) {
        case Type::Nil: return "nil";
        case Type::Bool: return asBool() ? "true" : "false";
        case Type::Int: return std::to_string(asInt());
        case Type::Float: {
            std::ostringstream oss;
            oss << asFloat();
            return oss.str();
        }
        case Type::Symbol:
            if (interner) {
                return ":" + std::string(interner->lookup(asSymbol()));
            }
            return ":<" + std::to_string(asSymbol()) + ">";
        case Type::String: return asString();
        case Type::Array: {
            std::string result = "[";
            auto& arr = asArray();
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += " ";
                result += arr[i].toString(interner);
            }
            result += "]";
            return result;
        }
        case Type::Map: return "<map>";
        case Type::Closure: {
            auto& c = asClosure();
            if (c.name.empty()) return "<fn>";
            return "<fn:" + c.name + ">";
        }
        case Type::NativeFunction: return "<native-fn>";
    }
    return "<unknown>";
}

} // namespace finescript
