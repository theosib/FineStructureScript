#include "finescript/cbor.h"
#include "finescript/value.h"
#include "finescript/map_data.h"
#include "finescript/interner.h"
#include "finescript/error.h"
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"
#include <cstring>
#include <unordered_set>

namespace finescript {
namespace {

// CBOR initial byte constants
constexpr uint8_t CBOR_FALSE   = 0xF4;
constexpr uint8_t CBOR_TRUE    = 0xF5;
constexpr uint8_t CBOR_NULL    = 0xF6;
constexpr uint8_t CBOR_FLOAT64 = 0xFB;

constexpr uint64_t SYMBOL_TAG  = 22; // custom CBOR tag for finescript symbols

// ---- Encoder ----

void writeUint8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void writeUint16BE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void writeUint32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void writeUint64BE(std::vector<uint8_t>& out, uint64_t v) {
    out.push_back(static_cast<uint8_t>(v >> 56));
    out.push_back(static_cast<uint8_t>(v >> 48));
    out.push_back(static_cast<uint8_t>(v >> 40));
    out.push_back(static_cast<uint8_t>(v >> 32));
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

// Write CBOR initial byte: major type (0-7) shifted left 5, plus argument encoding.
void writeHead(std::vector<uint8_t>& out, uint8_t majorType, uint64_t arg) {
    uint8_t mt = static_cast<uint8_t>(majorType << 5);
    if (arg < 24) {
        writeUint8(out, mt | static_cast<uint8_t>(arg));
    } else if (arg <= 0xFF) {
        writeUint8(out, mt | 24);
        writeUint8(out, static_cast<uint8_t>(arg));
    } else if (arg <= 0xFFFF) {
        writeUint8(out, mt | 25);
        writeUint16BE(out, static_cast<uint16_t>(arg));
    } else if (arg <= 0xFFFFFFFF) {
        writeUint8(out, mt | 26);
        writeUint32BE(out, static_cast<uint32_t>(arg));
    } else {
        writeUint8(out, mt | 27);
        writeUint64BE(out, arg);
    }
}

void writeFloat64(std::vector<uint8_t>& out, double v) {
    writeUint8(out, CBOR_FLOAT64);
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    writeUint64BE(out, bits);
}

void writeTextString(std::vector<uint8_t>& out, std::string_view s) {
    writeHead(out, 3, s.size());
    out.insert(out.end(), s.begin(), s.end());
}

void encodeValue(std::vector<uint8_t>& out, const Value& value,
                 Interner& interner,
                 std::unordered_set<const void*>& visited) {
    switch (value.type()) {
        case Value::Type::Nil:
            writeUint8(out, CBOR_NULL);
            break;

        case Value::Type::Bool:
            writeUint8(out, value.asBool() ? CBOR_TRUE : CBOR_FALSE);
            break;

        case Value::Type::Int: {
            int64_t n = value.asInt();
            if (n >= 0) {
                writeHead(out, 0, static_cast<uint64_t>(n));
            } else {
                // CBOR negative: encode -1-n as unsigned
                writeHead(out, 1, static_cast<uint64_t>(-(n + 1)));
            }
            break;
        }

        case Value::Type::Float:
            writeFloat64(out, value.asFloat());
            break;

        case Value::Type::Symbol: {
            auto sv = interner.lookup(value.asSymbol());
            writeHead(out, 6, SYMBOL_TAG); // tag 22
            writeTextString(out, sv);
            break;
        }

        case Value::Type::String:
            writeTextString(out, value.asString());
            break;

        case Value::Type::Array: {
            const auto& arr = value.asArray();
            const void* ptr = &arr;
            if (!visited.insert(ptr).second) {
                throw ScriptError("CBOR encode: cycle detected in array", SourceLocation{});
            }
            writeHead(out, 4, arr.size());
            for (const auto& elem : arr) {
                encodeValue(out, elem, interner, visited);
            }
            visited.erase(ptr);
            break;
        }

        case Value::Type::Map: {
            const auto& map = value.asMap();
            const void* ptr = &map;
            if (!visited.insert(ptr).second) {
                throw ScriptError("CBOR encode: cycle detected in map", SourceLocation{});
            }
            auto mapKeys = map.keys();
            writeHead(out, 5, mapKeys.size());
            for (uint32_t key : mapKeys) {
                writeTextString(out, interner.lookup(key));
                encodeValue(out, map.get(key), interner, visited);
            }
            visited.erase(ptr);
            break;
        }

        case Value::Type::Closure:
        case Value::Type::NativeFunction:
            // Non-serializable: encode as null
            writeUint8(out, CBOR_NULL);
            break;
    }
}

// ---- Decoder ----

struct DecodeState {
    const uint8_t* data;
    size_t length;
    size_t pos = 0;
};

uint8_t readByte(DecodeState& s) {
    if (s.pos >= s.length) {
        throw ScriptError("CBOR decode: unexpected end of input", SourceLocation{});
    }
    return s.data[s.pos++];
}

void readBytes(DecodeState& s, void* dst, size_t count) {
    if (s.pos + count > s.length) {
        throw ScriptError("CBOR decode: unexpected end of input", SourceLocation{});
    }
    std::memcpy(dst, s.data + s.pos, count);
    s.pos += count;
}

uint64_t readArgument(DecodeState& s, uint8_t additionalInfo) {
    if (additionalInfo < 24) return additionalInfo;
    if (additionalInfo == 24) return readByte(s);
    if (additionalInfo == 25) {
        uint8_t buf[2];
        readBytes(s, buf, 2);
        return (static_cast<uint64_t>(buf[0]) << 8) | buf[1];
    }
    if (additionalInfo == 26) {
        uint8_t buf[4];
        readBytes(s, buf, 4);
        return (static_cast<uint64_t>(buf[0]) << 24) |
               (static_cast<uint64_t>(buf[1]) << 16) |
               (static_cast<uint64_t>(buf[2]) << 8) |
                static_cast<uint64_t>(buf[3]);
    }
    if (additionalInfo == 27) {
        uint8_t buf[8];
        readBytes(s, buf, 8);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | buf[i];
        }
        return v;
    }
    throw ScriptError("CBOR decode: unsupported additional info " +
                       std::to_string(additionalInfo), SourceLocation{});
}

double readFloat64(DecodeState& s) {
    uint8_t buf[8];
    readBytes(s, buf, 8);
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits = (bits << 8) | buf[i];
    }
    double v;
    std::memcpy(&v, &bits, 8);
    return v;
}

Value decodeItem(DecodeState& s, Interner& interner) {
    uint8_t ib = readByte(s);
    uint8_t major = ib >> 5;
    uint8_t ai = ib & 0x1F;

    switch (major) {
        case 0: { // unsigned integer
            uint64_t val = readArgument(s, ai);
            return Value::integer(static_cast<int64_t>(val));
        }
        case 1: { // negative integer
            uint64_t val = readArgument(s, ai);
            // CBOR negative: value is -1 - val
            return Value::integer(static_cast<int64_t>(~val));
        }
        case 2: { // byte string -> treat as finescript string
            uint64_t len = readArgument(s, ai);
            if (s.pos + len > s.length) {
                throw ScriptError("CBOR decode: byte string length exceeds input", SourceLocation{});
            }
            std::string str(reinterpret_cast<const char*>(s.data + s.pos), len);
            s.pos += len;
            return Value::string(std::move(str));
        }
        case 3: { // text string
            uint64_t len = readArgument(s, ai);
            if (s.pos + len > s.length) {
                throw ScriptError("CBOR decode: text string length exceeds input", SourceLocation{});
            }
            std::string str(reinterpret_cast<const char*>(s.data + s.pos), len);
            s.pos += len;
            return Value::string(std::move(str));
        }
        case 4: { // array
            uint64_t count = readArgument(s, ai);
            std::vector<Value> elems;
            elems.reserve(static_cast<size_t>(count));
            for (uint64_t i = 0; i < count; ++i) {
                elems.push_back(decodeItem(s, interner));
            }
            return Value::array(std::move(elems));
        }
        case 5: { // map
            uint64_t count = readArgument(s, ai);
            auto mapData = std::make_shared<MapData>();
            for (uint64_t i = 0; i < count; ++i) {
                // Key must be a text/byte string; intern it as a symbol
                uint8_t keyIb = readByte(s);
                uint8_t keyMajor = keyIb >> 5;
                uint8_t keyAi = keyIb & 0x1F;
                if (keyMajor != 3 && keyMajor != 2) {
                    throw ScriptError("CBOR decode: map key must be a string", SourceLocation{});
                }
                uint64_t keyLen = readArgument(s, keyAi);
                if (s.pos + keyLen > s.length) {
                    throw ScriptError("CBOR decode: map key length exceeds input", SourceLocation{});
                }
                std::string_view keyStr(reinterpret_cast<const char*>(s.data + s.pos), keyLen);
                s.pos += keyLen;
                uint32_t sym = interner.intern(keyStr);
                Value val = decodeItem(s, interner);
                mapData->set(sym, std::move(val));
            }
            return Value::map(std::move(mapData));
        }
        case 6: { // tag
            uint64_t tag = readArgument(s, ai);
            if (tag == SYMBOL_TAG) {
                // Symbol: next item must be a text string
                uint8_t strIb = readByte(s);
                uint8_t strMajor = strIb >> 5;
                uint8_t strAi = strIb & 0x1F;
                if (strMajor != 3) {
                    throw ScriptError("CBOR decode: symbol tag must wrap a text string", SourceLocation{});
                }
                uint64_t len = readArgument(s, strAi);
                if (s.pos + len > s.length) {
                    throw ScriptError("CBOR decode: symbol string length exceeds input", SourceLocation{});
                }
                std::string_view sv(reinterpret_cast<const char*>(s.data + s.pos), len);
                s.pos += len;
                return Value::symbol(interner.intern(sv));
            }
            // Unknown tag: skip tag and decode the content item
            return decodeItem(s, interner);
        }
        case 7: { // simple values and floats
            if (ai == 20) return Value::boolean(false);
            if (ai == 21) return Value::boolean(true);
            if (ai == 22) return Value::nil();
            if (ai == 23) return Value::nil(); // undefined -> nil
            if (ai == 25) {
                // float16 — not emitted by our encoder, but decode for interop
                uint8_t buf[2];
                readBytes(s, buf, 2);
                uint16_t half = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
                // IEEE 754 half-precision to double
                int sign = (half >> 15) & 1;
                int exp = (half >> 10) & 0x1F;
                int mant = half & 0x3FF;
                double val;
                if (exp == 0) {
                    val = std::ldexp(mant, -24); // subnormal
                } else if (exp == 31) {
                    val = mant == 0 ? std::numeric_limits<double>::infinity()
                                    : std::numeric_limits<double>::quiet_NaN();
                } else {
                    val = std::ldexp(mant + 1024, exp - 25);
                }
                if (sign) val = -val;
                return Value::number(val);
            }
            if (ai == 26) {
                // float32
                uint8_t buf[4];
                readBytes(s, buf, 4);
                uint32_t bits = (static_cast<uint32_t>(buf[0]) << 24) |
                                (static_cast<uint32_t>(buf[1]) << 16) |
                                (static_cast<uint32_t>(buf[2]) << 8) |
                                 static_cast<uint32_t>(buf[3]);
                float f;
                std::memcpy(&f, &bits, 4);
                return Value::number(static_cast<double>(f));
            }
            if (ai == 27) {
                return Value::number(readFloat64(s));
            }
            throw ScriptError("CBOR decode: unsupported simple value " +
                               std::to_string(ai), SourceLocation{});
        }
        default:
            throw ScriptError("CBOR decode: invalid major type", SourceLocation{});
    }
}

} // anonymous namespace

// ---- Public API ----

std::vector<uint8_t> cborEncode(const Value& value, Interner& interner) {
    std::vector<uint8_t> out;
    std::unordered_set<const void*> visited;
    encodeValue(out, value, interner, visited);
    return out;
}

Value cborDecode(const std::vector<uint8_t>& data, Interner& interner) {
    return cborDecode(data.data(), data.size(), interner);
}

Value cborDecode(const uint8_t* data, size_t length, Interner& interner) {
    DecodeState s{data, length, 0};
    return decodeItem(s, interner);
}

// ---- Script builtins ----

void registerCborBuiltins(ScriptEngine& engine) {
    engine.registerFunction("cbor_encode",
        [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
            if (args.empty()) return Value::nil();
            auto bytes = cborEncode(args[0], ctx.engine().interner());
            std::string result(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            return Value::string(std::move(result));
        });

    engine.registerFunction("cbor_decode",
        [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) return Value::nil();
            const auto& s = args[0].asString();
            return cborDecode(
                reinterpret_cast<const uint8_t*>(s.data()),
                s.size(),
                ctx.engine().interner());
        });
}

} // namespace finescript
