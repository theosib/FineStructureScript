# Game Scripting Language — Design Document

## 1. Overview

A custom scripting language for a voxel game engine, serving as the **single
scripting layer** for all programmable aspects of the game:

- **Game logic** — entity behavior, block interactions, crafting rules, events
- **World generation** — biome rules, structure placement, cave carving
- **UI specification** — menu layouts, HUD elements, dialog trees
- **User commands** — in-game console commands, chat commands, admin tools

One syntax, one runtime, one mental model.

### Design Principles

- **Prefix-first** — everything is `verb arg arg ...` at its core.
- **Looks like commands** — simple expressions read like `/command` syntax.
  `give player diamond 64` is valid.
- **Gradual complexity** — beginners write one-line commands. Advanced users
  compose functions, define behaviors, write world-gen rules. Same language.
- **Functional core, practical surface** — expression-based and composable,
  but with mutable state because this is a game scripting language, not Haskell.
- **Embeddable** — scripts attach to blocks, entities, world-gen templates,
  and UI definitions. All share the same runtime.
- **Performance boundary** — the scripting language handles logic and
  coordination. Performance-critical code (rendering, physics, chunk meshing)
  stays in C++.

---

## 2. Syntax Overview

### The One Rule

Everything is `verb arg arg ...` in prefix form. A line of code is an
implicit function call. Braces make it explicit.

```
set x 5                    # top-level: verb=set, args=[x, 5]
{set x 5}                  # same thing, explicit braces
print {add x 3}            # nested: add is evaluated first, result passed to print
```

### Delimiters

| Syntax | Purpose | Example |
|--------|---------|---------|
| `{...}` | Explicit prefix expression | `{add x 3}` |
| `(...)` | Infix math expression | `(x * 2 + 1)` |
| `[...]` | Array literal | `[1 2 3]` |
| `do...end` | Multi-line block | `do ... end` |
| `.` | Member access / method call | `player.health` |
| `:name` | Symbol literal | `:stone` |
| `"..."` | String literal (with interpolation) | `"Hello {name}"` |

### Statement Separation

Newlines and semicolons both separate statements:

```
# Newline-separated
set x 5
set y 10

# Semicolon-separated (one line)
set x 5; set y 10

# Brace-delimited (one line)
{set x 5} {set y 10}

# Inside braces, semicolons separate multiple statements
{set x 5; set y 10; print (x + y)}
```

**Ambiguity rule**: a line starting with a bare word is ONE statement that
consumes the rest of the line as arguments. A line starting with `{` is one
or more brace-delimited expressions.

```
verb {a} {b}         # ONE call: verb with two args (the results of a and b)
{verb a} {verb b}    # TWO calls: verb(a), then verb(b)
```

---

## 3. Data Types

### Primitives

| Type | Syntax | Examples |
|------|--------|---------|
| Integer | digit sequence | `42`, `-7`, `0` |
| Float | digits with decimal point | `3.14`, `-0.5` |
| String | double-quoted | `"hello"`, `"coords: {x},{y}"` |
| Symbol | colon-prefixed, or unbound bare word | `:stone`, `:interact` |
| Boolean | reserved words | `true`, `false` |
| Nil | reserved word | `nil` |

### Compound Types

| Type | Syntax | Examples |
|------|--------|---------|
| Array | brackets | `[1 2 3]`, `[:a :b :c]` |
| Map/Dictionary | built via `map` verb | `{map :x 10 :y 20}` |
| Function | built via `fn` | `fn [x] (x * 2)` |
| Range | `..` or `..=` | `0..10`, `0..=10` |

### Symbols

Symbols are interned identifiers. They are the fundamental name type in the
language. Every bare word in source code is a symbol that gets looked up in
the current scope.

- `:name` — a symbol literal. Always evaluates to the symbol itself, never
  looked up as a variable. Used for map keys, event names, type tags, etc.
- `name` — a bare word. Looked up in scope. If bound, evaluates to the bound
  value. If unbound, evaluates to `nil`.

The plus sign `+`, any identifier, any string of characters not starting with
a digit — these are all symbols at the token level. The evaluator resolves
them by scope lookup.

```
set x 5
print x           # looks up x, finds 5, prints 5
print y           # looks up y, not bound, prints nil
print :x          # prints the symbol :x itself, no lookup
```

The colon is the quoting mechanism. It says "I mean this symbol literally,
don't look it up."

### Symbols as Strings

Symbols can be used as strings where needed. The engine interns all symbols
for efficient comparison. When a string representation is needed (display,
concatenation), the symbol's name is used.

---

## 4. Expressions and Evaluation

### Prefix Expressions

The default evaluation mode. The first element is the verb (looked up and
called as a function), the remaining elements are arguments.

```
add 3 4                   # → 7
print "hello"             # → prints "hello"
setblock 10 64 20 stone   # → sets block at (10,64,20) to stone
```

### Infix Expressions (Parentheses)

Inside parentheses, standard mathematical infix notation with operator
precedence:

```
(x + 5)
(x * 2 + y / 3)
(health > 0)
(a == b)
(x >= lo and x <= hi)     # logical operators
```

**Operators** (standard precedence, highest to lowest):

| Precedence | Operators | Description |
|-----------|-----------|-------------|
| 1 | `not` | Logical not (unary prefix) |
| 2 | `*`, `/`, `%` | Multiplication, division, modulo |
| 3 | `+`, `-` | Addition, subtraction |
| 4 | `..`, `..=` | Range construction |
| 5 | `<`, `>`, `<=`, `>=` | Comparison |
| 6 | `==`, `!=` | Equality |
| 7 | `and` | Logical and |
| 8 | `or` | Logical or |

Infix expressions can contain nested prefix expressions:

```
({getHeight x z} + 5)
(player.health * {difficulty_multiplier})
```

### Nested Prefix Expressions (Braces)

Braces evaluate a prefix expression and return the result:

```
print {add 3 4}                     # prints 7
setblock x {getHeight x z} z stone  # nested call in argument position
```

Multiple statements inside braces are separated by semicolons:

```
{set x 5; set y 10; add x y}       # evaluates to 15
```

### Blocks (do...end)

Multi-line version of brace-delimited blocks. Statements separated by
newlines. Returns the value of the last statement.

```
do
    set x 5
    set y 10
    add x y
end
# evaluates to 15
```

`do...end` is equivalent to `{stmt; stmt; stmt}` but more readable for
longer code. `do` is itself a macro — it evaluates its sub-expressions
sequentially in a shared scope and returns the last value.

The equivalence:

```
do                         # is equivalent to:
    set x 5                # {do {set x 5} {set y (x * 2)} {print y}}
    set y (x * 2)
    print y
end
```

---

## 5. Variable Binding

### `set` — Mutable Binding

`set` is the only binding form. All bindings are mutable.

```
set x 5
set x (x + 1)         # rebind x to 6
set name "Steve"
set items [1 2 3]
```

`set` is a macro: its first argument (the name) is not evaluated. Its second
argument is evaluated and bound to the name.

### Scoping Rules

**Python-style scoping**: `set` on an existing name updates it in the nearest
enclosing scope where the name exists. `set` on a new name creates the
binding in the current scope.

```
set x 10
if (x > 5) do
    set y (x + 1)      # y is new → created in this block's scope
    set x 20            # x exists in outer scope → updated there
end
print x                 # 20 (was updated inside the if)
print y                 # nil (y was local to the if block)
```

### Scope Hierarchy

From innermost to outermost:

1. **Local scope** — inside `do...end` blocks, function bodies, etc.
2. **Function scope** — the function's own scope
3. **Enclosing scopes** — for closures (lexical chain)
4. **Script scope** — top-level of the script file (persists for the
   lifetime of the script's attachment)
5. **Context scope** — engine-injected variables (player, world, target, etc.)
6. **Global scope** — built-in functions, engine-registered names

---

## 6. Functions

### Definition

`fn` is a macro that creates a function. It takes an optional name, a
parameter list (array of symbols), and a body.

```
# Named function, one-line body
fn double [x] (x * 2)

# Named function, multi-line body
fn clamp [x lo hi] do
    if (x < lo) do return lo end
    if (x > hi) do return hi end
    return x
end

# Anonymous function (no name)
fn [x] (x * 2)

# No parameters
fn greet [] do
    print "hello"
end
```

The parameter list is always an array `[...]`. Even for zero parameters,
use `[]`.

`fn` is a macro: none of its arguments are evaluated. The name is stored as
a symbol, the parameter array provides parameter names, and the body is
stored as unevaluated AST.

### Calling Functions

Functions are called by placing them in verb position (first on a line, or
first inside `{...}`):

```
double 5                     # → 10
print {double 5}             # → prints 10
set result {clamp x 0 100}
```

### Closures

Functions capture the lexical environment where they are defined. Free
variables are resolved via the scope chain at call time.

```
set multiplier 3
fn scale [x] (x * multiplier)

print {scale 5}              # → 15

# Closure over mutable state
fn make_counter [start] do
    set n start
    fn [] do
        set n (n + 1)
        return n
    end
end

set counter {make_counter 0}
print {counter}              # → 1
print {counter}              # → 2
print {counter}              # → 3
```

The closure captures a reference to the defining scope, not a copy. Changes
to captured variables are visible to the closure (and vice versa).

### Higher-Order Functions

Functions are values. They can be passed as arguments, stored in variables,
and returned from other functions.

```
fn apply [f x] {f x}
fn double [x] (x * 2)

print {apply double 5}       # → 10

# Anonymous function as argument
print {apply fn [x] (x + 1) 5}   # → 6
```

### Return

`return` is a special case among the language's built-in forms. Unlike
macros, it **does** evaluate its argument. Unlike regular functions, it
causes **non-local control flow** — it escapes the current scope and returns
from the enclosing function.

```
fn find_first [arr predicate] do
    for item in arr do
        if {predicate item} do
            return item        # evaluates item, then exits find_first
        end
    end
    return nil
end
```

`return` with no argument returns `nil`:

```
fn maybe_print [x] do
    if (x == nil) do return end    # return nil, exit function
    print x
end
```

Without an explicit `return`, a function returns the value of its last
expression.

**Implementation note**: `return` is implemented internally as a thrown
signal/exception that is caught at the function call boundary. The signal
carries the evaluated return value.

---

## 7. Control Flow

All control flow constructs are macros. They have special evaluation rules
for their arguments (conditions evaluated, branches deferred until selected).

### if / elif / else

```
if (x > 10) do
    print :big
elif (x > 0) do
    print :small
else do
    print :non_positive
end
```

One-line form (body is a single expression in braces):

```
if (x > 5) {print :big}
if (x > 5) {print :big} {print :small}    # with else (second arg is else-branch)
```

`if` evaluates the condition. If truthy, evaluates the then-branch. The
`elif` and `else` clauses are part of the `if` macro's syntax.

### for

Iterates over a range or collection.

```
# Range iteration
for i in 0..10 do
    print i                  # prints 0 through 9
end

for i in 0..=10 do
    print i                  # prints 0 through 10
end

# Collection iteration
for item in {inventory player} do
    print item.name
end

# Nested loops
for x in 0..16 do
    for z in 0..16 do
        setblock x 64 z stone
    end
end
```

`for` is a macro: the loop variable name is not evaluated (it's a binding
target, like `set`'s first argument). The iterable expression is evaluated
once. The body is evaluated once per iteration.

### while

```
while (health > 0) do
    set health (health - {damage_per_tick})
end
```

`while` re-evaluates both the condition and the body each iteration.

### match

Pattern matching against values.

```
match {getBlock x y z}
    :air do
        setblock x y z :stone
    end
    :water do
        setblock x y z :ice
    end
    _ do
        noop
    end
end
```

`match` evaluates the scrutinee (first argument), then compares against each
pattern in order. `_` is the wildcard / default pattern. The body of the
first matching pattern is evaluated.

---

## 8. Dot Notation and Objects

### Field Access

Dot notation looks up a field in a map/object. Left-to-right evaluation.

```
player.health              # look up :health in player → a number
player.inv.size            # look up :inv in player → object, then :size → a number
```

### Method Calls

If a dot-accessed field is a function AND it's in verb position, the object
is automatically passed as the first argument (`self`):

```
player.damage 10           # looks up :damage in player, calls it as damage(player, 10)
player.inv.add :diamond 5  # inv.add(player.inv, :diamond, 5)
```

This is the only OO mechanism. Methods are just functions stored in a map
that take `self` as their first parameter.

### The Rule

| Position | Value is data | Value is a function |
|----------|--------------|---------------------|
| **Verb** (first on line or in `{}`) | Error: not callable | Called with remaining args; object auto-passed as self |
| **Argument** | Passed as value | Passed as function value (no self injection) |

```
player.damage 10                 # verb position → called with self=player, amount=10
print player.health              # argument position, data → value passed to print
set f player.damage              # argument position, function → raw function stored in f
apply_to_all items player.damage # argument position → function passed as callback
```

### Distinguishing Methods from Data

When storing functions in a map, use `setMethod` to mark them as methods
(receive `self` on dot-call). Use `set` for plain data fields, including
function values that should NOT receive `self`.

```
set obj {map}
obj.set :name "button"                          # data field
obj.set :on_click fn [event] do print event end # stored function, NOT a method
obj.setMethod :describe fn [self] do            # method — dot-call injects self
    print self.name
end

obj.describe                   # self = obj, prints "button"
set f {obj.get :on_click}     # get the raw function
{f :click_event}               # call without self
```

### Objects Are Just Dictionaries

There is no class system. An "object" is a map that happens to have some
method entries. A "class" is a factory function that builds such maps.

```
fn make_enemy [hp speed] do
    set e {map}
    e.set :health hp
    e.set :speed speed
    e.set :alive true
    e.setMethod :damage fn [self amount] do
        set self.health (self.health - amount)
        if (self.health <= 0) do
            set self.alive false
        end
    end
    e.setMethod :is_alive fn [self] do
        return self.alive
    end
    return e
end

set goblin {make_enemy 50 1.2}
set dragon {make_enemy 500 0.8}

goblin.damage 10
print goblin.health              # 40
print {dragon.is_alive}          # true
```

No inheritance. No private members. Composition over inheritance — add fields
and methods to any map at any time. When reuse is needed, call another
factory function or copy fields from a template.

---

## 9. Arrays and Maps

### Arrays

Arrays are ordered, indexable collections. Created with bracket syntax.
Elements are space-separated (commas are optional whitespace).

```
set nums [1 2 3 4 5]
set names ["Alice" "Bob" "Carol"]
set mixed [1 "hello" :symbol [10 20]]   # heterogeneous, nested
set empty []
```

Array contents are evaluated: bare words are looked up, expressions computed.

```
set x 10
set y 20
set coords [x y (x + y)]        # → [10, 20, 30]
```

#### Array Operations

```
set a [1 2 3]
print a[0]                       # indexing → 1
print {a.length}                 # length → 3
a.push 4                         # append → [1 2 3 4]
set last {a.pop}                 # remove last → 4, array is [1 2 3]
for item in a do print item end  # iteration
```

### Maps

Maps are key-value collections. Created with the `map` verb, which takes
alternating key-value pairs.

```
set m {map :x 10 :y 20 :z 30}
print {m.get :x}                 # 10
m.set :w 40                      # add/update entry
m.remove :z                      # remove entry
print {m.has :x}                 # true
```

#### Map Iteration

```
for key in {m.keys} do
    print key {m.get key}
end
```

### Dictionaries and Objects Are the Same

A map with method entries (via `setMethod`) is an "object." A map without
method entries is a "dictionary." There is no structural difference. The
only distinction is whether fields are marked as methods for self-injection
during dot-call.

---

## 10. Strings

### String Literals

Double-quoted. Support escape sequences and interpolation.

```
"Hello, world!"
"Line one\nLine two"
"Tab\there"
"A quote: \"inside\""
```

### String Interpolation

Expressions inside `{...}` within a string are evaluated and their results
are inserted as text. This uses the same brace syntax as the rest of the
language.

```
set name "Steve"
set hp 20
print "Hello {name}, you have {hp} HP"
# → Hello Steve, you have 20 HP

print "Position: {player.x}, {player.y}, {player.z}"
print "Result: {add 3 4}"
print "Health: {clamp player.health 0 100}%"
```

To include a literal brace in a string, escape it:

```
"Use \{braces\} for expressions"
# → Use {braces} for expressions
```

---

## 11. Ranges

Ranges represent a sequence of integers. Two forms:

| Syntax | Meaning | Example |
|--------|---------|---------|
| `a..b` | Exclusive (a up to b-1) | `0..5` → 0,1,2,3,4 |
| `a..=b` | Inclusive (a up to b) | `0..=5` → 0,1,2,3,4,5 |

Ranges are used primarily in `for` loops and can be constructed as values:

```
for i in 0..10 do print i end

set r (0..=100)
for i in r do print i end

# Equivalent prefix forms
set r {.. 0 10}          # exclusive range
set r {..= 0 10}         # inclusive range
```

---

## 12. Events and Hooks

Scripts attached to blocks, entities, or other game objects can register
event handlers using the `on` macro.

```
on :interact do
    print "Block was right-clicked"
    print "By: {player.name}"
    print "At: {target.x}, {target.y}, {target.z}"
end

on :tick do
    set nearby {world.nearest_player self.x self.y self.z 32}
    if (nearby != nil) do
        self.move_toward nearby 0.5
    end
end

on :place do
    print "{player.name} placed a block at {target.x},{target.y},{target.z}"
end

on :break do
    drop_item target.x target.y target.z :cobblestone 1
end
```

`on` is a macro: neither the event name nor the body is evaluated at
definition time. The body is stored as a closure and registered with the
engine's event system. When the event fires, the engine sets up the
execution context (player, target, world, self, etc.) and evaluates the body.

### Context Variables

The engine injects context variables into scope before event handler
execution. The available variables depend on the event type:

| Variable | Description |
|----------|-------------|
| `player` | The player entity that triggered the event |
| `target` | The block position or entity being acted upon |
| `world` | The world object (for block/entity queries) |
| `self` | The block or entity this script is attached to |

These are ordinary variables in scope — no special syntax needed to access
them.

---

## 13. Execution Context

Before any script runs, the engine sets up the execution context by
pre-binding names in the script's scope.

### Engine-Provided Bindings

The engine registers:

- **Block types**: `stone`, `dirt`, `diamond`, etc. → BlockType values
- **Item types**: item identifiers → ItemType values
- **Built-in functions**: `print`, `add`, `sub`, `mul`, `div`, math
  functions, string functions, array functions, map functions, etc.
- **Game API functions**: `setblock`, `getblock`, `give`, `teleport`,
  `spawn`, `play_sound`, `drop_item`, etc.
- **Context objects**: `player`, `world`, `target`, `self` (set per
  execution)

Because these names are pre-bound, simple commands "just work":

```
give player diamond 64          # all names are pre-bound by engine
setblock 10 64 20 stone         # stone is a bound BlockType value
teleport player 100 64 200
```

### Imports

For now, a simple `source` macro that reads and evaluates another script
file in the current scope:

```
source "utils.script"
source "ui_helpers.script"
```

This is equivalent to `source` in bash — the file's definitions become
available in the current scope. The execution context can also pre-load
scripts before the main script runs.

---

## 14. Macros vs Functions — Evaluator Internals

The language has two categories of callable things:

### Functions

All arguments are evaluated to values before the function is called. The
function receives computed values.

```
print {add x 5}
#      ^^^^^^^^^ add receives the VALUES of x and 5
# ^^^^^^^^^^^^^ print receives the result of add
```

All user-defined functions (via `fn`) are functions. All engine-registered
API functions are functions.

### Macros

Arguments are passed as unevaluated AST nodes. The macro's implementation
decides which arguments to evaluate, which to treat as names, and which to
store as code.

| Macro | Evaluation rules |
|-------|-----------------|
| `set` | arg1: name (not evaluated), arg2: evaluated |
| `fn` | name: not evaluated, params: not evaluated, body: stored as code |
| `if` | condition: evaluated, branches: deferred (only matching branch evaluated) |
| `elif` | part of `if` syntax |
| `else` | part of `if` syntax |
| `for` | loop var: name (not evaluated), iterable: evaluated, body: re-evaluated per iteration |
| `while` | condition: re-evaluated each iteration, body: re-evaluated each iteration |
| `match` | scrutinee: evaluated, patterns: not evaluated, bodies: deferred |
| `on` | event name: not evaluated, body: stored as closure for event registration |
| `do` | all sub-expressions: evaluated sequentially in a shared scope |
| `source` | filename: evaluated, file contents: parsed and evaluated in current scope |

### Return

`return` is a special case. It **evaluates** its optional argument (like a
function), then performs a **non-local jump** out of the enclosing function
(like no other construct).

```
return (x + 5)     # evaluates (x + 5), then escapes the function with that value
return              # escapes the function with nil
```

**Implementation**: `return` throws an internal signal/exception carrying the
return value. The function call boundary catches this signal and uses the
carried value as the function's result. This signal is invisible to the
script — it cannot be caught or intercepted by script code.

### User-Defined Macros

Not supported. Only the built-in set listed above are macros. Users define
functions only. This keeps the language simple and the evaluator predictable.

---

## 15. Parser Grammar

The grammar is small enough for a hand-written recursive descent parser.

```
Program     → Statement*
Statement   → Expr (';' Expr)* NEWLINE
            | DoBlock

Expr        → Atom+

Atom        → NUMBER
            | STRING
            | ':' SYMBOL
            | '[' Atom* ']'
            | '(' InfixExpr ')'
            | '{' Statement+ '}'
            | DoBlock
            | DottedName

DottedName  → NAME ('.' NAME)*

DoBlock     → 'do' NEWLINE Statement* 'end'

InfixExpr   → UnaryExpr (BINOP UnaryExpr)*
UnaryExpr   → 'not' UnaryExpr | Atom
BINOP       → '+' | '-' | '*' | '/' | '%'
            | '<' | '>' | '<=' | '>=' | '==' | '!='
            | 'and' | 'or'
            | '..' | '..='
```

### Token Types

| Token | Pattern |
|-------|---------|
| NUMBER | `-?[0-9]+(\.[0-9]+)?` |
| STRING | `"` ... `"` with escape sequences and `{expr}` interpolation |
| SYMBOL | After `:`, any identifier-like sequence |
| NAME | `[a-zA-Z_][a-zA-Z0-9_]*` or operator characters like `+`, `-`, etc. |
| Punctuation | `{`, `}`, `(`, `)`, `[`, `]`, `.`, `:`, `;` |
| Keywords | `do`, `end`, `if`, `elif`, `else`, `for`, `in`, `while`, `match`, `_` |

### Parsing Rules

1. **Top level**: each line is a statement. First token determines the form.
2. **Bare word first**: parse as verb + arguments until end of line or `;`.
3. **`{` first**: parse brace-delimited expression(s). Multiple `{...}` on
   a line are separate statements.
4. **Inside `{...}`**: parse as verb + args. Semicolons separate multiple
   statements.
5. **Inside `(...)`**: switch to infix expression parser with precedence
   climbing.
6. **Inside `[...]`**: parse space-separated atoms as array elements.
7. **`do` keyword**: read lines until matching `end`, wrap as block node.
8. **Dot notation**: after any NAME, if followed by `.`, read the next NAME
   and build a member-access chain.

### AST Node Types

```
Node = NumberLit(value)
     | StringLit(parts: [string | Expr])    # for interpolation
     | SymbolLit(name)
     | BoolLit(value)
     | NilLit
     | ArrayLit(elements: [Node])
     | DottedName(parts: [string])
     | Call(verb: Node, args: [Node])        # verb + arguments
     | Infix(op: string, left: Node, right: Node)
     | Block(statements: [Node])             # do...end or {a; b; c}
     | Name(name: string)                    # bare symbol for lookup
```

---

## 16. Implementation Notes

### Project Name and Namespace

The script engine is called **finescript**. C++ namespace: `finescript`.
Build target: `libfinescript`. File extension: `.fsc` (or `.script`).

### Architecture

```
Source Text → Tokenizer → Parser → AST → Tree-Walk Evaluator
```

An AST-walking interpreter is the initial target. For short scripts (event
handlers, commands, small logic blocks), this is fast enough. If performance
becomes an issue, the AST is a clean target for compilation to bytecode.

### Build

- **C++17** — wide compiler support, no bleeding-edge requirements
- **CMake** build system
- **Catch2** test framework
- No external dependencies beyond the standard library (the engine
  integration headers are optional, not required to build finescript)

### Resolved Implementation Decisions

#### Value Representation

`std::variant` with inline small types and heap-allocated reference-counted
storage for complex types. Variant handles construction/destruction of
`shared_ptr` members correctly — no manual placement new/destroy needed.

```cpp
using ValueVariant = std::variant<
    std::monostate,                          // nil
    bool,                                    // boolean
    int64_t,                                 // integer
    double,                                  // float
    uint32_t,                                // symbol (interned ID)
    std::shared_ptr<std::string>,            // string (heap)
    std::shared_ptr<std::vector<Value>>,     // array (heap)
    std::shared_ptr<MapData>,                // map or proxy (heap)
    std::shared_ptr<Closure>,                // script function (heap)
    std::shared_ptr<NativeFunctionObject>    // native function (heap)
>;
```

The variant's built-in index serves as the type tag. Reference counting
(via `shared_ptr`) handles memory management for heap-allocated values.
No garbage collector needed. Copying a `Value` is cheap for small types
(inline copy) and shared for complex types (refcount bump).

#### Map Keys: Symbols Only

Map keys are **always symbols** (interned `uint32_t` IDs). No string or
integer keys. This means:

- `m.set :name "Alice"` — `:name` is a symbol key
- `m.name` — dot notation looks up symbol `name`
- Maps are essentially symbol-indexed structs
- Hash map keyed by `uint32_t` — fast and simple

#### Integer Division: Truncating

`(3 / 2)` = `1` (integer). Int / int = int, like C.
Use `(3.0 / 2)` or `{float 3} / 2` for float division.
Mixed int/float operations promote to float: `(3 + 1.5)` = `4.5`.

#### For-Loop Capture: Shared (Python-style)

All closures in a loop body share the same loop variable. The last
iteration's value wins:

```
set fns []
for i in 0..5 do
    array.push fns fn [] do return i end
end
# All functions return 4 (last value of i before loop ended)
```

To capture per-iteration values, use an immediately-invoked function or
bind to a local:

```
for i in 0..5 do
    set captured i  # captured is a new binding each time (same scope, but
                    # the closure captures the value at binding time...
                    # actually no — same scope, same variable)
    # Correct pattern: wrap in a function
    array.push fns {fn [x] do return fn [] do return x end end} i
end
```

#### `set` with Dot Notation (Compound Assignment)

`set a.b.c 5` is parsed as a compound set:
1. Evaluate `a` → get map M1
2. Evaluate `M1.b` → get map M2
3. Set key `c` on M2 to `5`

The macro splits the dotted name: evaluate all segments except the last
to navigate to the target map, then set the final segment as a key.

Plain `set x 5` creates or updates `x` in the nearest enclosing scope
(Python-style).

#### `fn` Named vs Anonymous

The `fn` macro inspects its unevaluated arguments to determine form:

- `fn add [a b] (a + b)` — arg[0] is a symbol, arg[1] is an array →
  **named function**. Binds `add` in the current scope.
- `fn [a b] (a + b)` — arg[0] is an array → **anonymous function**.
  Returns the closure as a value.

#### `do...end` Parsing

`do...end` is always parsed as a block node. For macro keywords (`if`,
`for`, `while`, `fn`, `on`), the parser has special-case grammar:

- `if <expr> do <body> end [else do <body> end]`
- `for <name> in <expr> do <body> end`
- `while <expr> do <body> end`
- `fn <name> [<params>] do <body> end`
- `on <event> do <body> end`

The parser recognizes these keywords and knows `do` is a block delimiter.
Standalone `do <body> end` at statement level is the `do` macro
(sequential evaluation in shared scope).

Since there are only ~10 macros, special-casing each is practical and
efficient. User-defined functions have no special syntax.

#### `source` Scoping

`source "lib.script"` executes the sourced file in the **current scope**,
like bash's `source` command. Functions and variables defined in the sourced
file become visible to the sourcer.

#### Error Position Tracking

Every AST node carries source location: file ID (uint16), line (uint16),
column (uint16). ~6 bytes per node overhead, worth it for usable error
messages:

```
[SCRIPT ERROR] sign.script:15:8: cannot call nil as function
```

#### String Interner: Pluggable

The script engine ships with a built-in interner, but exposes an abstract
interface so host applications can provide their own (e.g., the voxel
engine's `StringInterner`). When all components share an interner, script
symbol IDs and engine IDs are interchangeable — no mapping needed.

See `ENGINE_INTEGRATION.md` §4.1 for details.

### Backend Options

The implementation can be from scratch (simple given the small grammar) or
can target an existing runtime:

- **Janet** (MIT license) — Lisp with good types, embeddable C
  implementation, solid VM. Could serve as a compilation target with a
  custom front-end syntax.
- **Lua VM** — well-proven in games, but the language semantics differ
  enough that targeting it adds translation complexity.
- **Custom** — given the small grammar and simple semantics, a hand-written
  tree-walker in C++ is straightforward and gives full control.

### Existing Tool Integration

Parser generators (PEG-based or otherwise) can be used for the tokenizer
and parser stages. The grammar is regular enough for most tools.

### Error Handling

No in-language exception system. When a script encounters an error (type
error, unbound function call, index out of bounds), execution aborts. The
interpreter throws a C++ exception that the engine catches, logs (with
script name and line number), and handles gracefully.

Script authors handle expected conditions with `if` checks, not exceptions:

```
set block {world.getBlock x y z}
if (block != nil) do
    # safe to use block
end
```

### Performance Boundary

The scripting language handles logic and coordination. The engine's C++ code
handles:

- Rendering and GPU operations
- Physics simulation
- Chunk meshing and generation (called into by scripts)
- Network I/O

Scripts call into C++ via registered API functions. Heavy loops (filling
large regions, complex generation) should use batch operations provided by
the engine rather than per-block script calls.

---

## 17. Example Programs

### Simple Commands

```
give player diamond 64
teleport player 100 64 200
setblock 10 64 20 stone
time set 6000
weather clear
```

### Block Interaction Script

```
set open false

on :interact do
    if open do
        set open false
        setblock target.x target.y target.z :door_closed
        play_sound :door_close target.x target.y target.z
    else do
        set open true
        setblock target.x target.y target.z :door_open
        play_sound :door_open target.x target.y target.z
    end
end
```

### World Generation Rule

```
on :generate_surface do
    set biome {world.getBiome target.x target.z}
    if (biome == :desert) do
        if ({random 100} < 3) do
            place_structure :cactus target.x target.y target.z
        end
    elif (biome == :forest) do
        if ({random 100} < 15) do
            place_structure :oak_tree target.x target.y target.z
        end
    end
end
```

### Entity AI Behavior

```
set state :idle
set home_x self.x
set home_z self.z

on :tick do
    set nearest {world.nearest_player self.x self.y self.z 16}

    match state
        :idle do
            if (nearest != nil) do
                set state :chase
            elif ({random 100} < 5) do
                set state :wander
            end
        end
        :chase do
            if (nearest == nil) do
                set state :idle
            else do
                self.move_toward nearest.x nearest.y nearest.z 0.8
                if ({distance self nearest} < 2.0) do
                    nearest.damage 5
                end
            end
        end
        :wander do
            set wx (home_x + {random_range -10 10})
            set wz (home_z + {random_range -10 10})
            self.move_toward wx self.y wz 0.3
            if ({distance_2d self.x self.z wx wz} < 1.0) do
                set state :idle
            end
        end
    end
end
```

### GUI Construction

```
fn make_button [label width on_click] do
    set b {map}
    b.set :type :button
    b.set :label label
    b.set :width width
    b.set :on_click on_click
    return b
end

fn make_panel [children] do
    set p {map}
    p.set :type :panel
    p.set :layout :vertical
    p.set :children children
    p.setMethod :add fn [self child] do
        self.children.push child
    end
    return p
end

set menu {make_panel [
    {make_button "Play" 200 fn [e] do start_game end}
    {make_button "Settings" 200 fn [e] do show_settings end}
    {make_button "Quit" 200 fn [e] do quit_game end}
]}

ui.show menu
```

### Custom Command

```
on :command_home do
    set home {get_player_data player :home}
    if (home == nil) do
        tell player "No home set! Use /sethome first."
        return
    end
    teleport player home.x home.y home.z
    tell player "Welcome home!"
end

on :command_sethome do
    set_player_data player :home {map
        :x player.x
        :y player.y
        :z player.z
    }
    tell player "Home set to {player.x}, {player.y}, {player.z}"
end
```
