# finescript User Guide

finescript is a scripting language for the FineStructureVoxel game engine.
It handles game logic, world generation, UI, and console commands with a
single, consistent syntax.

---

## Getting Started

finescript uses **prefix-first** syntax: every statement is `verb arg arg ...`.
Think of it like typing commands.

```
print "Hello, world!"
set x 5
print x
```

Newlines separate statements. Semicolons work too: `set x 5; print x`.

---

## Data Types

| Type | Examples | Notes |
|------|----------|-------|
| Integer | `42`, `-7`, `0` | 64-bit signed |
| Float | `3.14`, `-0.5` | Double precision |
| String | `"hello"`, `"x={x}"` | Double-quoted, supports interpolation |
| Boolean | `true`, `false` | |
| Nil | `nil` | The absence of a value |
| Symbol | `:stone`, `:interact` | Interned name, used as map keys |
| Array | `[1 2 3]` | Ordered, heterogeneous |
| Map | `{=x 10 =y 20}` | Symbol-keyed dictionary |
| Function | `fn [x] (x * 2)` | First-class, closures |

### Truthiness

Only `nil` and `false` are falsy. Everything else is truthy, including `0`,
`""`, and `[]`.

---

## Three Delimiter Styles

finescript has three bracket types, each with a distinct purpose:

### Braces `{...}` --- Nested Prefix Expressions

Evaluate a prefix expression inline and return the result.

```
print {add 3 4}              # prints 7
set y {max x 100}            # y = max of x and 100
```

Multiple statements with semicolons:

```
{set x 5; set y 10; print (x + y)}
```

### Parentheses `(...)` --- Infix Math

Switch to standard math notation inside parentheses.

```
set result (x * 2 + 1)
if (health > 0) { print "alive" }
set avg ((a + b + c) / 3)
```

You can nest braces inside parentheses:

```
({getHeight x z} + 5)
```

### Brackets `[...]` --- Array Literals

Create arrays. Elements are space-separated.

```
set nums [1 2 3 4 5]
set names ["Alice" "Bob"]
set coords [x y (x + y)]     # expressions are evaluated
```

---

## Variables

Use `set` to create or update variables. Use `let` to declare a local variable.

```
set x 10
set name "Steve"
set x (x + 1)                # update x to 11
let y 5                       # declare y in current scope
```

### Scoping

finescript uses Python-style scoping:

- **`set`** on an existing name updates it wherever it was defined (walks up
  the scope chain). On a new name, creates it in the current scope.
- **`let`** always creates a variable in the current scope, even if the same
  name exists in an outer scope. Use `let` when you want a local that doesn't
  accidentally overwrite an outer variable.

```
set x 10
if true do
    set x 20                  # updates outer x
    set y 99                  # creates y in this scope
end
print x                       # 20
print y                       # nil (y is out of scope)
```

Using `let` to protect local variables:

```
set count 0
fn doWork [] do
    let count 100             # local count, outer count unchanged
    set count (count + 1)     # updates the local count
end
{doWork}
print count                   # still 0
```

Unbound names evaluate to `nil` rather than causing an error.

### The `global` Object

`global` is a special built-in that provides direct access to the top-level
scope. Use it to create or read variables in the top-level scope from inside
a function:

```
set counter 0

fn increment [] do
    set global.counter (global.counter + 1)
end

fn makeGlobal [] do
    set global.shared_state 42   # creates variable in top-level scope
end
```

`global.name` is also useful when a local variable shadows a top-level one:

```
set x 100
fn f [] do
    let x 5                       # local x
    print x                       # 5
    print global.x                # 100
end
```

---

## Functions

Define functions with `fn`. The parameter list is always in brackets.

```
# Named function
fn double [x] (x * 2)

# Multi-line body
fn clamp [x lo hi] do
    if (x < lo) { return lo }
    if (x > hi) { return hi }
    x
end

# Anonymous function
set square fn [x] (x * x)
```

### Default Parameter Values

Optional parameters have defaults specified with `=name default`:

```
fn make_slot [item =size 48] do
    print "Slot for {item} at size {size}"
end

make_slot "sword"             # size defaults to 48
make_slot "sword" 64          # size is 64
```

Required parameters come first, optional parameters last.

### Calling Functions

Functions are called in prefix position:

```
double 5                      # 10
print {clamp 150 0 100}       # 100
```

### Named Parameters

Use `=name value` to pass arguments by name:

```
fn make_button [label =size 24 =color "white"] do
    {=type :button =label label =size size =color color}
end

# Pass only the args you need
make_button "OK" =color "green"  # size defaults to 24
make_button "Cancel" =size 32    # color defaults to "white"
```

Named parameters can appear in any order after positional arguments.

Without explicit `return`, a function returns its last expression.

### Variadic Parameters

Use `[name]` inside the parameter list to collect remaining positional arguments
into an array, and `{name}` to collect unmatched named arguments into a map:

```
# Collect all positional args
fn sum [[nums]] do
    set total 0
    for n in nums do
        set total (total + n)
    end
    total
end
print {sum 1 2 3 4 5}         # 15

# Required params + rest
fn log [level [msgs]] do
    for m in msgs do
        print "[{level}] {m}"
    end
end
log "WARN" "disk full" "retry in 5s"

# Collect unmatched named args into a map
fn create [type {opts}] do
    print "Creating {type}"
    for k in {opts.keys} do
        print "  {k} = {opts.get k}"
    end
end
create "button" =label "OK" =size 24
```

`[rest]` and `{kwargs}` must come after all regular and default parameters.
Both produce empty collections if no excess arguments are provided.

### Closures

Functions capture their defining scope. This enables stateful closures:

```
fn makeCounter [] do
    set count 0
    fn [] do
        set count (count + 1)
        count
    end
end

set c {makeCounter}
print {c}                     # 1
print {c}                     # 2
print {c}                     # 3
```

### Function References with `~`

Bare function names in statement position are automatically called. Use `~`
to get a reference to the function without calling it.

```
fn greet [] { print "hello" }

greet                         # calls greet, prints "hello"
set f ~greet                  # stores the function itself in f
{f}                           # now call it
```

This also works with dotted names: `~obj.method` gets the method as a value.

### Higher-Order Functions

Functions are values. Pass them as arguments or return them.

```
fn apply [f x] {f x}

apply ~double 5               # 10

# Arrays have built-in higher-order methods
set nums [1 2 3 4 5]
set doubled {nums.map fn [x] (x * 2)}
set evens {nums.filter fn [x] ((x % 2) == 0)}
nums.foreach fn [x] { print x }
```

---

## Control Flow

### if / elif / else

Multi-line form:

```
if (x > 10) do
    print "big"
elif (x > 0) do
    print "small"
else do
    print "non-positive"
end
```

One-line form (brace bodies):

```
if (x > 5) {print "big"}
if (x > 5) {print "big"} {print "small"}    # with else
```

### for

Iterate over arrays or ranges.

```
# Range (exclusive)
for i in 0..5 do
    print i                   # 0, 1, 2, 3, 4
end

# Range (inclusive)
for i in 0..=5 do
    print i                   # 0, 1, 2, 3, 4, 5
end

# Array
for name in ["Alice" "Bob" "Carol"] do
    print "Hello {name}"
end
```

### while

```
set n 10
while (n > 0) do
    print n
    set n (n - 1)
end
```

### match

Pattern matching against values.

```
match direction
    :north { print "Going up" }
    :south { print "Going down" }
    _ { print "Going somewhere" }
end
```

`_` is the wildcard that matches anything.

### return

Exit a function early with a value.

```
fn find [arr target] do
    for item in arr do
        if (item == target) do
            return item
        end
    end
    return nil
end
```

`return` with no argument returns `nil`.

---

## Strings

### Interpolation

Expressions inside `{...}` within strings are evaluated and inserted:

```
set name "Steve"
set hp 20
print "Hello {name}, you have {hp} HP"
# Hello Steve, you have 20 HP

print "Result: {add 3 4}"
# Result: 7
```

### Escape Sequences

| Escape | Character |
|--------|-----------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |
| `\{` | Literal `{` |
| `\}` | Literal `}` |

---

## Arrays

```
set a [1 2 3]

# Indexing (zero-based)
print a[0]                    # 1
print a[-1]                   # 3 (negative = from end)

# Properties and methods
print {a.length}              # 3
a.push 4                      # [1 2 3 4]
set last {a.pop}              # removes and returns 4

# More methods
a.get 0                       # 1
a.set 0 99                    # [99 2 3]
a.slice 1 3                   # [2 3]
a.contains 2                  # true
a.sort                        # in-place sort (natural order)
a.sort_by fn [x y] (x > y)   # sort with custom comparator

# Higher-order
a.map fn [x] (x * 2)         # returns new array
a.filter fn [x] (x > 1)      # returns new array
a.foreach fn [x] { print x } # iterates, returns nil
```

---

## Maps (Dictionaries)

Maps use symbol keys. Create them with the `{=key value}` literal syntax:

```
set m {=name "Alice" =age 30 =score 100}

# Dot access
print m.name                  # "Alice"
print m.age                   # 30

# Method-style access
print {m.get :name}           # "Alice"
m.set :age 31                 # update
m.remove :score               # delete
print {m.has :name}           # true

# Iteration
for key in {m.keys} do
    print key {m.get key}
end
```

### Setting Nested Fields

```
set m.name "Bob"              # set works with dot notation
set m.address {=city "NYC" =zip "10001"}
set m.address.city "LA"       # nested dot set
```

The `map` built-in also still works: `{map :name "Alice" :age 30}`.

---

## Objects and Methods

Objects are just maps with methods. Any closure whose first parameter is named
`self` is **automatically detected as a method** when stored in a map --- via
map literals, `set obj.field`, or `m.set :key`. Methods automatically receive
the object as their first argument (`self`) when called with dot notation.

```
fn makeEnemy [hp speed] do
    set e {=health hp =speed speed =alive true}

    set e.damage fn [self amount] do
        set self.health (self.health - amount)
        if (self.health <= 0) do
            set self.alive false
        end
    end

    set e.isAlive fn [self] self.alive

    return e
end

set goblin {makeEnemy 50 1.2}
goblin.damage 10
print goblin.health           # 40
print {goblin.isAlive}        # true
```

You can also define methods inline in map literals:

```
set obj {=hp 100 =damage fn [self amt] do
    set self.hp (self.hp - amt)
end}
obj.damage 30
print obj.hp                  # 70
```

### Methods vs Data

- Closures with first param `self` → auto-detected as methods
- Closures without `self` first param → stored as plain functions (no self-injection)
- `setMethod` still works to explicitly mark any function as a method (even if first param isn't named `self`)

---

## Events

Scripts attached to game objects register event handlers with `on`.

```
on :interact do
    print "Block was clicked by {player.name}"
end

on :tick do
    set nearest {world.nearest_player self.x self.y self.z 16}
    if (nearest != nil) do
        self.move_toward nearest 0.5
    end
end
```

The engine provides context variables (`player`, `target`, `world`, `self`)
that are automatically available inside handlers.

---

## Imports

Use `source` to load another script file into the current scope (like
bash's `source` command):

```
source "utils.script"
source "ui_helpers.script"
```

Definitions from the sourced file become available in the current scope.

---

## Operators

### Infix (inside parentheses)

| Precedence | Operators | Description |
|-----------|-----------|-------------|
| Highest | `not`, `-` (unary) | Logical/arithmetic negation |
| 7 | `*`, `/`, `%` | Multiply, divide (truncating for int), modulo/format |
| 6 | `+`, `-` | Add, subtract, concatenate |
| 5 | `..`, `..=` | Range (exclusive, inclusive) |
| 4 | `<`, `>`, `<=`, `>=` | Comparison |
| 3 | `==`, `!=` | Equality |
| 2 | `and` | Logical and (short-circuit) |
| 1 | `or` | Logical or (short-circuit) |
| Lowest | `??`, `?:` | Null/falsy coalescing (short-circuit) |

Integer division truncates: `(7 / 2)` is `3`. Mix in a float for float
division: `(7.0 / 2)` is `3.5`.

String concatenation with `+`: `("hello" + " " + "world")`.
Array concatenation with `+`: `([1 2] + [3 4])` produces `[1 2 3 4]`.

### String Format Operator `%`

The `%` operator on strings does printf-style formatting:

```
("%.2f" % 3.14159)           # "3.14"
("%d" % 42)                  # "42"
("%04x" % 255)               # "00ff"
```

For multiple values, pass an array on the right side:

```
("%d/%d" % [50 100])         # "50/100"
("%s has %d HP" % ["Goblin" 50])  # "Goblin has 50 HP"
```

Use `%%` in the format string for a literal `%`: `("%d%%" % 42)` produces `"42%"`.

Alternatively, use the `format` built-in function for a prefix-style multi-arg call:

```
format "%d/%d" hp max_hp
format "%s: %.1f" "FPS" fps
"Health: {format \"%d/%d\" hp max_hp}"  # works in interpolation
```

Supported specifiers: `%d`, `%i`, `%f`, `%e`, `%g`, `%x`, `%X`, `%o`, `%s`.
Integers auto-promote to float for `%f`.

### Null / Falsy Coalescing

```
(x ?? "default")              # returns x unless x is nil
(x ?: "default")              # returns x unless x is falsy (nil or false)
```

Prefix form also works: `{?? x "default"}` and `{?: x "default"}`.

---

## Built-in Functions

### Math

| Function | Description |
|----------|-------------|
| `abs x` | Absolute value |
| `min a b` | Minimum |
| `max a b` | Maximum |
| `floor x` | Round down to integer |
| `ceil x` | Round up to integer |
| `round x` | Round to nearest integer |
| `sqrt x` | Square root (returns float) |
| `pow base exp` | Exponentiation |
| `sin x`, `cos x`, `tan x` | Trigonometry (radians) |
| `random` | Random integer |
| `random_range lo hi` | Random integer in [lo, hi] |
| `random_float` | Random float in [0, 1) |

### String

| Function | Description |
|----------|-------------|
| `str_length s` | String length |
| `str_concat a b ...` | Concatenate (converts non-strings) |
| `str_substr s start [len]` | Substring |
| `str_find s needle` | Find index (-1 if not found) |
| `str_upper s` | Uppercase |
| `str_lower s` | Lowercase |
| `format fmt a b ...` | Printf-style multi-arg formatting |

### Type Conversion

| Function | Description |
|----------|-------------|
| `to_int x` | Convert to integer |
| `to_float x` | Convert to float |
| `to_str x` | Convert to string |
| `to_bool x` | Convert to boolean |
| `type x` | Get type name ("int", "float", "string", etc.) |

### Comparison (prefix form)

| Function | Description |
|----------|-------------|
| `eq a b` | Equal |
| `ne a b` | Not equal |
| `lt a b` | Less than |
| `gt a b` | Greater than |
| `le a b` | Less or equal |
| `ge a b` | Greater or equal |

### I/O

| Function | Description |
|----------|-------------|
| `print a b ...` | Print space-separated values with newline |

---

## Comments

Line comments start with `#`:

```
# This is a comment
set x 5  # inline comment
```

---

## Blocks

`do...end` creates a multi-line block. Returns the last expression.

```
set result do
    set a 10
    set b 20
    (a + b)
end
# result is 30
```

Braces `{...}` are the single-line equivalent:

```
set result {set a 10; set b 20; (a + b)}
```

---

## C++ Embedding

finescript is designed to be embedded in C++ applications. Here are the key
APIs.

### Basic Usage

```cpp
#include "finescript/script_engine.h"
#include "finescript/execution_context.h"

finescript::ScriptEngine engine;
finescript::ExecutionContext ctx(engine);

// Execute a command
auto result = engine.executeCommand("(1 + 2)", ctx);
if (result.success) {
    int64_t val = result.returnValue.asInt(); // 3
}
```

### Registering Functions and Constants

```cpp
engine.registerFunction("give_gold",
    [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
        int amount = args[0].asInt();
        // ... game logic ...
        return Value::boolean(true);
    });

engine.registerConstant("MAX_HEALTH", Value::integer(100));
```

When a script calls a native function with named arguments (e.g., `give_gold 100 =source "quest"`),
the named args are collected into a map and appended as the last positional argument. The native
function receives `args = [100, {source: "quest"}]`.

### Context Variables

```cpp
ExecutionContext ctx(engine);
ctx.set("player_name", Value::string("Alice"));
ctx.set("player_x", Value::integer(100));
// Scripts can now use player_name and player_x directly
```

### Calling Script Functions from C++

Use `callFunction` to invoke a script closure or native function that you
hold as a `Value`:

```cpp
// Suppose a script defined: fn double [x] (x * 2)
// And you have the closure as a Value (e.g., from an event handler):
Value result = engine.callFunction(closure, {Value::integer(21)}, ctx);
// result.asInt() == 42
```

This is especially useful for GUI callbacks, event handlers, and any
pattern where scripts register closures that C++ invokes later:

```cpp
// After script runs `on :interact do ... end`:
for (auto& handler : ctx.eventHandlers()) {
    if (handler.eventSymbol == interactSymbol) {
        engine.callFunction(handler.handlerFunction, {}, ctx);
    }
}
```

### Loading Script Files

```cpp
auto* script = engine.loadScript("path/to/script.fsc");
auto result = engine.execute(*script, ctx);

// Scripts are cached by path; reload on file change:
engine.invalidateCache("path/to/script.fsc");
```

### Resource Finder

By default, `source "path"` treats the argument as a literal filesystem path. You can
plug in a custom resource finder to resolve logical names (e.g., `"blocks/torch"`) to
actual paths — useful for mod directories, asset bundles, or search paths.

```cpp
class MyResourceFinder : public finescript::ResourceFinder {
public:
    std::filesystem::path resolve(std::string_view name) override {
        // Search mod directories, asset bundles, etc.
        auto path = modsDir_ / (std::string(name) + ".fsc");
        if (std::filesystem::exists(path)) return path;
        return {};  // empty = not found
    }
private:
    std::filesystem::path modsDir_ = "mods/scripts";
};

MyResourceFinder finder;
engine.setResourceFinder(&finder);
// Now: source "blocks/torch"  →  resolves via finder  →  loads mods/scripts/blocks/torch.fsc
```

---

## Quick Reference

```
# Variables
set x 10
set x (x + 1)
let y 5                       # always local

# Functions
fn add [a b] (a + b)
fn greet [] { print "hi" }

# Control flow
if (x > 0) { print "positive" }
for i in 0..10 do print i end
while (n > 0) { set n (n - 1) }

# Data structures
set arr [1 2 3]
set m {=key "value"}
print arr[0]
print m.key

# String interpolation
print "x is {x}"

# Function references
set f ~myFunction
{f arg1 arg2}

# Events
on :interact do
    print "clicked"
end

# Imports
source "lib.script"
```
