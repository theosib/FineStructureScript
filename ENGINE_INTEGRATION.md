# Scripting Engine — FineStructureVoxel Integration Design

## 1. Architecture Overview

```
┌───────────────────────────────────────────────────────┐
│                  FineStructureVoxel                     │
│                                                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
│  │ Module   │  │ Update   │  │ World    │            │
│  │ System   │  │ Scheduler│  │ Gen      │            │
│  │ (Phase 7)│  │ (Phase 9)│  │ (Phase10)│            │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘            │
│       │              │              │                   │
│       ▼              ▼              ▼                   │
│  ┌─────────────────────────────────────────────────┐  │
│  │              ScriptBridge (C++ glue)             │  │
│  │  - ScriptedBlockHandler : BlockHandler          │  │
│  │  - ScriptedEntityBehavior : Entity              │  │
│  │  - ScriptedGenerationPass : GenerationPass      │  │
│  │  - ScriptedCommand                              │  │
│  └──────────────────────┬──────────────────────────┘  │
│                          │                              │
└──────────────────────────┼──────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────┐
│                 Script Engine (separate project)          │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Parser   │  │ AST      │  │ Evaluator            │  │
│  │          │──▶│ Cache    │──▶│                      │  │
│  │          │  │          │  │  Global scope          │  │
│  └──────────┘  └──────────┘  │  + context scope      │  │
│                               │  + native functions   │  │
│                               └──────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

The script engine is a **separate library** that the voxel engine links against.
The voxel engine provides a **bridge layer** that adapts between engine-specific
C++ interfaces (BlockHandler, GenerationPass, etc.) and the script engine's
generic execution model.

---

## 2. Script Engine C++ API

### 2.1 Core Types

```cpp
namespace finescript {

/// Script value — the universal type in the language.
/// Corresponds to: nil, bool, int, float, string, symbol, array, map, function.
class Value {
public:
    // Construction
    static Value nil();
    static Value boolean(bool b);
    static Value integer(int64_t i);
    static Value number(double d);
    static Value string(std::string s);
    static Value symbol(uint32_t internedId);
    static Value array(std::vector<Value> elems);
    static Value map();
    static Value proxyMap(std::shared_ptr<ProxyMap> proxy);
    static Value nativeFunction(std::shared_ptr<NativeFunctionObject> func);
    static Value function(/* closure representation */);

    // Type checks
    bool isNil() const;
    bool isBool() const;
    bool isInt() const;
    bool isNumber() const;
    bool isString() const;
    bool isSymbol() const;
    bool isArray() const;
    bool isMap() const;
    bool isCallable() const;

    // Access (these assert on type)
    bool asBool() const;
    int64_t asInt() const;
    double asNumber() const;
    std::string_view asString() const;
    uint32_t asSymbol() const;

    // Truthiness: nil and false are falsy, everything else truthy
    bool truthy() const;
};

/// Parsed script — the cached AST representation.
class CompiledScript {
public:
    // Opaque, owned by the engine. Contains the AST.
    // Can be executed multiple times with different contexts.
};

/// Result of script execution.
struct ScriptResult {
    bool success = true;
    Value returnValue;
    std::string error;          // empty if success
    std::string scriptName;     // for error reporting
    int errorLine = 0;          // for error reporting
};

/// Native function signature.
/// Receives the calling scope (for context access) and evaluated arguments.
using NativeFunction = std::function<Value(std::span<const Value> args)>;

/// Native function with context access (for functions that need world, player, etc.)
using ContextFunction = std::function<Value(class ExecutionContext& ctx,
                                             std::span<const Value> args)>;

/// Abstract string interner interface. finescript ships with a built-in
/// implementation (DefaultInterner). Host applications can provide their own
/// by subclassing this and passing it to ScriptEngine::setInterner().
/// When all components share one interner, symbol IDs are interchangeable.
class Interner {
public:
    virtual ~Interner() = default;

    /// Intern a string, returning a unique ID. Same string → same ID.
    virtual uint32_t intern(std::string_view str) = 0;

    /// Look up the string for an interned ID.
    virtual std::string_view lookup(uint32_t id) const = 0;
};

/// A native function object — a C++ object with state and a callable method.
/// From the script's perspective, it looks like a regular function.
/// From C++'s perspective, it's an object that can hold references to engine
/// systems, cached data, etc.
///
/// When the evaluator encounters a NativeFunctionObject in verb position,
/// it calls operator(). The object can also be stored in dictionaries to
/// create organized namespaces of engine functions (world.setBlock, etc.).
class NativeFunctionObject {
public:
    virtual ~NativeFunctionObject() = default;
    virtual Value call(ExecutionContext& ctx, std::span<const Value> args) = 0;
};

/// A proxy map — looks like a dictionary to scripts but delegates all
/// reads/writes to external C++ code. This is used for tight integration
/// with external storage (e.g., a block's DataContainer, entity properties,
/// GUI widget state).
///
/// From the script's perspective, a ProxyMap is indistinguishable from a
/// regular map: you can read fields with dot notation, write with .set,
/// iterate keys, etc. But every access goes through virtual dispatch to
/// the C++ implementation.
class ProxyMap {
public:
    virtual ~ProxyMap() = default;

    /// Get a value by key. Returns nil if not found.
    virtual Value get(uint32_t key) const = 0;

    /// Set a value by key. Creates the key if it doesn't exist.
    virtual void set(uint32_t key, Value value) = 0;

    /// Check if a key exists.
    virtual bool has(uint32_t key) const = 0;

    /// Remove a key.
    virtual bool remove(uint32_t key) = 0;

    /// Get all keys (for iteration).
    virtual std::vector<uint32_t> keys() const = 0;
};

} // namespace finescript
```

### 2.2 Script Engine

```cpp
namespace finescript {

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // =========================================================================
    // Parsing (produces ASTs, caches them)
    // =========================================================================

    /// Parse a script file. Returns cached AST if source hasn't changed.
    /// Reparses if file timestamp is newer than cached AST.
    CompiledScript* loadScript(const std::filesystem::path& path);

    /// Parse a script from a string (for inline scripts, command bar, etc.)
    std::unique_ptr<CompiledScript> parseString(std::string_view source,
                                                 std::string_view name = "<inline>");

    /// Force reparse of a cached script.
    void invalidateCache(const std::filesystem::path& path);

    /// Force reparse of all cached scripts.
    void invalidateAllCaches();

    // =========================================================================
    // Execution
    // =========================================================================

    /// Execute a compiled script with a given context.
    ScriptResult execute(const CompiledScript& script, ExecutionContext& context);

    /// Execute a one-line command string (parse + execute, no caching).
    ScriptResult executeCommand(std::string_view command, ExecutionContext& context);

    // =========================================================================
    // Global Registration (available to all scripts)
    // =========================================================================

    /// Register a native function in the global scope.
    void registerFunction(std::string_view name, NativeFunction func);

    /// Register a native function that needs execution context.
    void registerContextFunction(std::string_view name, ContextFunction func);

    /// Register a constant value in the global scope.
    void registerConstant(std::string_view name, Value value);

    // =========================================================================
    // Symbol Interning (pluggable — see §4.1)
    // =========================================================================

    /// Set a custom string interner. If not set, uses the built-in one.
    /// The engine takes a non-owning pointer; the caller must ensure the
    /// interner outlives the ScriptEngine.
    void setInterner(Interner* interner);

    /// Intern a string, returning its symbol ID.
    uint32_t intern(std::string_view str);

    /// Look up an interned string.
    std::string_view lookupSymbol(uint32_t id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace finescript
```

### 2.3 Execution Context

The execution context is created per-invocation. It holds the local scope
(pre-bound context variables) for a single script execution.

```cpp
namespace finescript {

class ExecutionContext {
public:
    explicit ExecutionContext(ScriptEngine& engine);

    /// Pre-bind a variable in the context scope.
    void set(std::string_view name, Value value);

    /// Convenience: bind a map-like object with fields.
    void setObject(std::string_view name, Value mapValue);

    /// Get a value from the context (for native functions that need it).
    Value get(std::string_view name) const;

    /// Access the engine (for native functions).
    ScriptEngine& engine() { return engine_; }

    /// Set user data pointer (for native functions to access engine objects).
    void setUserData(void* data) { userData_ = data; }
    void* userData() const { return userData_; }

    /// Collect event handlers registered during execution.
    /// Scripts use `on :event_name do ... end` to register handlers.
    /// After execution, the caller retrieves them.
    struct EventHandler {
        uint32_t eventSymbol;           // interned event name
        Value handlerFunction;          // closure to call when event fires
    };
    const std::vector<EventHandler>& eventHandlers() const;

private:
    ScriptEngine& engine_;
    void* userData_ = nullptr;
    // ... scope, event handler collection, etc.
};

} // namespace finescript
```

---

## 3. AST Caching

Scripts are parsed once and cached. The cache is keyed by file path.

```cpp
// Internal to ScriptEngine
struct ASTCacheEntry {
    std::unique_ptr<CompiledScript> script;
    std::filesystem::file_time_type sourceTimestamp;
};

std::unordered_map<std::string, ASTCacheEntry> astCache_;

CompiledScript* ScriptEngine::loadScript(const std::filesystem::path& path) {
    auto key = path.string();
    auto sourceTime = std::filesystem::last_write_time(path);

    auto it = astCache_.find(key);
    if (it != astCache_.end() && it->second.sourceTimestamp >= sourceTime) {
        return it->second.script.get();  // cache hit
    }

    // Parse (or reparse)
    auto source = readFile(path);
    auto script = parse(source, path.filename().string());

    auto* ptr = script.get();
    astCache_[key] = {std::move(script), sourceTime};
    return ptr;
}
```

### Cache Invalidation

- **Automatic**: `loadScript()` checks file timestamp on every call.
- **Explicit**: `invalidateCache(path)` forces reparse on next load.
- **Bulk**: `invalidateAllCaches()` for hot-reload during development.

### Inline Scripts

Scripts embedded in block data (e.g., command blocks) are parsed from strings
and can be cached by the caller if desired:

```cpp
// Command block stores script source in its DataContainer
std::string_view source = blockData.get<std::string>("script");

// Parse it (not cached by the engine — caller can cache the CompiledScript)
auto script = engine.parseString(source, "command_block@(10,64,20)");
engine.execute(*script, context);
```

For command blocks with static scripts, the parsed AST can be stored alongside
the block data (e.g., in a `ScriptedBlockHandler` cache).

---

## 4. Integration with Engine Systems

### 4.1 Symbol Interning — Pluggable

finescript ships with a `DefaultInterner` (simple `std::vector<std::string>`
+ `std::unordered_map` for reverse lookup). This works standalone.

When integrating with a host application that has its own interning system,
wrap it in the `Interner` interface so all components share the same ID space:

```cpp
// finescript's built-in interner (used by default)
namespace finescript {

class DefaultInterner : public Interner {
public:
    uint32_t intern(std::string_view str) override {
        auto it = index_.find(str);
        if (it != index_.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(strings_.size());
        strings_.emplace_back(str);
        index_[strings_.back()] = id;
        return id;
    }

    std::string_view lookup(uint32_t id) const override {
        return strings_.at(id);
    }

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string_view, uint32_t> index_;
};

} // namespace finescript
```

```cpp
// Wrapper for FineStructureVoxel's StringInterner
class FineVoxInterner : public finescript::Interner {
public:
    explicit FineVoxInterner(finevox::StringInterner& interner)
        : interner_(interner) {}

    uint32_t intern(std::string_view str) override {
        return interner_.intern(str).id();
    }

    std::string_view lookup(uint32_t id) const override {
        return interner_.lookup(finevox::InternedId(id));
    }

private:
    finevox::StringInterner& interner_;
};

// Usage:
FineVoxInterner wrapper(finevox::StringInterner::instance());
finescript::ScriptEngine engine;
engine.setInterner(&wrapper);  // now :stone == BlockTypeId for "stone"
```

```cpp
// Wrapper for any other application's interner (e.g., finegui)
class MyAppInterner : public finescript::Interner {
    // ... same pattern: delegate intern() and lookup() to host system
};
```

When all components use the same interner, `:stone` in a script resolves to
the same `uint32_t` as `BlockTypeId::fromName("stone")`. No mapping table,
no string comparison — symbol IDs are directly interchangeable.

### 4.2 Block Handlers — ScriptedBlockHandler

A C++ `BlockHandler` that delegates to scripts. Registered with BlockRegistry
like any other handler.

```cpp
namespace finevox {

class ScriptedBlockHandler : public BlockHandler {
public:
    ScriptedBlockHandler(finescript::ScriptEngine& engine,
                          const std::filesystem::path& scriptPath);

    void onPlace(BlockContext& ctx) override {
        if (auto* handler = getHandler(EventType::BlockPlaced)) {
            auto context = makeContext(ctx);
            engine_.execute(*handler, context);
        }
    }

    void onBreak(BlockContext& ctx) override {
        if (auto* handler = getHandler(EventType::BlockBroken)) {
            auto context = makeContext(ctx);
            engine_.execute(*handler, context);
        }
    }

    void onUse(BlockContext& ctx) override {
        if (auto* handler = getHandler(EventType::PlayerUse)) {
            auto context = makeContext(ctx);
            engine_.execute(*handler, context);
        }
    }

    void onTick(BlockContext& ctx) override {
        if (auto* handler = getHandler(EventType::TickScheduled)) {
            auto context = makeContext(ctx);
            engine_.execute(*handler, context);
        }
    }

    void onNeighborChanged(BlockContext& ctx, Face face) override {
        if (auto* handler = getHandler(EventType::NeighborChanged)) {
            auto context = makeContext(ctx);
            context.set("changed_face", faceToValue(face));
            engine_.execute(*handler, context);
        }
    }

private:
    finescript::ScriptEngine& engine_;
    std::filesystem::path scriptPath_;

    // Cached: the script's parsed event handlers
    struct CachedHandlers {
        std::unique_ptr<finescript::CompiledScript> script;
        std::unordered_map<uint32_t, finescript::Value> handlers;  // event → closure
    };
    CachedHandlers cached_;

    /// Load script, execute top level (which registers `on` handlers),
    /// cache the handler closures.
    void loadAndCacheHandlers();

    /// Get a cached handler for an event type.
    finescript::Value* getHandler(EventType type);

    /// Build an execution context from a BlockContext.
    finescript::ExecutionContext makeContext(BlockContext& ctx);
};

} // namespace finevox
```

### How makeContext Works

```cpp
finescript::ExecutionContext ScriptedBlockHandler::makeContext(BlockContext& ctx) {
    finescript::ExecutionContext sctx(engine_);

    // Pre-bind context variables as script-accessible objects
    auto target = finescript::Value::map();
    target.mapSet("x", Value::integer(ctx.pos().x));
    target.mapSet("y", Value::integer(ctx.pos().y));
    target.mapSet("z", Value::integer(ctx.pos().z));
    sctx.set("target", std::move(target));

    auto self = finescript::Value::map();
    self.mapSet("x", Value::integer(ctx.pos().x));
    self.mapSet("y", Value::integer(ctx.pos().y));
    self.mapSet("z", Value::integer(ctx.pos().z));
    self.mapSet("type", Value::symbol(ctx.blockType().id));
    sctx.set("self", std::move(self));

    // Bind world operations as native functions on a world object
    auto world = finescript::Value::map();
    // world.getBlock, world.setBlock, etc. are native functions
    // that capture &ctx.world() and delegate to C++
    sctx.set("world", std::move(world));

    // Store C++ context pointer for native functions that need it
    sctx.setUserData(&ctx);

    return sctx;
}
```

### 4.3 Block Script Lifecycle

```
1. Module registers a scripted block type:
   blockgame:programmable_sign → ScriptedBlockHandler("scripts/sign.script")

2. ScriptedBlockHandler loads and parses the script (or uses cache).
   Top-level execution runs, which registers `on` handlers:

   on :interact do
       ...
   end

3. Handler closures are cached in the ScriptedBlockHandler.

4. When a player right-clicks the block:
   - Engine fires PlayerUse event → UpdateScheduler
   - UpdateScheduler dispatches to BlockHandler::onUse()
   - ScriptedBlockHandler::onUse() builds an ExecutionContext
   - Calls engine.execute() with the cached :interact handler closure
   - Script runs, can call setblock, give, print, etc.
```

### 4.4 Entity AI Scripts

Entity behavior scripts follow the same pattern but attach to Entity::tick():

```cpp
class ScriptedEntity : public Entity {
public:
    ScriptedEntity(EntityId id, EntityType type,
                    finescript::ScriptEngine& engine,
                    const std::filesystem::path& scriptPath);

    void tick(float dt, World& world) override {
        // Run the :tick handler
        auto context = makeEntityContext(dt, world);
        if (auto* handler = getHandler("tick")) {
            engine_.execute(*handler, context);
        }
    }

private:
    finescript::ScriptEngine& engine_;
    // ... cached handlers, persistent script scope for entity state
};
```

The entity's persistent state (health, AI state, home position) lives in the
script's scope, which is preserved across ticks. This scope is the "script
scope" level in the scoping hierarchy.

### 4.5 World Generation Scripts

Custom generation passes can be defined in scripts:

```cpp
class ScriptedGenerationPass : public GenerationPass {
public:
    ScriptedGenerationPass(finescript::ScriptEngine& engine,
                             const std::filesystem::path& scriptPath,
                             std::string_view name,
                             int32_t priority);

    std::string_view name() const override { return name_; }
    int32_t priority() const override { return priority_; }

    void generate(GenerationContext& ctx) override {
        auto sctx = makeGenContext(ctx);
        engine_.execute(*script_, sctx);
    }

private:
    finescript::ExecutionContext makeGenContext(GenerationContext& ctx) {
        finescript::ExecutionContext sctx(engine_);

        // Expose heightmap, biome data, noise functions, etc.
        sctx.set("heightmap", wrapHeightmap(ctx.heightmap));
        sctx.set("biomes", wrapBiomes(ctx.biomes));
        sctx.set("seed", Value::integer(ctx.worldSeed));
        sctx.set("column_x", Value::integer(ctx.pos.x));
        sctx.set("column_z", Value::integer(ctx.pos.z));

        // Expose engine noise functions
        // set_block_at, get_height, random, etc.

        return sctx;
    }
};
```

### 4.6 Commands (Console / Chat)

Commands are the simplest integration — just parse and execute:

```cpp
// In the command handler (e.g., chat bar, console)
void handleCommand(std::string_view input, Entity* executor) {
    finescript::ExecutionContext ctx(scriptEngine_);

    // Bind the executor as "player"
    if (executor) {
        ctx.set("player", wrapEntity(executor));
    }
    ctx.set("world", wrapWorld(world_));

    auto result = scriptEngine_.executeCommand(input, ctx);
    if (!result.success) {
        sendMessage(executor, "Error: " + result.error);
    }
}
```

One-line commands go through `executeCommand()` which parses (no caching)
and executes immediately. This is the "Minecraft /command" use case.

### 4.7 GUI Construction (finegui Integration)

#### The Problem: Immediate Mode vs Declarative Scripts

finegui wraps Dear ImGui, which is **immediate mode** — you call widget
functions every frame, and interactions are returned as boolean values:

```cpp
// C++ ImGui pattern (runs every frame)
if (ImGui::Button("Play")) { startGame(); }   // returns true on click frame
ImGui::SliderFloat("Volume", &vol, 0, 1);     // mutates vol in place
```

But our scripting language is **imperative/declarative** — scripts build a
description of the GUI once, and the engine renders it repeatedly. This is
the right model because:

- Scripts shouldn't run every frame (too slow for AST walking)
- Networked thin clients receive GUI definitions, not per-frame render loops
- It matches the language's "build a dict" philosophy

**Solution: Retained-mode wrapper.** Scripts build widget-tree dictionaries.
A C++ `GuiRenderer` walks the tree each frame, calling ImGui functions and
dispatching events back to script closures.

#### Widget Tree Structure

Scripts construct GUIs as nested maps using the language's standard
map/array primitives:

```
# Main menu
fn make_main_menu [] do
    set menu {ui.window "Main Menu" [
        {ui.text "Welcome to the game!"}
        {ui.button "Play" fn [] do
            start_game
        end}
        {ui.button "Settings" fn [] do
            ui.show {make_settings_menu}
        end}
        {ui.button "Quit" fn [] do
            quit_game
        end}
    ]}
    return menu
end

ui.show {make_main_menu}
```

The `ui.*` builder functions are native functions that create maps with
well-known fields:

```cpp
// ui.button creates:
// {
//   :type     → :button
//   :label    → "Play"
//   :on_click → <closure>
// }

engine.registerFunction("ui.button", [](auto args) -> Value {
    auto widget = Value::map();
    widget.mapSet("type", Value::symbol(intern("button")));
    widget.mapSet("label", args[0]);
    if (args.size() > 1) widget.mapSet("on_click", args[1]);
    return widget;
});

// ui.window creates:
// {
//   :type     → :window
//   :title    → "Main Menu"
//   :children → [<widget>, <widget>, ...]
// }

engine.registerFunction("ui.window", [](auto args) -> Value {
    auto widget = Value::map();
    widget.mapSet("type", Value::symbol(intern("window")));
    widget.mapSet("title", args[0]);
    widget.mapSet("children", args[1]);  // array of child widgets
    return widget;
});
```

#### Available Widget Builders

| Builder | Creates | Key fields |
|---------|---------|------------|
| `ui.window title children` | Window container | `:title`, `:children` |
| `ui.text content` | Static text | `:text` |
| `ui.button label callback` | Clickable button | `:label`, `:on_click` |
| `ui.checkbox label value callback` | Toggle checkbox | `:label`, `:value`, `:on_change` |
| `ui.slider label min max value callback` | Float slider | `:label`, `:min`, `:max`, `:value`, `:on_change` |
| `ui.input label value callback` | Text input | `:label`, `:value`, `:on_submit` |
| `ui.combo label items selected callback` | Dropdown | `:label`, `:items`, `:selected`, `:on_change` |
| `ui.image handle width height` | Texture image | `:texture`, `:width`, `:height` |
| `ui.separator` | Visual separator | (none) |
| `ui.group children` | Layout group | `:children` |
| `ui.columns count children` | Column layout | `:count`, `:children` |

#### C++ GuiRenderer — Walking the Widget Tree

Each frame, the `GuiRenderer` walks the widget tree between
`beginFrame()`/`endFrame()`, translating maps to ImGui calls:

```cpp
class GuiRenderer {
public:
    GuiRenderer(finescript::ScriptEngine& engine, finegui::GuiSystem& gui);

    /// Call each frame inside beginFrame/endFrame to render active GUIs.
    void renderAll();

    /// Show a widget tree (called from script's ui.show).
    void show(finescript::Value widgetTree);

    /// Hide/close a widget tree.
    void hide(int guiId);

private:
    void renderWidget(finescript::Value& widget);

    void renderWindow(finescript::Value& w) {
        auto title = w.mapGet("title").asString();
        bool open = true;
        if (ImGui::Begin(std::string(title).c_str(), &open)) {
            auto& children = w.mapGet("children");
            for (size_t i = 0; i < children.arrayLength(); i++) {
                renderWidget(children.arrayGet(i));
            }
        }
        ImGui::End();
        if (!open) w.mapSet("visible", Value::boolean(false));
    }

    void renderButton(finescript::Value& w) {
        auto label = w.mapGet("label").asString();
        if (ImGui::Button(std::string(label).c_str())) {
            auto& callback = w.mapGet("on_click");
            if (callback.isCallable()) {
                engine_.call(callback, {});  // invoke script closure
            }
        }
    }

    void renderSlider(finescript::Value& w) {
        auto label = w.mapGet("label").asString();
        float val = static_cast<float>(w.mapGet("value").asNumber());
        float lo = static_cast<float>(w.mapGet("min").asNumber());
        float hi = static_cast<float>(w.mapGet("max").asNumber());
        if (ImGui::SliderFloat(std::string(label).c_str(), &val, lo, hi)) {
            w.mapSet("value", Value::number(val));  // update widget state
            auto& callback = w.mapGet("on_change");
            if (callback.isCallable()) {
                engine_.call(callback, {Value::number(val)});
            }
        }
    }

    // ... renderCheckbox, renderInput, renderCombo, etc.
};
```

#### State Flow

```
┌─────────────────────────────────────────────────────────────┐
│  Game Logic Thread                                          │
│                                                             │
│  ┌──────────┐    state updates     ┌──────────────────┐    │
│  │ Game     │ ──────────────────▶  │ Widget Tree      │    │
│  │ Systems  │                      │ (script maps)    │    │
│  │          │  ◀────────────────── │                  │    │
│  │          │    callbacks/actions  │  :value fields   │    │
│  └──────────┘                      │  :on_click etc.  │    │
│                                    └────────┬─────────┘    │
│                                             │               │
│                                    ┌────────▼─────────┐    │
│                                    │ GuiRenderer      │    │
│                                    │ walks tree each  │    │
│                                    │ frame, calls     │    │
│                                    │ ImGui functions   │    │
│                                    └────────┬─────────┘    │
│                                             │               │
│                                    ┌────────▼─────────┐    │
│                                    │ finegui          │    │
│                                    │ GuiSystem        │    │
│                                    │ (ImGui + Vulkan) │    │
│                                    └──────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

**Game → GUI**: Game systems update `:value` fields in the widget tree maps
directly (the maps are mutable). Alternatively, scripts can re-build parts
of the tree. For data that doesn't correspond to a widget (e.g., player
health for a HUD), use finegui's `TypedStateUpdate` mechanism from C++, or
bind values into the widget tree's scope.

**GUI → Game**: When a user clicks a button or changes a slider, the
`GuiRenderer` invokes the script closure stored in `:on_click` or
`:on_change`. That closure can call any native function (setblock, teleport,
etc.) or modify script state.

#### Reactive Updates

Widgets that display changing data (health bars, inventory counts) need to
reflect current game state. Two patterns:

**Pattern 1: Direct mutation** — Game code holds a reference to the widget
map and updates `:value` directly:

```
# Script sets up a health bar
set health_bar {ui.text ""}

# Native function updates it each tick
on :tick do
    health_bar.set :text "HP: {player.health}/{player.max_health}"
end
```

**Pattern 2: Rebuild on change** — Script rebuilds the widget tree when
state changes, replacing the old tree. Cheap because we're just building
maps, not doing real rendering:

```
fn build_inventory_ui [] do
    set items []
    for slot in inventory.slots do
        if (slot != nil) do
            array.push items {ui.text "{slot.name} x{slot.count}"}
        end
    end
    return {ui.window "Inventory" items}
end

on :inventory_changed do
    ui.show {build_inventory_ui}
end
```

#### Thin-Client Networking

For networked games, the server sends GUI definitions as script source code.
The client parses, caches, and executes them locally:

```
Server                              Client
  │                                   │
  │  Player opens shop NPC            │
  │  ─────────────────────────────▶   │
  │                                   │
  │  Send: shop_gui.script source     │
  │  (or just script name if cached)  │
  │  ◀─────────────────────────────   │
  │                                   │
  │                                   ├── Parse script → AST
  │                                   ├── Cache AST by name
  │                                   ├── Execute top-level
  │                                   │   (builds widget tree)
  │                                   ├── ui.show renders it
  │                                   │
  │  Player clicks "Buy sword"        │
  │  ─────────────────────────────▶   │
  │                                   │
  │  (callback closure fires,         │
  │   sends buy_item action           │
  │   to server via native func)      │
  │                                   │
  │  Server validates, sends          │
  │  inventory update                 │
  │  ◀─────────────────────────────   │
  │                                   │
  │                                   ├── Update widget tree
  │                                   └── GUI re-renders
```

**Script transport**:
- First time: server sends full script source with a content hash
- Subsequent: server sends hash only; client uses cached AST if hash matches
- Scripts are safe to run on the client because they can only call
  native functions the client exposes (no file system access, no raw
  network calls)

**Example shop GUI sent by server**:

```
# shop_gui.script — sent by server, rendered by client

fn make_shop [items] do
    set widgets []
    for item in items do
        set row {ui.group [
            {ui.text "{item.name} — {item.price} gold"}
            {ui.button "Buy" fn [] do
                net.send_action :buy_item item.id
            end}
        ]}
        array.push widgets row
    end
    return {ui.window "Shop" widgets}
end

# 'shop_items' is pre-bound by the client from server data
ui.show {make_shop shop_items}
```

#### Script Engine Instances for GUI

As discussed, script engine instances are lightweight — primarily just an
outer variable scope. For GUI scripts:

1. Parse the script source → get/cache `CompiledScript*`
2. Create an `ExecutionContext` (the "instance")
3. Pre-bind context variables: `player`, `inventory`, `shop_items`, etc.
4. Pre-bind native functions: `ui.show`, `ui.hide`, `net.send_action`, etc.
5. Execute the script — it builds a widget tree and calls `ui.show`
6. The `GuiRenderer` takes ownership of the widget tree

The `ExecutionContext` can be kept alive for the lifetime of the GUI (so
closures remain valid and can reference the original scope), or discarded
if the widget tree is self-contained.

#### GUI-Related Native Functions

| Function | Description |
|----------|-------------|
| `ui.window title children` | Create window widget map |
| `ui.text content` | Create text widget map |
| `ui.button label callback` | Create button widget map |
| `ui.checkbox label value callback` | Create checkbox widget map |
| `ui.slider label min max value callback` | Create slider widget map |
| `ui.input label value callback` | Create text input widget map |
| `ui.combo label items selected callback` | Create dropdown widget map |
| `ui.image handle w h` | Create image widget map |
| `ui.separator` | Create separator widget map |
| `ui.group children` | Create layout group widget map |
| `ui.columns count children` | Create column layout widget map |
| `ui.show widget` | Display a widget tree (or replace current) |
| `ui.hide` | Hide/close the current GUI |
| `ui.update widget key value` | Update a field in a widget map |
| `net.send_action type data` | Send action to server (thin client) |

---

## 5. Batch Operations Integration

Scripts that modify many blocks should use batch operations for performance.
The scripting engine exposes batch ops as native functions:

```
# Script: fill a 10x10 platform
set batch {batch.begin}
for x in 0..10 do
    for z in 0..10 do
        batch.set (player.x + x) player.y (player.z + z) stone
    end
end
batch.execute batch
```

Under the hood:

```cpp
// Native function registered by the bridge
engine.registerContextFunction("batch.begin", [](auto& ctx, auto args) {
    auto* bridge = static_cast<ScriptBridge*>(ctx.userData());
    auto batch = std::make_unique<BatchBuilder>(bridge->world());
    return Value::nativeObject(std::move(batch));  // wrapped as a script value
});

engine.registerContextFunction("batch.execute", [](auto& ctx, auto args) {
    auto* batch = args[0].asNativeObject<BatchBuilder>();
    auto result = batch->execute();
    return Value::integer(result.blocksChanged);
});
```

For simple single-block operations, the bridge wraps `setblock` to go through
the event system directly:

```cpp
engine.registerContextFunction("setblock", [](auto& ctx, auto args) {
    auto* bridge = static_cast<ScriptBridge*>(ctx.userData());
    BlockPos pos(args[0].asInt(), args[1].asInt(), args[2].asInt());
    BlockTypeId type(args[3].asSymbol());
    bridge->scheduler().pushExternalEvent(
        BlockEvent::blockPlaced(pos, type, bridge->world().getBlock(pos)));
    return Value::nil();
});
```

---

## 6. Native Function Objects

### Why Objects, Not Just Lambdas

The `NativeFunction` and `ContextFunction` typedefs work fine for simple
stateless operations (math, string manipulation). But engine integration
often needs functions that:

- Hold references to engine systems (world, scheduler, entity manager)
- Cache data between calls (block type lookups, compiled patterns)
- Have multiple related methods organized in a namespace

A `NativeFunctionObject` is a C++ object with a `call()` method. From the
script's perspective it's just a callable value. From C++'s perspective it's
a proper object with state.

### Organizing Functions in Dictionaries

Native function objects can be placed in script-visible dictionaries to
create organized namespaces:

```cpp
// Build a "world" object with methods — each method is a NativeFunctionObject
class WorldSetBlock : public NativeFunctionObject {
public:
    WorldSetBlock(World& world, UpdateScheduler& sched)
        : world_(world), scheduler_(sched) {}

    Value call(ExecutionContext& ctx, std::span<const Value> args) override {
        BlockPos pos(args[0].asInt(), args[1].asInt(), args[2].asInt());
        BlockTypeId type(args[3].asSymbol());
        scheduler_.pushExternalEvent(
            BlockEvent::blockPlaced(pos, type, world_.getBlock(pos)));
        return Value::nil();
    }
private:
    World& world_;
    UpdateScheduler& scheduler_;
};

// Registration:
auto worldObj = Value::map();
worldObj.mapSet("setBlock", Value::nativeFunction(
    std::make_shared<WorldSetBlock>(world, scheduler)));
worldObj.mapSet("getBlock", Value::nativeFunction(
    std::make_shared<WorldGetBlock>(world)));
// ...
context.set("world", std::move(worldObj));

// Script uses it naturally via dot notation:
//   world.setBlock 10 64 20 stone
//   set blockType {world.getBlock 10 64 20}
```

This pattern applies to all engine subsystems: `world.*`, `batch.*`,
`entity.*`, `ui.*`, etc. The evaluator sees a map with callable values —
no special dispatch needed.

---

## 7. Threading Model

### Where Scripts Run

Scripts execute on the **game logic thread**, same as BlockHandlers and the
UpdateScheduler event loop. This means:

- **No concurrent script execution** — one script runs at a time on the
  game thread. No locks needed inside the script engine.
- **Must be fast** — a script that takes too long blocks the game tick.
  Long-running scripts (generation, large builds) should yield periodically
  or use batch operations.
- **World access is safe** — scripts can read/write the world directly
  (through the bridge's native functions) without locking, because they're
  on the game thread.

### AST Cache Thread Safety

The AST cache is read from the game thread (during execution) and potentially
written from the game thread (during first load or reparse). Since all access
is single-threaded, no locking is needed.

If script loading is ever moved to a background thread, the cache would need
a read-write lock. But for now, keep it simple.

---

## 8. Script Registration via Modules

Game modules register scripted content during their `onRegister()` phase:

```cpp
class MyGameModule : public GameModule {
public:
    void onRegister(ModuleRegistry& registry) override {
        auto& engine = registry.scriptEngine();  // engine available at registration

        // Register a scripted block handler
        registry.blocks().registerHandler(
            "mymod:magic_sign",
            std::make_unique<ScriptedBlockHandler>(
                engine, "scripts/blocks/magic_sign.script"));

        // Register a scripted command
        registry.commands().registerScriptCommand(
            "home", engine, "scripts/commands/home.script");

        // Register a scripted generation pass
        registry.worldGen().addScriptPass(
            engine, "scripts/worldgen/rivers.script",
            "mymod:rivers", 2500);
    }
};
```

Alternatively, a module could just register a directory of scripts and the
bridge scans it for block scripts, entity scripts, etc. based on convention:

```
scripts/
  blocks/
    magic_sign.script       → registered for mymod:magic_sign
    programmable_block.script
  entities/
    zombie_ai.script
  commands/
    home.script
    sethome.script
  worldgen/
    rivers.script
```

---

## 9. Native Function Library

The bridge registers these native functions in the script engine's global
scope. All scripts can use them.

### 9.1 Math

| Function | Description |
|----------|-------------|
| `add a b` | Addition (also via `+` in parens) |
| `sub a b` | Subtraction |
| `mul a b` | Multiplication |
| `div a b` | Division |
| `mod a b` | Modulo |
| `abs x` | Absolute value |
| `min a b` | Minimum |
| `max a b` | Maximum |
| `floor x` | Floor |
| `ceil x` | Ceiling |
| `round x` | Round |
| `sqrt x` | Square root |
| `pow x y` | Exponentiation |
| `random max` | Random int 0..max-1 |
| `random_range lo hi` | Random int lo..hi |
| `random_float` | Random float 0.0..1.0 |
| `sin x`, `cos x`, `tan x` | Trig (radians) |

### 9.2 Comparison

| Function | Description |
|----------|-------------|
| `eq a b` | Equal (also `==` in parens) |
| `ne a b` | Not equal |
| `lt a b` | Less than |
| `gt a b` | Greater than |
| `le a b` | Less or equal |
| `ge a b` | Greater or equal |

### 9.3 String Operations

| Function | Description |
|----------|-------------|
| `str.length s` | String length |
| `str.concat a b` | Concatenation |
| `str.substr s start len` | Substring |
| `str.find s pattern` | Find substring (returns index or nil) |
| `str.upper s` | Uppercase |
| `str.lower s` | Lowercase |
| `str.format fmt args...` | Printf-style formatting |

### 9.4 Array Operations

| Function | Description |
|----------|-------------|
| `array.length a` | Array length |
| `array.push a item` | Append (mutates) |
| `array.pop a` | Remove last (mutates), returns it |
| `array.get a index` | Get element (also `a[index]`) |
| `array.set a index val` | Set element (mutates) |
| `array.slice a start end` | Sub-array |
| `array.contains a item` | Membership test |
| `array.sort a` | Sort in place |
| `array.map a func` | Map function over array, return new array |
| `array.filter a func` | Filter array, return new array |
| `array.foreach a func` | Call function on each element |

### 9.5 Map Operations

| Function | Description |
|----------|-------------|
| `map` | Create empty map (or `map :k1 v1 :k2 v2 ...`) |
| `m.get key` | Get value (also `m.key` via dot notation) |
| `m.set key val` | Set value |
| `m.setMethod key func` | Set method (auto-passes self on dot-call) |
| `m.has key` | Check key existence |
| `m.remove key` | Remove key |
| `m.keys` | Array of keys |
| `m.values` | Array of values |

### 9.6 Engine API (Context-Dependent)

These are registered by the bridge and may only be available in certain
execution contexts.

| Function | Context | Description |
|----------|---------|-------------|
| `setblock x y z type` | Block handler, command | Place a block |
| `getblock x y z` | Any | Get block type at position |
| `give target item count` | Command | Give items to entity |
| `teleport target x y z` | Command | Teleport entity |
| `play_sound name x y z` | Any | Play sound at position |
| `drop_item x y z item count` | Block handler | Drop item entity |
| `tell target message` | Command | Send message to player |
| `schedule_tick delay` | Block handler | Schedule a future tick |
| `distance a b` | Any | Distance between two entities/positions |
| `print args...` | Any | Print to console/log |
| `batch.begin` | Any | Start batch builder |
| `batch.set batch x y z type` | Any | Queue block change |
| `batch.fill batch x1 y1 z1 x2 y2 z2 type` | Any | Queue fill |
| `batch.execute batch` | Any | Execute batch |
| `ui.show widget` | GUI | Display a widget tree (see §4.7) |
| `ui.hide` | GUI | Hide/close the current GUI |
| `net.send_action type data` | GUI (thin client) | Send action to server |

---

## 10. Persistent Script State via ProxyMap

### The Problem

Block and entity scripts need persistent state that survives world
save/load. The engine already has `DataContainer` (CBOR-serialized) for
this. Rather than copying data in and out of the script scope on every
invocation, we use a **ProxyMap** that reads/writes directly from the
`DataContainer`.

### DataContainerProxy

```cpp
/// ProxyMap backed by a DataContainer. Reads and writes go directly
/// to the container — no serialization boundary for script access.
class DataContainerProxy : public finescript::ProxyMap {
public:
    explicit DataContainerProxy(finevox::DataContainer& container,
                                 finescript::ScriptEngine& engine)
        : container_(container), engine_(engine) {}

    Value get(uint32_t key) const override {
        auto name = engine_.lookupSymbol(key);
        if (!container_.has(name)) return Value::nil();

        // Convert DataContainer value → script Value
        // DataContainer stores: int, float, string, bool, binary, nested
        return convertToScriptValue(container_.get(name));
    }

    void set(uint32_t key, Value value) override {
        auto name = engine_.lookupSymbol(key);
        // Convert script Value → DataContainer value
        convertAndStore(container_, name, value);
    }

    bool has(uint32_t key) const override {
        return container_.has(engine_.lookupSymbol(key));
    }

    bool remove(uint32_t key) override {
        return container_.remove(engine_.lookupSymbol(key));
    }

    std::vector<uint32_t> keys() const override {
        std::vector<uint32_t> result;
        for (auto& name : container_.keys()) {
            result.push_back(engine_.intern(name));
        }
        return result;
    }

private:
    finevox::DataContainer& container_;
    finescript::ScriptEngine& engine_;
};
```

### Block-Attached Scripts

Blocks with scripts (command blocks, programmable signs) store their source
in the block's `DataContainer`. The script's persistent state is exposed
as a proxy map variable:

```cpp
// Block's DataContainer layout:
// "script_source" → string (the script text)
// "script_data"   → nested DataContainer (script-accessible state)

finescript::ExecutionContext ScriptedBlockHandler::makeContext(BlockContext& ctx) {
    finescript::ExecutionContext sctx(engine_);

    // ... bind target, self, world as before ...

    // Expose block data as a proxy map — scripts read/write directly
    auto& dataContainer = ctx.blockData().getOrCreateNested("script_data");
    auto proxy = std::make_shared<DataContainerProxy>(dataContainer, engine_);
    sctx.set("data", Value::proxyMap(proxy));

    return sctx;
}
```

From the script's perspective, `data` is just a dictionary:

```
# Script reads/writes "data" — goes straight to DataContainer
on :interact do
    set data.click_count (data.click_count + 1)
    tell player "This sign has been clicked {data.click_count} times"
end

on :place do
    set data.click_count 0
    set data.owner player.name
end
```

No explicit save/restore step. The `DataContainer` is already serialized
with CBOR via the existing persistence system, so script state survives
world save/load automatically.

### Entity-Attached Scripts

Same pattern — the entity's DataContainer is wrapped in a ProxyMap:

```cpp
finescript::ExecutionContext ScriptedEntity::makeContext(float dt, World& world) {
    finescript::ExecutionContext sctx(engine_);

    sctx.set("dt", Value::number(dt));
    // ... bind self, world, etc. ...

    // Entity persistent state
    auto proxy = std::make_shared<DataContainerProxy>(
        entity_.data().getOrCreateNested("script_data"), engine_);
    sctx.set("data", Value::proxyMap(proxy));

    return sctx;
}
```

```
# Entity AI script — "data" persists across ticks and save/load
on :tick do
    if (data.state == :idle) do
        if ({find_nearest_player 20} != nil) do
            set data.state :chasing
            set data.chase_target {find_nearest_player 20}
        end
    end
end
```

### ProxyMap for Other External Storage

The ProxyMap pattern generalizes beyond DataContainers. Any external
system that wants to expose key-value state to scripts can implement
the `ProxyMap` interface:

- **Player preferences** — backed by settings file
- **World metadata** — backed by level.dat equivalent
- **Network-synced state** — reads/writes go over the network
- **GUI widget state** — backed by the retained-mode widget tree

---

## 11. Error Handling

When a script encounters an error:

1. The evaluator throws an internal `ScriptError` with message + line number
2. `ScriptEngine::execute()` catches it and returns `ScriptResult{false, ...}`
3. The bridge (ScriptedBlockHandler, etc.) logs the error:
   `[SCRIPT ERROR] sign.script:15: cannot call nil as function`
4. The game continues — script errors never crash the engine
5. The block/entity/command that triggered the script simply does nothing

For the command console, errors are displayed to the player:
```
> give player unknownitem 5
Error: 'unknownitem' is not bound (nil)
```

---

## 12. Hot Reload (Development)

During development, script hot-reload works automatically:

1. Developer edits `scripts/blocks/sign.script`
2. Next time the block handler fires, `loadScript()` checks the timestamp
3. File is newer → reparse → new AST replaces old in cache
4. New handler closures are cached
5. Next event uses the updated script

No restart required. This is a major advantage over compiled C++ handlers
for iterative gameplay development.

For production, ASTs could be pre-compiled and shipped as binary blobs
alongside the source files, skipping the parse step entirely.

---

## 13. Execution Flow Summary

### Command Execution

```
Player types: /give player diamond 64
    │
    ▼
CommandHandler receives input string
    │
    ▼
scriptEngine.executeCommand("give player diamond 64", context)
    │
    ├── Parse: verb=give, args=[player, diamond, 64]
    ├── Evaluate: look up "give" → native function
    ├── Evaluate args: player → Entity, diamond → BlockTypeId, 64 → int
    ├── Call native give(Entity, BlockTypeId, 64)
    └── Return ScriptResult{success=true}
```

### Block Event Execution

```
Player right-clicks a scripted block
    │
    ▼
UpdateScheduler receives PlayerUse event
    │
    ▼
Dispatches to ScriptedBlockHandler::onUse(BlockContext& ctx)
    │
    ├── Look up cached :interact handler closure
    ├── Build ExecutionContext from BlockContext
    │   ├── target = {x: 10, y: 64, z: 20}
    │   ├── player = {name: "Steve", health: 20, ...}
    │   ├── world = {getBlock: <native>, setBlock: <native>, ...}
    │   └── self = {type: :magic_sign, ...}
    ├── scriptEngine.execute(handler_closure, context)
    │   ├── Script runs: reads self.type, calls world.setBlock, etc.
    │   └── Returns ScriptResult
    └── Log error if !result.success
```

### Script Loading

```
Module registers ScriptedBlockHandler("scripts/blocks/sign.script")
    │
    ▼
ScriptedBlockHandler::loadAndCacheHandlers()
    │
    ├── scriptEngine.loadScript("scripts/blocks/sign.script")
    │   ├── Check cache: miss (first load)
    │   ├── Read file, parse → AST
    │   └── Store in cache with file timestamp
    │
    ├── Execute top-level of script (registers `on` handlers)
    │   ├── `on :interact do ... end` → stores closure
    │   ├── `on :place do ... end` → stores closure
    │   └── `set initial_state false` → sets persistent state
    │
    └── Cache handler closures for fast lookup during events
```
