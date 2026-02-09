# finescript LLM Reference

Compact reference for LLMs generating or analyzing finescript code.

## Syntax Model

Prefix-first: `verb arg arg ...`. Three bracket types with distinct roles:
- `{expr}` = evaluate prefix expression, return result
- `(expr)` = infix math/logic
- `[a b c]` = array literal

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
```
Last expression is return value. `return val` for early exit (non-local jump).
Closures capture defining scope by reference.

`~name` prevents auto-calling, returns function reference.

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

`or` `and` `==` `!=` `<` `>` `<=` `>=` `..` `..=` `+` `-` `*` `/` `%` `not` `-`(unary)

int/int division truncates. Mixed int/float promotes to float. `+` concatenates strings.
`and`/`or` short-circuit. `..` exclusive range, `..=` inclusive range (both produce arrays).

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
- `.slice start [end]` `.contains val` `.sort`
- `.map fn` `.filter fn` `.foreach fn`

## Maps

Symbol keys only. Create: `{map :key1 val1 :key2 val2}`.

```
m.field               # dot access
set m.field val       # dot set (including nested: set m.a.b val)
```

Methods: `.get :key` `.set :key val` `.has :key` `.remove :key` `.keys` `.values` `.setMethod :name fn`

## Objects

Maps with methods. `setMethod` marks a function as a method; dot-call auto-injects receiver as first arg (`self`).

```
set obj {map :hp 100}
obj.setMethod :damage fn [self amt] do
    set self.hp (self.hp - amt)
end
obj.damage 10         # self=obj injected automatically
```

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

String: `str_length` `str_concat a b ...` `str_substr s start [len]` `str_find s needle` `str_upper` `str_lower`

Type: `to_int` `to_float` `to_str` `to_bool` `type`

Comparison (prefix): `eq` `ne` `lt` `gt` `le` `ge`

I/O: `print a b ...`

Map: `map :k1 v1 :k2 v2 ...`

## Common Patterns

```
# Factory function (constructor pattern)
fn makeThing [x y] do
    set t {map :x x :y y}
    t.setMethod :move fn [self dx dy] do
        set self.x (self.x + dx)
        set self.y (self.y + dy)
    end
    return t
end

# Counter closure
fn makeCounter [] do
    set n 0
    fn [] do set n (n + 1); n end
end

# Higher-order
set doubled {arr.map fn [x] (x * 2)}
set big {arr.filter fn [x] (x > 10)}

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
```

Library: `libfinescript.so`/`.dylib` (shared). CMake target: `finescript`. Namespace: `finescript`.
