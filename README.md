# finescript

A prefix-first scripting language designed for embedding in game engines, with one syntax for game
logic, world generation, UI, and console commands. It runs all the game logic and UI of
[Keystone](https://github.com/theosib/Keystone-RTS), an RTS with no C++ of its own, on the
[FineStructureVoxel](https://github.com/theosib/FineStructureVoxel) engine.

```
set hp 100
fn take_damage [self amount] do
    set self.hp (self.hp - amount)
    if (self.hp <= 0) { set self.alive false }
end
```

## Why it looks like this

A line of finescript reads like a command, because in a game a lot of them are. The line a player
types into a console is the same language as the script behind a block. So `give_gold 100` needs no
parentheses, and a language for writing commands turns out to scale up to whole games.

Each kind of bracket has one job, so you can tell what any expression is by looking at it:

```
{random_range 1 6}            # Braces call something
(hp - damage * 2)             # Parentheses are ordinary infix maths
[1 2 3]                       # Brackets make an array
{=name "Hand" =hp 40}         # Named values make a map
```

Calls take named arguments, which keeps a long call readable:

```
spawn_unit :archer =x 10 =z 12 =facing :north
```

## Features

- **Prefix-first syntax**: `verb arg arg ...`, like shell commands
- **Objects are maps.** Functions in a map get `self` automatically when they ask for it, and
  `unit.take_damage 5` dispatches the way you'd expect, with no classes to declare
- **Maps and arrays are shared by reference.** A host and a script can hold the same data live, which
  is what lets [finegui](https://github.com/theosib/finegui) treat a GUI as a script data structure
- **First-class functions**: closures, higher-order functions, variadics, default and named parameters
- **Symbols**: `:name` values that compare as fast as integers, with interning you can share
- **String interpolation**: `"Hello {name}, you have {hp} HP"`
- **Native objects that look like maps.** With `ProxyMap`, a C++ object can present itself as a map,
  so a script reads and writes its fields with no copying and no glue
- **CBOR serialization**: values turn into bytes for the network, save data, or crossing between
  engines. Keystone's client and server talk this way
- **Hot reload**: parsed scripts are cached by file timestamp, and reparse by themselves on change
- **Pluggable**: bring your own `Interner` and `ResourceFinder`
- **Embeddable**: a small C++17 library with `ScriptEngine` and `ExecutionContext`

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

## License

Apache License 2.0: see [LICENSE](LICENSE) and [NOTICE](NOTICE).
