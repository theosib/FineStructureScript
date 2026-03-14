# finescript

A prefix-first scripting language designed for embedding in game engines.
Single syntax for game logic, world generation, UI, and console commands.

```
set hp 100
fn take_damage [self amount] do
    set self.hp (self.hp - amount)
    if (self.hp <= 0) { set self.alive false }
end
```

## Features

- **Prefix-first syntax** — `verb arg arg ...`, like shell commands
- **Three bracket types** — `{prefix}`, `(infix math)`, `[array]`
- **First-class functions** — closures, higher-order, variadics, named params
- **Objects via maps** — `self`-parameter auto-detection, dot-call dispatch
- **String interpolation** — `"Hello {name}, you have {hp} HP"`
- **Embeddable** — C++17 library, `ScriptEngine` + `ExecutionContext` API
- **Pluggable** — custom `Interner`, `ResourceFinder`, `ProxyMap` interfaces
- **CBOR serialization** — encode/decode values to binary for network, save data, cross-context transfer
- **Hot reload** — AST cached by file timestamp, transparent reparse on change

## Building

```bash
cmake -B build && cmake --build build
./build/tests/finescript_tests
```

Requires: C++17, CMake 3.15+. No external dependencies (Catch2 fetched automatically for tests).

## Documentation

- **[GUIDE.md](GUIDE.md)** — language tutorial, reference, and C++ embedding API
- **[LLM_REFERENCE.md](LLM_REFERENCE.md)** — compact syntax reference for LLMs and code generation

## Quick Start

```cpp
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"

finescript::ScriptEngine engine;

// Register a native function
engine.registerFunction("give_gold",
    [](finescript::ExecutionContext&, const std::vector<finescript::Value>& args) {
        int64_t amount = args[0].asInt();
        // ... game logic ...
        return finescript::Value::boolean(true);
    });

// Execute a script
finescript::ExecutionContext ctx(engine);
ctx.set("player_name", finescript::Value::string("Alice"));
auto result = engine.executeCommand("give_gold 100", ctx);
if (result.success) {
    // result.returnValue contains the script's return value
}
```

## Namespace

`finescript`. Build target: `finescript` (shared library). File extension: `.fsc`.
