# FineScript Improvements for GUI Scripting

This document identifies language improvements that would significantly benefit
GUI scripting with finegui. Each section shows the problem, a concrete example
from GUI use cases, and a proposed solution.

## Context

finegui's script-driven GUI path uses finescript maps as widget data. Scripts
build widget trees by calling `ui.*` functions that return maps, compose them
into arrays, and mutate them via shared-reference semantics. The renderer reads
from these maps each frame.

Typical pattern:
```finescript
set slider {ui.slider "Volume" 0.0 1.0 0.75 fn [v] do
    set volume v
end}

ui.show {ui.window "Settings" [
    {ui.text "Audio Settings"}
    slider
    {ui.button "Reset" fn [] do
        set volume 0.75
        set slider.value 0.75
    end}
]}
```

The language features below would make these patterns more ergonomic.

---

## 1. Named / Keyword Parameters

**Status:** Not supported. Functions use positional parameters only.

**Problem:** Widget functions often have many optional parameters. With positional
args, callers must pass all preceding args to reach later ones, or use awkward
workarounds.

```finescript
# Current: positional args, must include all to reach on_change
{ui.slider "Label" 0.0 1.0 0.5 fn [v] do ... end}

# Problem: how to specify format without on_change?
# Can't skip: {ui.slider "Label" 0.0 1.0 0.5 ??? "%.1f"}
```

**Proposed syntax:**
```finescript
# Named params after positional args, using :symbol value pairs
{ui.slider "Volume" 0.0 1.0 0.75 :format "%.1f" :on_change fn [v] do ... end}

# Window with optional size and flags
ui.show {ui.window "Settings" children :size [600 400] :flags [:no_resize]}
```

TM: I think this overloads the symbol syntax in an ambiguous way. How do we tell the difference between :symbol as just a function argument vs a named parameter marker? I think we could use some other sigil, like "=symbol", followed by the argument. Function/method calls would start with zero or more positional arguments, and then the first parameter name would switch modes to consuming named arguments. What do we do about arguments that are missing? Do we have a null? Also, =symbol could be used for map building, specifying the key name. 


**Implementation notes:**
- Named params could be collected into a map and passed as the last argument
- Or: the function signature declares which params are named with defaults
- Alternative: convention of passing an options map as last arg (works today,
  but verbose with `{map :key val}` syntax)

TM: In a way, interpreting a parameter list and interpreting a map assignment could be the same mechanism. The difference is that I don't know what positional arguments would have as keys in a map; maybe that's invalid.

**Priority:** High — affects every widget function call.

**Workaround today:** Set fields on the returned map:
```finescript
set win {ui.window "Settings" children}
set win.size [600 400]
set win.flags [:no_resize]
ui.show win
```
This works but prevents single-expression widget trees.

---

## 2. Map Literal Shorthand

**Status:** Maps require `{map :key value ...}` function call syntax.

**Problem:** Widget options maps are extremely common. The `map` function call
adds noise. This is especially painful when passing inline options.

```finescript
# Current: verbose
set style {map :color [1.0 0.0 0.0 1.0] :border true :padding 5}

# Want: shorter
set style #{:color [1.0 0.0 0.0 1.0] :border true :padding 5}
# or
set style {:color [1.0 0.0 0.0 1.0] :border true :padding 5}
```

TM: I suppose if something in braces started with =symbol, we could just interpret that as meaning that the stuff in braces is a map. =symbol and :symbol and even unadorned symbol all associate to the same underlying intern and are all symbols, but they have different semantic meanings in the language (respectively: key, symbol as a data item, and variable/function/method name to be looked up).

**Proposed syntax options:**

Option A: `#{}` literal (Ruby-inspired):
```finescript
set opts #{:size [600 400] :flags [:no_resize]}
```

Option B: Bare `{}` with `:key val` pairs (context-sensitive — if first token
is a symbol, it's a map):
```finescript
set opts {:size [600 400] :flags [:no_resize]}
```

Option C: Just make `{map ...}` shorter by aliasing:
```finescript
set opts {m :size [600 400] :flags [:no_resize]}
```

**Priority:** Medium-High — reduces boilerplate in widget-heavy code. Especially
impactful if named parameters are NOT added (options map becomes the primary
pattern for widget configuration).

---

## 3. Format Specifiers in String Interpolation

**Status:** String interpolation exists (`"Value: {x}"`) but no format control.

**Problem:** GUI widgets frequently display numeric values. ImGui format strings
like `"%.1f"` control decimal places. Scripts need to display values similarly.

```finescript
# Current: no control over decimal places
set label "FPS: {fps}"               # "FPS: 59.99834752"
set label "FPS: {round fps}"         # "FPS: 60" (but is an int now)

# Want: format specifiers
set label "FPS: {fps:.0f}"           # "FPS: 60"
set label "Pos: ({x:.1f}, {y:.1f})"  # "Pos: (3.1, 7.4)"
set label "HP: {hp:>4d}/{max_hp:d}"  # "HP:   85/100"
```

TM: How could we do this without adding more syntax? IIRC, some languages use the mod (%) operator between format string and value. So interpolating {expr} just inserts a value with default formatting. But using % (or is there a better operator?) on a string as the first argument makes it act like a format string. So in our syntax, we'd do {% "format" value}. Or ("format" % value) for infix.

**Proposed syntax:** Python-style format mini-language after `:` in interpolation:
```
{expression:format_spec}
```

Common specifiers for GUI use:
- `.Nf` — N decimal places for floats
- `d` — integer
- `>N` / `<N` — right/left align with padding
- `0N` — zero-pad

**Priority:** Medium — `to_str` and `round` work as workarounds, but format
specifiers are much more readable for numeric display.

TM: Since the format specifier is a value string, we can make the formatting specification as comprehensive as we want. Also, the string could be variable name. So {% string_var value_var} is also good.

---

## 4. Default Parameter Values

**Status:** Not supported. All parameters are required.

**Problem:** Reusable GUI component functions often have optional parameters.
Without defaults, callers must always pass all args or the function must check
for nil.

```finescript
# Current: manual nil checks
fn make_slot [item size] do
    if (size == nil) do set size 48 end
    {ui.image_button "slot" (get_icon item) [size size]}
end

# Want: defaults in signature
fn make_slot [item size=48] do
    {ui.image_button "slot" (get_icon item) [size size]}
end
```

TM: How about we use =symbol again here. A bare symbol is intepreted as a parameter name. But if it has the = sigil, then the next parsing token represents a value, which should probably only be a literal.

**Proposed syntax:**
```finescript
fn name [required optional=default_value] do
    ...
end
```

Default expressions evaluated at call time (not definition time), in the
function's scope.

**Priority:** Medium — makes reusable widget component functions cleaner.
Workaround (nil checks) is functional but noisy.

---

## 5. Null Coalescing Operator

**Status:** Not supported.

**Problem:** Optional widget fields and data lookups frequently need fallback
values. Verbose with if/else.

```finescript
# Current
set name {if (item != nil) item.name "Empty"}

# Or worse
set name "Empty"
if (item != nil) do
    set name item.name
end

# Want
set name (item.name ?? "Empty")
```

**Proposed syntax:**
```finescript
(expr ?? fallback)       # Returns fallback if expr is nil
(expr ?: fallback)       # Returns fallback if expr is falsy (nil or false)
```

**Priority:** Medium — very common pattern in GUI scripts where data may not
be loaded yet. The if-expression workaround is compact but less readable.

TM: I see the need for this. I see no problem with those operators. The examples are in infix format, so the prefix format would be {?? exor fallback} and {?: expr fallback}.

---

## 6. Conditional Expression Improvements

**Status:** `if` works as an expression: `{if cond then_val else_val}`.

**Problem:** Using conditional expressions inside array literals for conditional
widget rendering.

```finescript
# Want: conditionally include a widget in a children array
set children [
    {ui.text "Always shown"}
    {if show_debug {ui.text "Debug info"}}
    {ui.button "OK"}
]
```

If `show_debug` is false, the if-expression returns nil, putting nil in the
array. The renderer must skip nil values (easy fix on the C++ side).

**Questions:**
1. Does `{if cond expr}` (without else) already return nil when false?
   If so, this already works and just needs renderer support.
2. Can `{if cond expr}` appear inside an array literal `[...]`?

If both work, this is a **C++ renderer fix**, not a language change. The
MapRenderer's `renderNode` just needs to skip nil entries in children arrays.

**Priority:** High if it doesn't work today. Conditional rendering is essential
for every non-trivial GUI.

TM: We just need to make sure a missing else returns nil and that array literals can contain expressions.

---

## 7. Spread Operator for Arrays

**Status:** Not supported.

**Problem:** Composing widget children arrays from multiple sources. Common when
building dynamic UIs where sections are conditionally included.

```finescript
set header_widgets [
    {ui.text title}
    {ui.separator}
]
set body_widgets [
    {ui.slider "X" 0.0 1.0 x}
    {ui.slider "Y" 0.0 1.0 y}
]

# Current: must concatenate manually (no concat method?)
set all_widgets header_widgets
for w in body_widgets do
    all_widgets.push w
end

# Want: spread
set all_widgets [...header_widgets ...body_widgets]
```

**Proposed syntax:**
```finescript
[...arr1 elem ...arr2]    # Spread arrays inline
```

**Priority:** Medium — useful for composing widget trees from pieces.
Workaround: `.push` in a loop, or build the full array in one place.

TM: Is this like the splat operator in some languages? Like [element *other_array] flatens? I'm not a big fan of special cases like this. But I do see a gap. We need support for concatenating arrays. The splat or ... or whatever is nice syntactic sugar, but it goes against our syntactic rules by introducing operators. I don't see =symbol or :symbol as an operators on a symbols but rather as whole data items with different meanings. On the other hand ...array or *array or whatever introduces an operator concept that we don't have. Do you have any other ideas? Adding concat support might be enough, though.

---

## 8. Array Comprehensions

**Status:** `.map` exists on arrays. Range literals exist (`0..N`).

**Problem:** Generating widget arrays from ranges or transformations. The `.map`
method works but can be awkward for complex expressions.

```finescript
# Current: works but creates an intermediate range array
set slots {{0..36}.map fn [i] {make_slot i}}

# Or:
set slots []
for i in 0..36 do
    slots.push {make_slot i}
end

# Want: inline comprehension
set slots [for i in 0..36: {make_slot i}]
```

**Questions:**
1. Does `{0..36}` create an array? Or is `0..36` only valid in `for` context?
2. Can you call `.map` on a range directly?

If ranges produce arrays and `.map` works on them, this is already adequate.
The comprehension syntax would just be sugar.

**Priority:** Low if `.map` on ranges works. Medium otherwise.

TM: This is more colon overloading. I can see some value in array comprehensions, but we're not implementing a Python interpreter here either. The map function should indeed take ranges, though.

---

## 9. Destructuring Assignment

**Status:** Not supported.

**Problem:** Extracting multiple fields from maps (e.g., widget state, game
entity data) requires multiple statements.

```finescript
# Current
set x entity.position.x
set y entity.position.y
set z entity.position.z

# Want: map destructuring
let {:x :y :z} entity.position

# Array destructuring
let [r g b] color_array
```

**Priority:** Low — nice syntactic sugar but not blocking anything. The
dot-access pattern works fine.

TM: This doesn't look like new syntax. Currently let and set expect a symbol as the target. We could augment them to accept arrays and maps as targets. The array syntax is fairly obvious, although we'd either have to use [:r :b :g] or have to make sure that the handling of let and set forces the array to not evaluate the arguments as variables. As for the map destructuring, we'd want to use the =symbol syntax. This would construct a map with a bunch of null values, which let/set interprets to mean it needs to extract those keys. Then in both cases, multiple variable assignents are made. BTW, it occurs to me that we could use brackets with =symbol names to make a map instead of braces, if that makes parsing easier. So if brackets contain just values, we get an array, and if brackets contain =symbol entries, then we get a dictionary. Some things would be invalid, like starting with a value but then including an =symbol, whereas =symbol followed by another =symbol does null/nil assignment on the ones that don't have values. Syntax from other languages makes us want to use braces to specify dictionaries, but we're not really bound by that. We need to do whatever makes the most sense here.

---

## 10. Method Chaining Improvements

**Status:** Method calls work (`arr.map`, `arr.filter`, etc.). Chaining works
if methods return the right types.

**Problem:** Complex data transformations for dynamic UI (filtering items,
sorting, transforming for display).

```finescript
# Does this chain work today?
set visible_items {items.filter fn [i] i.visible}
set sorted {visible_items.sort_by fn [a b] (a.name < b.name)}

# Want to chain in one expression:
set display_items {items
    .filter fn [i] i.visible
    .sort_by fn [a b] (a.name < b.name)
    .map fn [i] {make_item_widget i}}
```

**Questions:**
1. Can method calls chain across lines?
2. Does `.sort_by` exist (sort with custom comparator)?

**Priority:** Low — mostly a convenience. Multi-statement version is clear.

TM: I don't even know what to do with this. The dot prefix in these examples is acting like an operator, which we don't have the concept of. And it's also blowing up our function call syntax. How do we call methods on an unnamed return value that is an object? Need to brainstorm. Dont's want to blow up the interpreter with to much junk either. sort_by does sound useful.

---

## Summary: Priority Ranking

| # | Feature | Priority | Impact on GUI Scripts |
|---|---------|----------|----------------------|
| 1 | Named parameters | High | Every widget function call |
| 6 | Conditional expr in arrays | High | Conditional rendering |
| 2 | Map literal shorthand | Medium-High | Options maps, widget config |
| 3 | Format specifiers | Medium | Numeric display |
| 4 | Default parameter values | Medium | Reusable components |
| 5 | Null coalescing | Medium | Optional data handling |
| 7 | Array spread | Medium | Widget array composition |
| 8 | Array comprehensions | Low-Medium | Widget generation |
| 9 | Destructuring | Low | Convenience |
| 10 | Method chaining | Low | Data transformation |

## Interaction with finegui

Several of these have **C++ workarounds** in finegui that reduce urgency:

- **Named params**: Widget maps can have fields set after creation
- **Conditional rendering**: MapRenderer can skip nil children (easy C++ fix)
- **Array spread**: `.push` in loops works

The most impactful language-level change is **named parameters**, since it
enables single-expression widget trees with full configuration — the most
natural pattern for declarative UI.
