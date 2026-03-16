# finescript Optimization Plan

This document catalogs optimization strategies for the finescript interpreter,
roughly ordered by effort and impact. The current architecture is an AST-walking
interpreter with `std::variant`-based Values, refcounted heap objects, interned
symbols, and hash-map-based scopes.

---

## Table of Contents

1. [AST-Level Optimizations](#1-ast-level-optimizations)
2. [Bytecode VM](#2-bytecode-vm)
3. [Lexical Variable Indexing](#3-lexical-variable-indexing)
4. [NaN-Boxing](#4-nan-boxing)
5. [Inline Caching](#5-inline-caching)
6. [Superinstructions and Specialization](#6-superinstructions-and-specialization)
7. [Copy-on-Write Collections](#7-copy-on-write-collections)
8. [Arena and Pool Allocation](#8-arena-and-pool-allocation)
9. [Constant Folding and Dead-Code Elimination](#9-constant-folding-and-dead-code-elimination)
10. [Profile-Guided Optimization](#10-profile-guided-optimization)
11. [JIT Compilation](#11-jit-compilation)

---

## 1. AST-Level Optimizations

**Effort:** Low | **Impact:** Low-Medium | **Prerequisite for:** Bytecode VM

### 1.1 Block Flattening

Braces `{expr}` in finescript create Block AST nodes that wrap single
expressions. Game engine scripts use these heavily for clarity:

```
print {a.length}              # Block(Call(DottedName)) instead of just DottedName
set last {a.pop}              # Block wrapping a method call
({fibonacci (n - 1)} + {fibonacci (n - 2)})  # Blocks inside infix
```

A single-child Block is semantically identical to its child. An AST
simplification pass can flatten these before evaluation or bytecode emission:

- **Single-statement Block** -> replace with child node
- **Nested Blocks** `{{ x }}` -> collapse to innermost
- **Block in argument position** -> unwrap

This reduces tree depth and eliminates unnecessary `evalBlock` recursions in
the tree-walker. More importantly, it simplifies bytecode emission since the
compiler doesn't need to handle redundant nesting.

### 1.2 Tail-Call Detection

Mark Call nodes in tail position (last expression in a function body, last
branch of if/match). Even without full TCO, this enables the bytecode VM to
reuse the current call frame instead of pushing a new one, preventing stack
overflow in recursive scripts.

### 1.3 Constant Expression Tagging

Tag AST nodes that are provably constant (literals, pure arithmetic on
literals) so the evaluator or compiler can skip re-evaluation. This is a
lightweight form of constant folding that doesn't require a full optimization
pass.

---

## 2. Bytecode VM

**Effort:** High | **Impact:** Very High (3-10x) | **Foundation for most other optimizations**

Replace the AST-walking evaluator with a flat bytecode interpreter. This is the
single highest-impact change and the foundation that most other optimizations
build on.

### Why the AST walker is slow

- **Pointer chasing:** Every node is a `shared_ptr<AstNode>` on the heap;
  evaluating `a + b` follows 3+ pointers through unrelated memory.
- **Dispatch overhead:** Each `eval()` call branches on `AstNodeKind` (26+
  cases), then recurses. This is unpredictable for branch predictors.
- **No locality:** The AST is allocated node-by-node; adjacent operations may
  be scattered across memory pages.

### Bytecode design choices

**Stack-based** (simpler, like Python/Lua 4):
- Operands live on an implicit value stack
- Instructions: `PUSH_CONST 0`, `LOAD_LOCAL 3`, `ADD`, `CALL 2`
- Easier to compile, but more instructions per operation

**Register-based** (fewer instructions, like Lua 5+):
- Operands reference numbered registers: `ADD R0 R1 R2`
- Fewer instructions but wider encoding and more complex compiler
- Measured ~20-30% faster than stack-based in Lua benchmarks

**Recommendation:** Start stack-based for simplicity, consider register-based
later if instruction dispatch becomes the bottleneck.

### Core instruction set (sketch)

```
# Literals and variables
LOAD_CONST    idx          # push constant pool entry
LOAD_LOCAL    slot         # push local variable
STORE_LOCAL   slot         # pop and store to local
LOAD_UPVAL    depth slot   # push closed-over variable
STORE_UPVAL   depth slot   # store to closed-over variable
LOAD_GLOBAL   sym          # hash lookup in global scope (fallback)

# Arithmetic and logic
ADD, SUB, MUL, DIV, MOD, NEG
EQ, NE, LT, LE, GT, GE
NOT, AND, OR

# Control flow
JUMP          offset
JUMP_IF_FALSE offset
CALL          nargs
RETURN
RETURN_NIL

# Objects
MAKE_ARRAY    count
MAKE_MAP      count        # count key-value pairs
GET_FIELD     sym
SET_FIELD     sym
GET_INDEX
METHOD_CALL   sym nargs

# Iteration
GET_ITER
ITER_NEXT     exit_offset
RANGE         inclusive?

# Special
MAKE_CLOSURE  func_idx
POP
DUP
```

### Compilation pipeline

```
Source -> Tokens -> AST -> [AST simplification] -> Bytecode + ConstPool
```

The compiler walks the AST once, emitting bytecodes into a flat `vector<uint8_t>`
(or `vector<uint32_t>` for wider instructions). Each function/closure gets its
own chunk. A constant pool holds literals, interned symbol IDs, and nested
function prototypes.

### Preserving source locations

Maintain a parallel `vector<SourceLocation>` (or run-length-encoded line table)
indexed by bytecode offset. This preserves error messages with file+line+column
without polluting the hot instruction stream.

---

## 3. Lexical Variable Indexing

**Effort:** Medium | **Impact:** High | **Requires:** Bytecode VM (or significant evaluator rework)

### Current cost

Every variable read does: `scope->get(symbol)` which hashes `uint32_t`, probes
an `unordered_map`, and on miss walks to the parent scope. In a hot loop with
`n` variables at depth `d`, this is `O(n * d)` hash lookups per iteration.

### The optimization

During compilation, resolve each variable to `(frame_depth, slot_index)`:

```
fn outer [x] do            # x -> (0, 0)
    set y (x + 1)          # y -> (0, 1)
    fn inner [z] do         # z -> (0, 0), x -> (1, 0), y -> (1, 1)
        (x + y + z)
    end
end
```

Variable access becomes a direct array index: `frames[depth].locals[slot]`.
No hashing, no map probing, no pointer chasing through scope chains.

### Closures become flat upvalue arrays

Instead of capturing entire scope maps, closures capture only the variables they
reference, stored as a flat array of upvalue slots. This is the Lua upvalue
model and dramatically reduces closure allocation cost.

### Interaction with `set` semantics

finescript's `set` has Python-style scoping (assigns to enclosing scope if
variable exists there). The compiler must do a static scope analysis pass to
determine where each `set` target resolves, which is straightforward for lexical
scoping.

The `global` ProxyMap and dynamic `source` imports may require a fallback to
hash-based lookup for names that can't be statically resolved.

---

## 4. NaN-Boxing

**Effort:** Medium | **Impact:** Medium-High | **Independent of bytecode**

### Current cost

`Value` is a `std::variant` with 10+ alternatives. On typical implementations
this is 16-48 bytes (discriminator + largest member + padding). Every value
copy, every comparison, every function argument involves copying and branching
on this variant.

### The optimization

IEEE 754 doubles have a huge NaN space: any double with all exponent bits set
and a non-zero mantissa is NaN. This leaves ~2^51 bit patterns unused. We can
encode the type tag and a 48-bit payload into a single 8-byte double:

```
Regular double:    used as-is (when not NaN)
Integer:           NaN + tag bits + 32-bit int (or 48-bit with tricks)
Boolean:           NaN + tag bits + 0/1
Nil:               NaN + tag bits + 0
Symbol:            NaN + tag bits + 32-bit intern ID
Heap pointer:      NaN + tag bits + 48-bit pointer (x86-64 uses 48 bits)
```

Heap pointers cover strings, arrays, maps, closures — anything that doesn't
fit in 48 bits.

### Benefits

- `Value` drops from ~40 bytes to 8 bytes
- Arrays become contiguous `double[]` with excellent cache behavior
- Arithmetic on doubles is a no-op (already in the right format)
- Integer arithmetic: unmask, operate, re-tag (a few bitwise ops)
- Passing values to/from functions is a single register move

### Tradeoffs

- Integer range limited to 32 bits (or 48 with more complex encoding) unless
  large integers spill to heap. Current `int64_t` range would need a fallback.
- More complex, lower-level code; harder to debug
- Platform-dependent assumptions about pointer width

### Alternative: tagged pointer

If NaN-boxing's integer range is too limiting, a tagged-pointer scheme uses the
low 3 bits of aligned pointers (or high bits on x86-64) for type tags, with
small integers stored inline. Less elegant than NaN-boxing but avoids the
integer range limitation.

---

## 5. Inline Caching

**Effort:** Medium | **Impact:** Medium-High | **Requires:** Bytecode VM

### Current cost

Method dispatch in `dispatchBuiltinMethod` is a linear chain of
`if (methodSym == sym_foo_)` comparisons — up to ~15 checks for arrays. Map
field access goes through `MapData::get()` which hashes the symbol key. Both
happen on every call, even when the same call site always sees the same type.

### Monomorphic inline caches

Each `METHOD_CALL` or `GET_FIELD` bytecode site gets a small cache slot:

```cpp
struct InlineCache {
    ValueType cachedType;    // last seen object type
    void* cachedHandler;     // function pointer or method entry
};
```

On hit (same type as last time), jump directly to the handler. On miss,
do the full lookup and update the cache. In practice, most call sites are
monomorphic (always see the same type), so the hit rate is very high.

### Polymorphic inline caches (PICs)

For call sites that see 2-3 types (e.g., a function that operates on both
arrays and maps), use a small array of (type, handler) pairs. Fall back to
full dispatch only for megamorphic sites.

### Hidden classes for maps

If map field access becomes a bottleneck, maps with the same set of keys can
share a "shape" or "hidden class" that maps key symbols to fixed slot offsets.
This turns `map.get(sym)` from a hash lookup into an array index — but adds
complexity for shape transitions when keys are added or removed.

---

## 6. Superinstructions and Specialization

**Effort:** Low (once bytecode exists) | **Impact:** Medium

### Fused instruction sequences

Profile common bytecode patterns and fuse them into single instructions that
avoid intermediate stack operations:

```
LOAD_LOCAL + LOAD_CONST + ADD + STORE_LOCAL  ->  ADD_LOCAL_CONST slot const
LOAD_LOCAL + LOAD_LOCAL + LT + JUMP_IF_FALSE ->  BRANCH_LT_LOCAL slot slot offset
LOAD_LOCAL + LOAD_CONST + LT + JUMP_IF_FALSE ->  LOOP_BOUND_CHECK slot const offset
LOAD_LOCAL + GET_FIELD + CALL               ->  METHOD_CALL_LOCAL slot sym nargs
```

### Type-specialized opcodes

When the compiler or inline cache knows the operand types:

```
ADD_INT_INT       # skip type checking, no promotion
ADD_FLOAT_FLOAT   # direct double addition
CONCAT_STR        # string concatenation without type dispatch
ARRAY_PUSH        # direct vector::push_back without method lookup
```

These avoid the overhead of checking types at runtime for operations that are
statically or dynamically known to be type-stable.

---

## 7. Copy-on-Write Collections

**Effort:** Medium | **Impact:** Medium

### Current behavior

Arrays and maps are refcounted. Multiple references to the same array share
the underlying storage, and mutations are visible through all references
(Python semantics). Operations like `.map`, `.filter`, and `.slice` allocate
new arrays.

### The optimization

Add a COW flag to array/map storage. When refcount > 1, mutation triggers a
copy of the underlying storage before modifying. This enables:

- **Cheap immutable copies:** Passing arrays to functions or storing snapshots
  costs O(1) instead of O(n).
- **Safe functional patterns:** `.map`/`.filter` can reuse storage when the
  source is about to be discarded.
- **Structural sharing for maps:** Persistent/immutable map updates share
  unchanged subtrees.

### Tradeoff

Adds a branch on every mutation (`if (refcount > 1) copy()`). For code that
mutates arrays in tight loops through a single reference, this is pure
overhead — but the branch is highly predictable (almost always not-taken for
single-owner arrays).

---

## 8. Arena and Pool Allocation

**Effort:** Medium | **Impact:** Medium

### Current cost

Every Value heap object (string, array, map, closure), every scope, and every
AST node is individually heap-allocated via `new`/`make_shared`. The general
allocator has per-allocation overhead (headers, fragmentation, thread
synchronization) and scattered memory layout.

### Per-context arena allocator

Allocate a large block up front for each `ExecutionContext`. Short-lived
allocations (intermediate values, scope frames, temporary arrays) bump a
pointer into the arena and are freed in bulk when the context completes.

```
Arena: [scope1|vals...|scope2|vals...|closure|vals...|FREE            ]
                                                      ^bump pointer
```

### Pool allocator for common sizes

Value heap objects tend to cluster around a few sizes (small strings, small
arrays, closures with 0-3 upvalues). A pool allocator for each size class
eliminates fragmentation and makes allocation/deallocation O(1).

### Benefits for game scripting

Game scripts often run per-frame or per-event with bounded lifetimes. Arena
allocation turns cleanup into a single pointer reset instead of hundreds of
individual deallocations. This also improves cache locality since objects
allocated together tend to be accessed together.

---

## 9. Constant Folding and Dead-Code Elimination

**Effort:** Low-Medium | **Impact:** Low-Medium | **Requires:** Compilation pass

### Constant folding

Evaluate pure expressions at compile time:

```
# Before
set x (2 + 3)         # LOAD_CONST 2, LOAD_CONST 3, ADD, STORE_LOCAL
set y (x * 0)         # ... can reduce to 0 if x is provably const

# After
set x 5               # LOAD_CONST 5, STORE_LOCAL
set y 0               # LOAD_CONST 0, STORE_LOCAL
```

Applicable to: arithmetic on literals, string concatenation of literals,
boolean short-circuit with constant operands, known-length array/map literals.

### Dead-code elimination

Remove code that provably has no effect:

```
if false do
    # entire block eliminated
end

set x 5
set x 10              # first set is dead (no intervening read)
```

### Unreachable code after return

```
fn foo [x] do
    return 42
    print "never"      # eliminated
end
```

### Caveat

finescript's `source` (dynamic file inclusion) and `global` (ProxyMap-backed
scope access) limit how much the compiler can prove statically. Conservative
analysis should only fold/eliminate when no side effects or dynamic lookups
are possible.

---

## 10. Profile-Guided Optimization

**Effort:** Medium | **Impact:** High (cumulative) | **Ongoing**

As the game engine accumulates more scripts, runtime profiling data becomes a
powerful optimization driver. This is a meta-strategy that informs all other
optimizations.

### 10.1 Instruction-Level Profiling

Instrument the bytecode VM to count:

- **Opcode frequencies:** Which instructions dominate? If 60% of executed
  instructions are LOAD_LOCAL and ADD, those deserve the fastest paths.
- **Type frequencies per instruction:** If ADD is 90% int+int, the default
  path should be the integer fast path.
- **Branch taken/not-taken ratios:** Informs branch layout for CPU prediction.

### 10.2 Hot Loop Detection

Count backward-jump executions to identify hot loops. These are candidates for:

- Superinstruction fusion
- Type specialization (if the loop is type-stable)
- JIT compilation (if implemented)

### 10.3 Call-Site Profiling

Track (caller, callee, argument types) at each call site:

- **Monomorphic sites** (>95% same callee) -> inline caching
- **Polymorphic sites** (2-4 callees) -> PIC
- **Megamorphic sites** -> fall back to hash dispatch

### 10.4 Memory Allocation Profiling

Track:

- Object sizes and lifetimes (are most objects short-lived? -> arena)
- Array size distributions (most arrays < 8 elements? -> small-array optimization)
- Map key-set patterns (most maps have the same shape? -> hidden classes)

### 10.5 Script-Corpus Analysis

Statically analyze the full body of game scripts to identify:

- **Common code patterns** for superinstruction design
- **Typical function sizes** to tune inlining thresholds
- **Variable usage patterns** for register allocation heuristics
- **Brace usage patterns** and their impact on AST depth (informing how
  aggressively to flatten)
- **Method call frequency distribution** to order dispatch chains

This analysis can run offline and feed back into compiler and VM tuning.

### 10.6 Implementation

A lightweight profiling mode adds counters to the VM loop with minimal
overhead (~5-15%). Profiling data is written to a file and consumed by
offline analysis tools or by the compiler in a second pass.

```cpp
struct ProfilingData {
    uint64_t opcodeCount[NUM_OPCODES];
    std::unordered_map<uint32_t, TypeProfile> callSiteTypes;
    std::unordered_map<uint32_t, uint64_t> loopIterations;
};
```

---

## 11. JIT Compilation

**Effort:** Very High | **Impact:** Very High (10-50x for hot code)

### 11.1 Baseline JIT

The simplest JIT: mechanically translate each bytecode instruction to a few
native instructions without optimization. This eliminates dispatch overhead
(the `switch` or computed-goto) which can account for 15-30% of execution
time in a bytecode VM.

- Each opcode maps to a fixed template of machine instructions
- No register allocation; values live on the native stack mirroring the VM
  stack
- Compilation is fast (linear pass) so it can run eagerly
- Libraries: **asmjit** (x86-64/ARM64), **DynASM** (used by LuaJIT)

### 11.2 Method JIT

Compile entire functions with optimizations:

- Register allocation (values in CPU registers instead of memory stack)
- Instruction scheduling
- Inlining of small callees
- Loop-invariant code motion
- Type specialization based on profiling data

More complex but produces significantly better code for hot functions.
Libraries: **LLVM ORC JIT**, **Cranelift** (Rust, but C API exists).

### 11.3 Tracing JIT

Record execution traces through hot loops, including across function calls
and branches, then compile the trace to native code with guards for type
assumptions:

```
# Trace of a hot loop iteration:
LOAD_LOCAL 0       -> int         # guard: slot 0 is int
LOAD_CONST 1       -> int
ADD_INT_INT                       # specialized
STORE_LOCAL 0
LOAD_LOCAL 0
LOAD_LOCAL 2       -> int         # guard: slot 2 is int
LT_INT_INT                        # specialized
JUMP_IF_FALSE -> exit trace
```

If a guard fails (unexpected type), the trace deoptimizes back to the
interpreter. This is LuaJIT's approach and achieves near-C performance for
type-stable numeric loops.

### 11.4 Considerations for finescript

- **Target architectures:** x86-64 (desktop/server) and ARM64 (mobile, Apple
  Silicon). asmjit supports both.
- **Warm-up cost:** JIT compilation takes time; short-lived scripts (one-shot
  commands) won't benefit. Use profiling counters to only JIT functions that
  execute above a threshold.
- **Memory overhead:** JIT code occupies executable memory. For a game with
  many small scripts, this could add up. Consider evicting cold compiled code.
- **Debugging:** JIT-compiled code is harder to debug and profile. Maintain
  the ability to fall back to the interpreter for development/debugging.

---

## Recommended Phasing

### Phase 1: Foundation (enables everything else)
1. AST simplification pass (block flattening, constant tagging)
2. Bytecode VM (stack-based)
3. Lexical variable indexing

### Phase 2: Value representation
4. NaN-boxing (or tagged pointer)

### Phase 3: Dispatch optimization
5. Inline caching for method calls and field access
6. Superinstructions for common patterns

### Phase 4: Memory optimization
7. Copy-on-write for arrays/maps
8. Arena allocation for per-context objects

### Phase 5: Compile-time optimization
9. Constant folding and dead-code elimination

### Phase 6: Data-driven tuning
10. Profiling infrastructure and corpus analysis

### Phase 7: Native code (if needed)
11. Baseline JIT, then tracing/method JIT
