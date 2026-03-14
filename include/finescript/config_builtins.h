#pragma once

namespace finescript {

class ScriptEngine;

/// Register config_parse and config_encode as script builtins.
///
/// config_parse(string) → map
///   Parses a "key: value" config format string into a finescript map.
///   Keys become map keys (interned symbols). Values are parsed as:
///   - "true"/"false" → bool
///   - integers → int
///   - floats → float
///   - everything else → string
///   Comments (#) and blank lines are ignored.
///
/// config_encode(map) → string
///   Encodes a finescript map back to "key: value" config format.
///   Keys are sorted alphabetically. Non-string/numeric/bool values are skipped.
void registerConfigBuiltins(ScriptEngine& engine);

} // namespace finescript
