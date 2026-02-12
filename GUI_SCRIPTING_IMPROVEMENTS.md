# FineScript Improvements for GUI Scripting

This document tracks language improvements for GUI scripting with finegui.

## Context

finegui's script-driven GUI path uses finescript maps as widget data. Scripts
build widget trees by calling `ui.*` functions that return maps, compose them
into arrays, and mutate them via shared-reference semantics. The renderer reads
from these maps each frame.

Typical pattern (using new syntax):
```finescript
set slider {ui.slider "Volume" 0.0 1.0 0.75 =on_change fn [v] do
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

---

## Implemented Features

### 1. Named / Keyword Parameters --- IMPLEMENTED

Pass arguments by name using `=name value` after positional arguments:

```finescript
fn make_button [label =size 24 =color "white"] do
    {=type :button =label label =size size =color color}
end

make_button "OK" =color "green"    # size defaults to 24
make_button "Cancel" =size 32      # color defaults to "white"
```

Named args are matched to parameter names. They can appear in any order
after positional args.

**Impact on GUI:** Every widget function can now have clean optional params:
```finescript
{ui.slider "Volume" 0.0 1.0 0.75 =format "%.1f" =on_change fn [v] do ... end}
{ui.window "Settings" children =size [600 400] =flags [:no_resize]}
```

---

### 2. Map Literal Syntax `{=key value ...}` --- IMPLEMENTED

Maps can be created with concise literal syntax:

```finescript
# New (preferred)
set widget {=type :button =label "OK" =width 100}

# Old (still works)
set widget {map :type :button :label "OK" :width 100}
```

If the first token after `{` is `=name`, the entire brace expression is
parsed as a map literal.

---

### 3. Default Parameter Values --- IMPLEMENTED

Functions can declare optional parameters with defaults using `=name default`:

```finescript
fn make_slot [item =size 48 =tooltip nil] do
    {=item item =size size =tooltip tooltip}
end

make_slot "sword"             # size=48, tooltip=nil
make_slot "sword" 64          # size=64, tooltip=nil
```

Required params must come before optional params. Defaults are evaluated at
call time.

---

### 4. Null Coalescing `??` and `?:` --- IMPLEMENTED

Short-circuit operators for handling nil/falsy values:

```finescript
set name (player.name ?? "Unknown")    # returns left unless nil
set hp (health ?: 100)                 # returns left unless falsy
```

Both also work as prefix: `{?? expr fallback}`, `{?: expr fallback}`.

---

### 5. String Format Operator `%` --- IMPLEMENTED

Printf-style formatting:

```finescript
set label ("%.1f" % fps)               # "60.0"
set msg ("%d/%d" % [hp max_hp])        # "85/100"
format "%d items at $%.2f" count price # multi-arg prefix form
```

Supported: `%d`, `%i`, `%f`, `%e`, `%g`, `%x`, `%X`, `%o`, `%s`.

Also usable in string interpolation:
```finescript
"Health: {format \"%d/%d\" hp max_hp}"
```

---

### 6. Array Concatenation with `+` --- IMPLEMENTED

```finescript
set all (base_items + extra_items)
set combined ([1 2] + [3 4])           # [1 2 3 4]
```

Creates new array, does not modify operands. This addresses the array
composition need without a spread operator.

---

### 7. `sort_by` Array Method --- IMPLEMENTED

```finescript
set sorted {items.sort_by fn [a b] (a.priority < b.priority)}
```

Sorts in-place and returns the array.

---

## Key Sigils Summary

finescript now has three name sigils:

| Sigil | Meaning | Example |
|-------|---------|---------|
| `name` | Scope lookup (variable/function) | `player`, `print` |
| `:name` | Literal symbol (data, map keys) | `:button`, `:on_click` |
| `=name` | Key specifier (map literals, named params, defaults) | `=size 48` |

---

## Remaining Items

### 8. Conditional Expression in Arrays

**Status:** Needs verification / possible C++ fix.

The pattern:
```finescript
set children [
    {ui.text "Always shown"}
    {if show_debug {ui.text "Debug info"}}
    {ui.button "OK"}
]
```

Requires:
1. `{if cond expr}` without else returns nil --- needs verification
2. Array literals can contain arbitrary expressions --- needs verification
3. MapRenderer skips nil entries in children arrays --- C++ fix in finegui

**Priority:** High for GUI scripting. May be purely a renderer fix.

---

### 9. Destructuring Assignment

**Status:** Not implemented. Low priority.

```finescript
# Want
let [=x =y =z] entity.position    # map destructuring via =key syntax
let [r g b] color_array            # array destructuring
```

Uses `=symbol` syntax consistently with the rest of the language. The
brackets-with-=keys pattern mirrors map literal syntax.

**Priority:** Low — dot-access works fine.

---

### 10. Method Chaining on Return Values

**Status:** Not implemented. Needs design thought.

The problem: calling methods on unnamed return values.
```finescript
# Want (but syntax unclear)
set result {items.filter fn [i] i.visible}
set sorted {result.sort_by fn [a b] (a.name < b.name)}
```

Two-statement version works. Single-expression chaining would need new syntax
that doesn't conflict with existing parsing rules.

**Priority:** Low — multi-statement approach is clear enough.

---

### 11. Array Comprehensions / Range as Array

**Status:** Unclear if `.map` works on ranges.

```finescript
# Does this work?
set slots {{0..36}.map fn [i] {make_slot i}}
```

If ranges produce arrays, this is already solved. If not, `.map` should be
extended to accept ranges. No new syntax needed.

**Priority:** Low if `.map` on ranges works. Medium otherwise.

---

## Summary

| # | Feature | Status | Impact |
|---|---------|--------|--------|
| 1 | Named parameters (`=name`) | DONE | Every widget call |
| 2 | Map literals (`{=key val}`) | DONE | Widget config |
| 3 | Default params (`=name default`) | DONE | Reusable components |
| 4 | Null coalescing (`??`, `?:`) | DONE | Optional data |
| 5 | Format operator (`%`) | DONE | Numeric display |
| 6 | Array concat (`+`) | DONE | Widget array composition |
| 7 | `sort_by` | DONE | Data transformation |
| 8 | Conditional in arrays | Verify | Conditional rendering |
| 9 | Destructuring | Not yet | Low priority |
| 10 | Method chaining | Not yet | Low priority |
| 11 | Range `.map` | Verify | Widget generation |
