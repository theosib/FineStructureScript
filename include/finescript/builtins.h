#pragma once

namespace finescript {

class ScriptEngine;

/// Register all built-in functions in the script engine's global scope.
void registerBuiltins(ScriptEngine& engine);

void registerMathBuiltins(ScriptEngine& engine);
void registerComparisonBuiltins(ScriptEngine& engine);
void registerStringBuiltins(ScriptEngine& engine);
void registerTypeBuiltins(ScriptEngine& engine);
void registerIOBuiltins(ScriptEngine& engine);

} // namespace finescript
