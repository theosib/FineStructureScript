#pragma once

#include <cstdint>
#include <vector>

namespace finescript {

class Value;
class Interner;
class ScriptEngine;

/// Encode a Value to CBOR binary.
/// Throws ScriptError on cycles in the object graph.
/// Closure and NativeFunction values are encoded as CBOR null.
std::vector<uint8_t> cborEncode(const Value& value, Interner& interner);

/// Decode CBOR binary to a Value.
/// Throws ScriptError on malformed input.
Value cborDecode(const std::vector<uint8_t>& data, Interner& interner);

/// Decode CBOR from a raw byte range.
Value cborDecode(const uint8_t* data, size_t length, Interner& interner);

/// Register cbor_encode and cbor_decode as script builtins.
void registerCborBuiltins(ScriptEngine& engine);

} // namespace finescript
