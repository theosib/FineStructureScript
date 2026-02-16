# finescript LLM Reference

Compact reference for LLMs generating or analyzing finescript code.

## Syntax Model

Prefix-first: `verb arg arg ...`. Three bracket types with distinct roles:
- `{expr}` = evaluate prefix expression, return result (also map literal: `{=key val ...}`)
- `(expr)` = infix math/logic
- `[a b c]` = array literal

Three symbol sigils: `name` (scope lookup), `:name` (literal symbol), `=name` (key specifier for map literals, named params, default params).

Newline or `;` separates statements. `#` starts line comment.

## Types

nil, bool, int (i64), float (f64), string, symbol (`:name`, interned u32), array, map (symbol-keyed), closure, native function.

Truthiness: only `nil` and `false` are falsy. `0`, `""`, `[]` are truthy.

## Variables

```
set x 5
set x (x + 1)
let y 10                       # always creates in current scope
```
- `set`: updates nearest existing binding in scope chain, or creates in current scope if unbound.
- `let`: always creates/overwrites in current scope (local declaration). Use to avoid clobbering outer variables.
- `global.name`: read/write top-level scope directly. Use to create globals from functions or disambiguate shadowed names.
- Unbound name = nil.

## Functions

```
fn name [params] body          # named
fn [params] body               # anonymous
fn name [params] do ... end    # multi-line
fn name [req =opt default] body  # with default parameter values
fn name [req [rest]] body      # variadic positional (rest = array)
fn name [req [rest] {opts}] body # variadic positional + named (opts = map)
```
Last expression is return value. `return val` for early exit (non-local jump).
Closures capture defining scope by reference. Defaults evaluated at call time.

`~name` prevents auto-calling, returns function reference.

Named parameters at call site: `{func posArg =name1 val1 =name2 val2}`.
Named args matched to param names; unmatched params get defaults or nil.
**Native functions**: named args are collected into a map and appended as the last positional argument.

**Variadic params**: `[rest]` collects remaining positional args into an array. `{kwargs}` collects unmatched named args into a map. Both must come after all regular/default params. Empty array/map if no excess args.

## Control Flow

```
if cond do ... elif cond do ... else do ... end
if cond {then} {else}          # one-line form
for var in iterable do ... end # iterable: array or range
while cond do ... end
match scrutinee
    pattern1 body1
    pattern2 body2
    _ default_body
end
return val
```

## Operators (inside parens, by precedence low→high)

`??` `?:` `or` `and` `==` `!=` `<` `>` `<=` `>=` `..` `..=` `+` `-` `*` `/` `%` `not` `-`(unary)

int/int division truncates. Mixed int/float promotes to float. `+` concatenates strings and arrays.
`and`/`or` short-circuit. `..` exclusive range, `..=` inclusive range (both produce arrays).
`??` null coalesce: returns left unless nil, then evaluates right. `?:` falsy coalesce: returns left unless falsy.
`%` on strings: format operator — `("%.2f" % 3.14)` → `"3.14"`. Multi-arg: `("%d/%d" % [10 20])` → `"10/20"`.
`+` on arrays: concatenation — `([1 2] + [3 4])` → `[1 2 3 4]` (new array).
Both `??` and `?:` also work as prefix verbs: `{?? expr fallback}`, `{?: expr fallback}`.

## Strings

Double-quoted. Interpolation: `"x={x}, sum={add 1 2}"`. Escapes: `\n \t \r \\ \" \{ \}`.

## Arrays

```
set a [1 2 3]
a[0]                  # index (no space before [)
a[-1]                 # negative index from end
```

Methods (dot-call on array):
- `.length` `.push elem` `.pop` `.get i` `.set i val`
- `.slice start [end]` `.contains val` `.sort` `.sort_by fn`
- `.map fn` `.filter fn` `.foreach fn`

## Maps

Symbol keys only. Two creation syntaxes:
```
{=key1 val1 =key2 val2}           # map literal (preferred)
{map :key1 val1 :key2 val2}       # map function (legacy)
```

```
m.field               # dot access
set m.field val       # dot set (including nested: set m.a.b val)
```

Methods: `.get :key` `.set :key val` `.has :key` `.remove :key` `.keys` `.values` `.setMethod :name fn`

## Objects

Maps with methods. **Auto-detection**: any closure whose first parameter is named `self` is automatically marked as a method when stored in a map (via map literal, `set obj.field`, or `m.set :key`). Dot-call auto-injects receiver as first arg (`self`).

```
set obj {=hp 100 =damage fn [self amt] do
    set self.hp (self.hp - amt)
end}
obj.damage 10         # self=obj injected automatically
```

`setMethod` still works for closures where the first param isn't named `self`.

Factory functions serve as constructors. No class system.

## Events

```
on :eventName do ... end
```
Registers closure in ExecutionContext. Engine provides context vars (`player`, `target`, `world`, `self`).

## Imports

```
source "file.script"
```
Executes in current scope (definitions persist).

## Built-in Functions

Math: `abs` `min` `max` `floor` `ceil` `round` `sqrt` `pow` `sin` `cos` `tan` `random` `random_range lo hi` `random_float`

String: `str_length` `str_concat a b ...` `str_substr s start [len]` `str_find s needle` `str_upper` `str_lower` `format fmt args...`

Type: `to_int` `to_float` `to_str` `to_bool` `type`

Comparison (prefix): `eq` `ne` `lt` `gt` `le` `ge`

I/O: `print a b ...`

Map: `map :k1 v1 :k2 v2 ...`

## Common Patterns

```
# Factory function (constructor pattern)
fn makeThing [x y] do
    set t {=x x =y y}
    set t.move fn [self dx dy] do
        set self.x (self.x + dx)
        set self.y (self.y + dy)
    end
    return t
end

# Default parameters
fn connect [host =port 8080 =timeout 30] do
    print "Connecting to" host "port" port
end
connect "localhost"                    # port=8080, timeout=30
connect "localhost" =port 9090         # override port only

# Named parameters
fn createWidget [=type :button =label "OK" =width 100] do
    {=type type =label label =width width}
end
set w {createWidget =label "Cancel" =type :link}

# Null coalescing
set name (?? player.name "Unknown")
set hp (health ?: 100)                # falsy coalesce

# Counter closure
fn makeCounter [] do
    set n 0
    fn [] do set n (n + 1); n end
end

# Higher-order
set doubled {arr.map fn [x] (x * 2)}
set big {arr.filter fn [x] (x > 10)}

# Custom sort
set sorted {items.sort_by fn [a b] (a.name < b.name)}

# Array concatenation
set all (base_items + extra_items)

# String formatting
set label ("%.1f" % fps)
set hud ("%d/%d" % [hp max_hp])
set msg {format "%s has %d HP" name hp}

# Variadic function
fn sum [[nums]] do
    set total 0
    for n in nums do set total (total + n) end
    total
end
sum 1 2 3 4 5                 # 15

# Flexible config with kwargs
fn create [type {opts}] do
    set obj {=type type}
    for k in {opts.keys} do
        obj.set k {opts.get k}
    end
    obj
end

# Iterate map
for k in {m.keys} do
    print k {m.get k}
end

# Conditional (one-line)
if (x > 0) {print "pos"} {print "non-pos"}

# Pattern matching
match val
    1 "one"
    2 "two"
    _ "other"
end
```

## Gotchas

1. Bare name in statement position auto-calls (zero-arg call). Use `~name` for reference.
2. `[` after name with no space = index. With space = array argument. `a[0]` vs `print [1 2]`.
3. Map keys are symbols only. `m.set :key val` not `m.set "key" val`.
4. For-loop variable is shared (Python-style), not per-iteration captured.
5. Integer division truncates: `(7 / 2)` = `3`. Use `(7.0 / 2)` for `3.5`.
6. `0` is truthy. Only `nil` and `false` are falsy.
7. Keywords work as field names after `.`: `obj.set :key val` is valid (calls map set method).
8. `set a.b.c val` navigates a→b, then sets field c. All intermediate values must be maps.
9. `return` throws internal signal. Only caught at function boundary, not by match/if/for.
10. `set` in a function updates outer variables if they exist. Use `let` for locals that shouldn't clobber outer scope.

## C++ Embedding API

```cpp
finescript::ScriptEngine engine;
finescript::ExecutionContext ctx(engine);

// Register native function
engine.registerFunction("myFunc",
    [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
        return Value::integer(args[0].asInt() + 1);
    });

// Register constant
engine.registerConstant("MAX_HP", Value::integer(100));

// Execute
auto result = engine.executeCommand("myFunc 41", ctx);
// result.success, result.returnValue, result.error

// Context variables
ctx.set("player_name", Value::string("Alice"));

// Load/cache script files
auto* script = engine.loadScript("path/to/script.fsc");
auto result = engine.execute(*script, ctx);

// Call a script closure or native function from C++
Value result = engine.callFunction(closureValue, {Value::integer(42)}, ctx);

// Event handlers (after script registers them with `on`)
for (auto& handler : ctx.eventHandlers()) {
    // handler.eventSymbol, handler.handlerFunction
    // Invoke: engine.callFunction(handler.handlerFunction, args, ctx);
}

// Custom interner
engine.setInterner(&myInterner);

// Resource finder: resolve logical script names to filesystem paths
// Subclass finescript::ResourceFinder, implement resolve(string_view) → path
engine.setResourceFinder(&myFinder);
// Now `source "blocks/torch"` goes through the finder
```

Library: `libfinescript.so`/`.dylib` (shared). CMake target: `finescript`. Namespace: `finescript`.
