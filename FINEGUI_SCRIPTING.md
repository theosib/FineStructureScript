# finegui — Retained-Mode GUI & Script Integration

This document is a handoff to the finegui project. It describes two modules
that extend finegui with retained-mode GUI construction and optional script
engine integration.

---

## 1. Architecture Overview

Two separate modules, layered on top of finegui's existing immediate-mode
system:

```
┌─────────────────────────────────────────────────────────┐
│  Application (game engine, tool, etc.)                  │
│                                                         │
│  ┌─────────────────────┐  ┌──────────────────────────┐ │
│  │ finegui-script       │  │ Network / Game Logic     │ │
│  │ (optional)           │  │                          │ │
│  │                      │  │ Can also use             │ │
│  │ Script engine builds │  │ finegui-retained         │ │
│  │ widget trees via     │  │ directly from C++        │ │
│  │ native functions     │  │                          │ │
│  └──────────┬───────────┘  └────────────┬─────────────┘ │
│             │                            │               │
│             ▼                            ▼               │
│  ┌──────────────────────────────────────────────────┐   │
│  │ finegui-retained                                  │   │
│  │                                                   │   │
│  │ Widget tree (nested structs/maps)                 │   │
│  │ GuiRenderer walks tree each frame → ImGui calls   │   │
│  │ Event dispatch: widget interactions → callbacks    │   │
│  └──────────────────────┬────────────────────────────┘   │
│                          │                                │
│  ┌───────────────────────▼────────────────────────────┐  │
│  │ finegui (existing)                                  │  │
│  │ GuiSystem, Dear ImGui, Vulkan rendering            │  │
│  └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**finegui-retained** can be used without a script engine. C++ code can build
widget trees directly and the `GuiRenderer` handles ImGui dispatch. This is
useful for tools, editors, or any application that wants declarative GUI
without writing ImGui calls.

**finegui-script** adds script engine integration. It registers native
functions that let scripts build widget trees using the scripting language's
map/array primitives, and manages execution context lifetimes for GUI
callbacks.

The script engine dependency is **optional** — finegui-retained works
standalone, and finegui-script is a thin adapter layer.

---

## 2. finegui-retained: Retained-Mode Widget System

### 2.1 The Problem

Dear ImGui is immediate-mode: you call widget functions every frame and
check return values for interactions. This works well for C++ tools, but
doesn't suit:

- **Declarative GUI definitions** — build once, render repeatedly
- **Networked thin clients** — receive GUI descriptions, render locally
- **Script-driven GUIs** — scripts are too slow to run every frame

### 2.2 The Solution

Scripts (or C++ code) build a **widget tree** — a hierarchy of
structs/maps describing what to render. The `GuiRenderer` walks this tree
each frame between `beginFrame()`/`endFrame()`, translating it into ImGui
calls and dispatching events back through callbacks.

### 2.3 Widget Node

Each node in the widget tree is a `WidgetNode`:

```cpp
namespace finegui {

/// Callback type for widget events.
/// The callback receives the widget node that triggered it.
using WidgetCallback = std::function<void(WidgetNode& widget)>;

/// A single node in the retained-mode widget tree.
struct WidgetNode {
    /// Widget type — determines which ImGui calls to make.
    enum class Type {
        Window, Text, Button, Checkbox, Slider, SliderInt,
        InputText, InputInt, InputFloat,
        Combo, Separator, Group, Columns,
        // Phase 2
        TabBar, TabItem, TreeNode, CollapsingHeader,
        MenuBar, Menu, MenuItem,
        // Phase 3
        Table, TableColumn, TableRow,
        ColorEdit, ColorPicker,
        DragFloat, DragInt,
        ListBox, Popup, Modal,
        // Phase 4
        Image, Canvas,
        // TODO: ProgressBar, Plot, Custom draw
    };

    Type type;

    /// Display properties — which fields are used depends on type.
    std::string label;              // button text, window title, etc.
    std::string text;               // static text content
    std::string id;                 // ImGui ID (for disambiguating widgets)

    /// Value storage — widgets that hold state use these.
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    std::string stringValue;
    int selectedIndex = -1;         // for Combo, ListBox

    /// Range constraints (sliders, drags).
    float minFloat = 0.0f, maxFloat = 1.0f;
    int minInt = 0, maxInt = 100;

    /// Layout properties.
    float width = 0.0f;            // 0 = auto
    float height = 0.0f;
    int columnCount = 1;

    /// Items list (for Combo, ListBox).
    std::vector<std::string> items;

    /// Children (for Window, Group, Columns, TabBar, etc.)
    std::vector<WidgetNode> children;

    /// Visibility / enabled state.
    bool visible = true;
    bool enabled = true;

    /// Callbacks — invoked by GuiRenderer when interactions occur.
    WidgetCallback onClick;         // Button, MenuItem
    WidgetCallback onChange;        // Checkbox, Slider, Input, Combo, ColorEdit
    WidgetCallback onSubmit;        // InputText (Enter pressed)
    WidgetCallback onClose;         // Window close button

    /// Texture handle (for Image widgets).
    TextureHandle texture{};
    float imageWidth = 0.0f;
    float imageHeight = 0.0f;

    // ── Convenience builders ──────────────────────────────────────

    static WidgetNode window(std::string title, std::vector<WidgetNode> children = {});
    static WidgetNode text(std::string content);
    static WidgetNode button(std::string label, WidgetCallback onClick = {});
    static WidgetNode checkbox(std::string label, bool value, WidgetCallback onChange = {});
    static WidgetNode slider(std::string label, float value, float min, float max,
                              WidgetCallback onChange = {});
    static WidgetNode sliderInt(std::string label, int value, int min, int max,
                                 WidgetCallback onChange = {});
    static WidgetNode inputText(std::string label, std::string value,
                                 WidgetCallback onChange = {}, WidgetCallback onSubmit = {});
    static WidgetNode combo(std::string label, std::vector<std::string> items,
                             int selected, WidgetCallback onChange = {});
    static WidgetNode separator();
    static WidgetNode group(std::vector<WidgetNode> children);
    static WidgetNode columns(int count, std::vector<WidgetNode> children);
    static WidgetNode image(TextureHandle texture, float width, float height);

    // Phase 2 builders
    // TODO: tabBar, tabItem, treeNode, collapsingHeader, menuBar, menu, menuItem

    // Phase 3 builders
    // TODO: table, colorEdit, colorPicker, dragFloat, dragInt, listBox, popup, modal

    // Phase 4 builders
    // TODO: canvas (custom draw area), embedded 3D viewport
};

} // namespace finegui
```

### 2.4 GuiRenderer

```cpp
namespace finegui {

class GuiRenderer {
public:
    explicit GuiRenderer(GuiSystem& gui);

    /// Register a widget tree to be rendered each frame.
    /// Returns an ID that can be used to update or remove it.
    int show(WidgetNode tree);

    /// Replace an existing widget tree.
    void update(int guiId, WidgetNode tree);

    /// Remove a widget tree.
    void hide(int guiId);

    /// Remove all widget trees.
    void hideAll();

    /// Get a reference to a live widget tree (for direct mutation).
    WidgetNode* get(int guiId);

    /// Call once per frame, between gui.beginFrame() and gui.endFrame().
    /// Walks all active widget trees and issues ImGui calls.
    void renderAll();

private:
    GuiSystem& gui_;
    int nextId_ = 1;
    std::map<int, WidgetNode> trees_;

    void renderNode(WidgetNode& node);
    void renderWindow(WidgetNode& node);
    void renderButton(WidgetNode& node);
    void renderCheckbox(WidgetNode& node);
    void renderSlider(WidgetNode& node);
    void renderSliderInt(WidgetNode& node);
    void renderInputText(WidgetNode& node);
    void renderCombo(WidgetNode& node);
    void renderGroup(WidgetNode& node);
    void renderColumns(WidgetNode& node);
    void renderImage(WidgetNode& node);
    // ... one render method per widget type
};

} // namespace finegui
```

### 2.5 Render Dispatch

The core dispatch loop:

```cpp
void GuiRenderer::renderNode(WidgetNode& node) {
    if (!node.visible) return;

    switch (node.type) {
        case WidgetNode::Type::Window:   renderWindow(node); break;
        case WidgetNode::Type::Text:     ImGui::TextUnformatted(node.text.c_str()); break;
        case WidgetNode::Type::Button:   renderButton(node); break;
        case WidgetNode::Type::Checkbox: renderCheckbox(node); break;
        case WidgetNode::Type::Slider:   renderSlider(node); break;
        case WidgetNode::Type::Separator: ImGui::Separator(); break;
        case WidgetNode::Type::Group:    renderGroup(node); break;
        case WidgetNode::Type::Image:    renderImage(node); break;
        // ... etc.
        default:
            // TODO: unimplemented widget type — render placeholder text
            ImGui::TextColored({1,0,0,1}, "[TODO: %s]", widgetTypeName(node.type));
            break;
    }
}

void GuiRenderer::renderButton(WidgetNode& node) {
    if (node.width > 0) {
        if (ImGui::Button(node.label.c_str(), {node.width, node.height})) {
            if (node.onClick) node.onClick(node);
        }
    } else {
        if (ImGui::Button(node.label.c_str())) {
            if (node.onClick) node.onClick(node);
        }
    }
}

void GuiRenderer::renderSlider(WidgetNode& node) {
    if (ImGui::SliderFloat(node.label.c_str(), &node.floatValue,
                            node.minFloat, node.maxFloat)) {
        if (node.onChange) node.onChange(node);
    }
}

void GuiRenderer::renderWindow(WidgetNode& node) {
    bool open = true;
    if (ImGui::Begin(node.label.c_str(), &open)) {
        for (auto& child : node.children) {
            renderNode(child);
        }
    }
    ImGui::End();
    if (!open) {
        node.visible = false;
        if (node.onClose) node.onClose(node);
    }
}
```

### 2.6 Direct C++ Usage (No Script Engine)

```cpp
// Build a settings panel in C++
auto settings = WidgetNode::window("Settings", {
    WidgetNode::text("Audio"),
    WidgetNode::slider("Volume", 0.5f, 0.0f, 1.0f, [&](WidgetNode& w) {
        audioSystem.setVolume(w.floatValue);
    }),
    WidgetNode::checkbox("Mute", false, [&](WidgetNode& w) {
        audioSystem.setMuted(w.boolValue);
    }),
    WidgetNode::separator(),
    WidgetNode::text("Graphics"),
    WidgetNode::combo("Resolution", {"1920x1080", "2560x1440", "3840x2160"}, 0,
        [&](WidgetNode& w) {
            setResolution(w.selectedIndex);
        }),
    WidgetNode::separator(),
    WidgetNode::button("Apply", [&](WidgetNode& w) {
        applySettings();
    }),
});

int settingsId = guiRenderer.show(std::move(settings));

// In the main loop:
gui.beginFrame();
guiRenderer.renderAll();
gui.endFrame();
```

---

## 3. finegui-script: Script Engine Integration

### 3.1 Script Engine Overview

The script engine (working name: **finescript**) is a separate project. It
provides:

- AST-walking evaluator with prefix-first syntax (`verb arg arg ...`)
- Values: nil, bool, int, float, string, symbol, array, map, function
- Mutable maps with dot notation (`widget.label`, `widget.visible`)
- Closures (lexical scope capture)
- NativeFunctionObject — C++ objects callable from scripts
- ProxyMap — map-like interface backed by external C++ storage

See `LANGUAGE_DESIGN.md` for the full language spec and
`ENGINE_INTEGRATION.md` for the C++ API.

### 3.2 Native Builder Functions

finegui-script registers native functions that let scripts construct
`WidgetNode` trees using the language's map primitives. Each builder
creates a script map with well-known fields that the `GuiRenderer` knows
how to interpret.

**Two integration modes are possible:**

**Mode A: Script maps → C++ WidgetNode conversion.** Script builds maps,
a converter translates them to `WidgetNode` structs before rendering. Clean
separation, but requires a conversion step.

**Mode B: ProxyMap backed by WidgetNode.** Script maps *are* the widget
nodes — reads/writes go directly to `WidgetNode` fields via a `ProxyMap`
implementation. Zero-copy, but tighter coupling.

Recommendation: **Mode A** for simplicity. The conversion step is cheap
(happens once when `ui.show` is called, not per-frame), and it keeps the
retained-mode module independent of the script engine.

```cpp
// finegui-script registers these in the script engine's global scope:

// ui.window "title" [children]  →  creates a window WidgetNode
engine.registerFunction("ui.window", [](auto args) -> Value {
    auto w = Value::map();
    w.mapSet("type", Value::symbol(intern("window")));
    w.mapSet("title", args[0]);
    if (args.size() > 1) w.mapSet("children", args[1]);
    return w;
});

// ui.button "label" callback  →  creates a button WidgetNode
engine.registerFunction("ui.button", [](auto args) -> Value {
    auto w = Value::map();
    w.mapSet("type", Value::symbol(intern("button")));
    w.mapSet("label", args[0]);
    if (args.size() > 1) w.mapSet("on_click", args[1]);
    return w;
});

// ... similar for all widget types
```

### 3.3 Map-to-WidgetNode Conversion

```cpp
/// Convert a script Value (map hierarchy) into a WidgetNode tree.
/// Called once when ui.show is invoked.
WidgetNode convertToWidget(const scriptlang::Value& map,
                            scriptlang::ScriptEngine& engine) {
    WidgetNode node;

    auto typeSym = map.mapGet("type").asSymbol();
    auto typeName = engine.lookupSymbol(typeSym);

    // Map symbol → WidgetNode::Type
    if (typeName == "window")     node.type = WidgetNode::Type::Window;
    else if (typeName == "text")  node.type = WidgetNode::Type::Text;
    else if (typeName == "button") node.type = WidgetNode::Type::Button;
    // ... etc.

    // Copy fields based on type
    if (auto v = map.mapGet("label"); !v.isNil()) node.label = std::string(v.asString());
    if (auto v = map.mapGet("title"); !v.isNil()) node.label = std::string(v.asString());
    if (auto v = map.mapGet("text");  !v.isNil()) node.text = std::string(v.asString());
    if (auto v = map.mapGet("value"); !v.isNil()) {
        if (v.isBool()) node.boolValue = v.asBool();
        else if (v.isInt()) node.intValue = static_cast<int>(v.asInt());
        else if (v.isNumber()) node.floatValue = static_cast<float>(v.asNumber());
        else if (v.isString()) node.stringValue = std::string(v.asString());
    }
    if (auto v = map.mapGet("min"); !v.isNil()) node.minFloat = static_cast<float>(v.asNumber());
    if (auto v = map.mapGet("max"); !v.isNil()) node.maxFloat = static_cast<float>(v.asNumber());

    // Wrap script closures as C++ callbacks
    if (auto v = map.mapGet("on_click"); v.isCallable()) {
        node.onClick = [&engine, closure = v](WidgetNode& w) {
            engine.call(closure, {});  // invoke script closure
        };
    }
    if (auto v = map.mapGet("on_change"); v.isCallable()) {
        node.onChange = [&engine, closure = v](WidgetNode& w) {
            // Pass the new value back to the script
            engine.call(closure, {widgetValueToScriptValue(w)});
        };
    }

    // Recurse into children
    if (auto children = map.mapGet("children"); children.isArray()) {
        for (size_t i = 0; i < children.arrayLength(); i++) {
            node.children.push_back(convertToWidget(children.arrayGet(i), engine));
        }
    }

    return node;
}
```

### 3.4 Script-Side Usage

```
# Build a settings menu
fn make_settings [] do
    return {ui.window "Settings" [
        {ui.text "Audio Settings"}
        {ui.slider "Volume" 0.0 1.0 0.5 fn [val] do
            set_volume val
        end}
        {ui.checkbox "Mute" false fn [val] do
            set_muted val
        end}
        {ui.separator}
        {ui.button "Close" fn [] do
            ui.hide
        end}
    ]}
end

ui.show {make_settings}
```

### 3.5 ExecutionContext Lifetime

When a script builds a GUI with closures (callbacks), the `ExecutionContext`
that holds those closures must stay alive as long as the GUI is displayed.
Otherwise the closures would reference a dead scope.

```cpp
struct ActiveGui {
    int guiId;                                          // GuiRenderer ID
    std::unique_ptr<scriptlang::ExecutionContext> ctx;   // keeps closures alive
};

// When ui.show is called from a script:
// 1. Convert the map hierarchy → WidgetNode tree
// 2. Transfer ownership of the ExecutionContext to the ActiveGui
// 3. Pass the WidgetNode tree to GuiRenderer::show()
// 4. When the GUI is closed, destroy the ActiveGui → frees the context
```

---

## 4. Network Callbacks into GUI Scripts

### 4.1 The Problem

In a networked game, the server sends GUI definitions to thin clients. The
client renders the GUI locally. When the server needs to update the GUI
(inventory changed, shop prices updated, quest progress), it needs to push
data into the running GUI.

The key insight: **network code should not manipulate widgets directly**.
Instead, it makes **callbacks into the GUI script**, and the script decides
how to update the widget tree.

### 4.2 Callback Registration

The GUI script registers named callbacks that the network layer (or any
external system) can invoke:

```
# shop_gui.script — runs on client

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

# Build initial GUI
set shop_items initial_items    # pre-bound by client from server data
set shop_gui_id {ui.show {make_shop shop_items}}

# Register a callback for server-pushed updates
gui.on_message :update_items fn [new_items] do
    # Rebuild the shop with updated items
    set shop_items new_items
    ui.update shop_gui_id {make_shop shop_items}
end

gui.on_message :purchase_result fn [result] do
    if result.success do
        ui.flash "Purchased {result.item_name}!"
    end
    if result.error do
        ui.flash result.error
    end
end
```

### 4.3 C++ Side — Dispatching Network Messages to Scripts

```cpp
class ScriptedGuiManager {
public:
    /// Show a GUI from a script source string (e.g., received from server).
    int showFromSource(std::string_view source, std::string_view name,
                        const std::vector<std::pair<std::string, Value>>& bindings);

    /// Deliver a message to a running GUI script.
    /// Invokes the callback registered via gui.on_message in the script.
    void deliverMessage(int guiId, uint32_t messageType, Value data);

    /// Deliver a message to ALL running GUI scripts that have a handler
    /// for this message type.
    void broadcastMessage(uint32_t messageType, Value data);

private:
    struct ActiveScriptGui {
        int guiId;
        std::unique_ptr<scriptlang::ExecutionContext> ctx;
        // Message handlers registered by the script via gui.on_message
        std::unordered_map<uint32_t, scriptlang::Value> messageHandlers;
    };

    std::vector<ActiveScriptGui> activeGuis_;
    scriptlang::ScriptEngine& engine_;
    GuiRenderer& renderer_;
};

// Network layer usage:
void onServerMessage(NetworkMessage& msg) {
    if (msg.type == ServerMsg::UpdateShopItems) {
        auto items = deserializeItems(msg.payload);
        auto scriptItems = convertToScriptArray(items);
        guiManager.broadcastMessage(intern("update_items"), scriptItems);
    }
}
```

### 4.4 Flow Diagram

```
Server                     Network Layer              GUI Script           GuiRenderer
  │                            │                          │                     │
  │  shop items changed        │                          │                     │
  ├──── UpdateShopItems ─────▶│                          │                     │
  │                            │                          │                     │
  │                            ├── deliverMessage() ────▶│                     │
  │                            │   :update_items          │                     │
  │                            │   [new item data]        │                     │
  │                            │                          │                     │
  │                            │                          ├── on_message fires  │
  │                            │                          │   rebuilds widget   │
  │                            │                          │   tree from data    │
  │                            │                          │                     │
  │                            │                          ├── ui.update ──────▶│
  │                            │                          │   new WidgetNode    │
  │                            │                          │   tree              │
  │                            │                          │                     │
  │                            │                          │              next frame:
  │                            │                          │              renders new
  │                            │                          │              widget tree
```

### 4.5 Script Transport & Caching

For thin clients, GUI scripts are transported from server to client:

| Scenario | What's Sent | Client Action |
|----------|-------------|---------------|
| First time | Full script source + content hash | Parse, cache by hash, execute |
| Repeat (hash matches) | Hash only | Use cached AST, execute with new bindings |
| Script updated | New source + new hash | Reparse, replace cache entry |

```cpp
// Client receives a GUI script from the server
void onGuiScriptReceived(const std::string& hash, const std::string& source,
                          const Value& bindings) {
    auto* script = engine_.loadFromCache(hash);
    if (!script) {
        script = engine_.parseAndCache(source, hash);
    }

    std::vector<std::pair<std::string, Value>> vars;
    // ... unpack bindings ...
    guiManager_.showFromSource(script, vars);
}
```

Scripts are safe to execute on the client because they can only call native
functions that the client exposes. There is no file system access, no raw
network I/O, no eval of arbitrary code beyond what the native function
registry permits.

---

## 5. Integration with finegui State Updates

finegui already has a `TypedStateUpdate<T>` mechanism for pushing state
from game logic to GUI. This integrates with the retained-mode system:

### 5.1 C++ State Updates → Widget Mutations

```cpp
// Define a state update type
struct HealthUpdate : finegui::TypedStateUpdate<HealthUpdate> {
    float current, max;
};

// Register handler that mutates the widget tree
gui.onStateUpdate<HealthUpdate>([&renderer](const HealthUpdate& state) {
    // Find the health bar widget and update it
    if (auto* hud = renderer.get(hudGuiId)) {
        // Walk tree to find health bar, update its value
        auto* healthBar = findWidgetById(*hud, "health_bar");
        if (healthBar) {
            healthBar->floatValue = state.current;
            healthBar->maxFloat = state.max;
        }
    }
});

// Game logic pushes updates
gui.applyState(HealthUpdate{player.health, player.maxHealth});
```

### 5.2 State Updates → Script Callbacks

When a script-driven GUI needs to react to state updates, the
`ScriptedGuiManager` bridges `TypedStateUpdate` to script callbacks:

```cpp
// Register a TypedStateUpdate that delivers to script
gui.onStateUpdate<InventoryUpdate>([&guiManager](const InventoryUpdate& state) {
    auto scriptData = convertInventoryToValue(state);
    guiManager.broadcastMessage(intern("inventory_changed"), scriptData);
});
```

The script handles it:

```
gui.on_message :inventory_changed fn [inv] do
    # Rebuild inventory panel
    ui.update inventory_id {build_inventory inv}
end
```

---

## 6. Comprehensive Widget Reference

### 6.1 Phase 1 — Core Widgets (implement first)

| Widget | ImGui Function | WidgetNode Fields | Callbacks | Status |
|--------|---------------|-------------------|-----------|--------|
| Window | `Begin`/`End` | label (title), children | onClose | TODO |
| Text | `TextUnformatted` | text | — | TODO |
| Button | `Button` | label, width, height | onClick | TODO |
| Checkbox | `Checkbox` | label, boolValue | onChange | TODO |
| Slider | `SliderFloat` | label, floatValue, minFloat, maxFloat | onChange | TODO |
| SliderInt | `SliderInt` | label, intValue, minInt, maxInt | onChange | TODO |
| InputText | `InputText` | label, stringValue | onChange, onSubmit | TODO |
| Combo | `Combo` | label, items, selectedIndex | onChange | TODO |
| Separator | `Separator` | — | — | TODO |
| Group | (children only) | children | — | TODO |
| Columns | `Columns` | columnCount, children | — | TODO |
| Image | `Image` | texture, imageWidth, imageHeight | onClick | TODO |

### 6.2 Phase 2 — Layout & Navigation

| Widget | ImGui Function | WidgetNode Fields | Callbacks | Status |
|--------|---------------|-------------------|-----------|--------|
| TabBar | `BeginTabBar`/`EndTabBar` | label (bar id), children (TabItems) | — | TODO |
| TabItem | `BeginTabItem`/`EndTabItem` | label, children | onClick | TODO |
| TreeNode | `TreeNode`/`TreePop` | label, children | onClick | TODO |
| CollapsingHeader | `CollapsingHeader` | label, children, boolValue (open) | onChange | TODO |
| MenuBar | `BeginMenuBar`/`EndMenuBar` | children (Menus) | — | TODO |
| Menu | `BeginMenu`/`EndMenu` | label, children (MenuItems) | — | TODO |
| MenuItem | `MenuItem` | label, boolValue (checked) | onClick | TODO |

### 6.3 Phase 3 — Advanced Widgets

| Widget | ImGui Function | WidgetNode Fields | Callbacks | Status |
|--------|---------------|-------------------|-----------|--------|
| Table | `BeginTable`/`EndTable` | label, columnCount, children | — | TODO |
| TableColumn | `TableSetupColumn` | label, width | — | TODO |
| TableRow | `TableNextRow` | children | — | TODO |
| ColorEdit | `ColorEdit3`/`4` | label, color (vec3/vec4) | onChange | TODO |
| ColorPicker | `ColorPicker3`/`4` | label, color (vec3/vec4) | onChange | TODO |
| DragFloat | `DragFloat` | label, floatValue, min, max, speed | onChange | TODO |
| DragInt | `DragInt` | label, intValue, min, max, speed | onChange | TODO |
| ListBox | `ListBox` | label, items, selectedIndex, height | onChange | TODO |
| InputInt | `InputInt` | label, intValue | onChange | TODO |
| InputFloat | `InputFloat` | label, floatValue | onChange | TODO |
| Popup | `OpenPopup`/`BeginPopup` | label, children, boolValue (open) | onClose | TODO |
| Modal | `BeginPopupModal` | label, children, boolValue (open) | onClose | TODO |

### 6.4 Phase 4 — Custom & Advanced

| Widget | ImGui Function | WidgetNode Fields | Callbacks | Status |
|--------|---------------|-------------------|-----------|--------|
| Canvas | `GetWindowDrawList` | width, height | onDraw | TODO |
| ProgressBar | `ProgressBar` | floatValue, label | — | TODO |
| Tooltip | `SetTooltip` / `BeginTooltip` | text, children | — | TODO |
| Plot | (ImPlot extension) | data arrays, labels | — | DEFERRED |
| 3D Viewport | `Image` + TextureHandle | texture, width, height | onInput | DEFERRED |

### 6.5 Missing Features Checklist

This is a running list of ImGui features not yet covered by the retained-mode
system. Update as features are implemented.

- [ ] Widget styling (colors, fonts, padding) per-node
- [ ] ImGui ID stack management (push/pop ID for disambiguation)
- [ ] Drag-and-drop source/target
- [ ] Keyboard navigation / focus management
- [ ] Multi-select in ListBox
- [ ] Text wrapping and rich text
- [ ] Child windows / scrollable regions
- [ ] Docking (ImGui docking branch)
- [ ] Custom draw commands (line, rect, circle, text at position)
- [ ] ImGui flags per-widget (e.g., `ImGuiWindowFlags_NoTitleBar`)
- [ ] Conditional visibility based on window focus
- [ ] Widget animation (lerp values over time)
- [ ] Input field validation (min/max, regex patterns)
- [ ] Right-click context menus
- [ ] File dialogs (via OS native or ImGui extension)

---

## 7. Script Builder Function Reference

These native functions are registered by finegui-script. Each returns a
script map that `convertToWidget()` translates into a `WidgetNode`.

### Phase 1

| Function | Signature | Description |
|----------|-----------|-------------|
| `ui.window` | `title children` | Window container |
| `ui.text` | `content` | Static text |
| `ui.button` | `label [callback]` | Clickable button |
| `ui.checkbox` | `label value [callback]` | Toggle checkbox |
| `ui.slider` | `label min max value [callback]` | Float slider |
| `ui.slider_int` | `label min max value [callback]` | Integer slider |
| `ui.input` | `label value [on_change] [on_submit]` | Text input |
| `ui.input_int` | `label value [callback]` | Integer input |
| `ui.input_float` | `label value [callback]` | Float input |
| `ui.combo` | `label items selected [callback]` | Dropdown |
| `ui.separator` | (none) | Horizontal separator |
| `ui.group` | `children` | Layout group |
| `ui.columns` | `count children` | Column layout |
| `ui.image` | `texture_handle width height` | Image display |
| `ui.show` | `widget_tree` | Display a widget tree |
| `ui.update` | `gui_id widget_tree` | Replace a displayed tree |
| `ui.hide` | `[gui_id]` | Close a GUI (current if no arg) |
| `gui.on_message` | `message_type callback` | Register network callback |

### Phase 2+

<!-- TODO: Add builder functions for Phase 2-4 widgets as they are implemented -->

| Function | Signature | Description |
|----------|-----------|-------------|
| `ui.tab_bar` | `id children` | Tab bar container |
| `ui.tab_item` | `label children` | Tab within a tab bar |
| `ui.tree_node` | `label children` | Expandable tree node |
| `ui.collapsing` | `label children [open]` | Collapsing header |
| `ui.menu_bar` | `children` | Menu bar |
| `ui.menu` | `label children` | Menu dropdown |
| `ui.menu_item` | `label [callback] [checked]` | Menu item |
| `ui.table` | `label columns children` | Table container |
| `ui.color_edit` | `label r g b [a] [callback]` | Color editor |
| `ui.drag_float` | `label min max value speed [callback]` | Drag float |
| `ui.drag_int` | `label min max value speed [callback]` | Drag integer |
| `ui.list_box` | `label items selected height [callback]` | List box |
| `ui.popup` | `label children` | Popup window |
| `ui.modal` | `label children` | Modal dialog |
| `ui.progress_bar` | `value [label]` | Progress bar |

---

## 8. Example: Complete Networked Shop GUI

### Server Side (Game Logic)

```cpp
// When player interacts with shop NPC:
void onPlayerOpenShop(Player& player, ShopNPC& shop) {
    // Serialize shop items for the script
    auto items = serializeShopItems(shop.inventory());

    // Send GUI script + data to client
    networkManager.sendToClient(player.id(), GuiPacket{
        .scriptName = "shop_gui",
        .scriptHash = shop.guiScriptHash(),
        .scriptSource = shop.guiScriptSource(),  // empty if client has it cached
        .bindings = {
            {"shop_items", items},
            {"player_gold", Value::integer(player.gold())},
            {"shop_name", Value::string(shop.name())},
        }
    });
}

// When client sends a buy action:
void onClientBuyItem(Player& player, uint32_t itemId) {
    auto* item = shop.findItem(itemId);
    if (!item || player.gold() < item->price) {
        // Send error back
        networkManager.sendToClient(player.id(), GuiMessage{
            .type = intern("purchase_result"),
            .data = Value::map({
                {"success", Value::boolean(false)},
                {"error", Value::string("Not enough gold")},
            })
        });
        return;
    }

    player.removeGold(item->price);
    player.addItem(item->itemId, 1);

    // Send success + updated inventory
    networkManager.sendToClient(player.id(), GuiMessage{
        .type = intern("purchase_result"),
        .data = Value::map({
            {"success", Value::boolean(true)},
            {"item_name", Value::string(item->name)},
        })
    });

    // Also send updated item list (prices/stock may have changed)
    networkManager.sendToClient(player.id(), GuiMessage{
        .type = intern("update_items"),
        .data = serializeShopItems(shop.inventory()),
    });
}
```

### Client Side (GUI Script)

```
# shop_gui.script
# Pre-bound: shop_items, player_gold, shop_name

fn make_item_row [item] do
    return {ui.group [
        {ui.columns 3 [
            {ui.text item.name}
            {ui.text "{item.price} gold"}
            {ui.button "Buy" fn [] do
                net.send_action :buy_item item.id
            end}
        ]}
    ]}
end

fn make_shop [] do
    set rows []
    for item in shop_items do
        array.push rows {make_item_row item}
    end
    array.push rows {ui.separator}
    array.push rows {ui.text "Your gold: {player_gold}"}
    array.push rows {ui.button "Close" fn [] do ui.hide end}
    return {ui.window shop_name rows}
end

set shop_gui_id {ui.show {make_shop}}

gui.on_message :update_items fn [new_items] do
    set shop_items new_items
    ui.update shop_gui_id {make_shop}
end

gui.on_message :purchase_result fn [result] do
    if result.success do
        # Could show a flash message or update gold display
        print "Purchased {result.item_name}!"
    end
    if result.error do
        print "Error: {result.error}"
    end
end
```

---

## 9. Implementation Notes

### Build System

- **finegui-retained**: New library target, depends only on finegui (Dear ImGui)
- **finegui-script**: New library target, depends on finegui-retained + finescript
- Both are optional — finegui core works without them
- Use CMake option flags: `FINEGUI_BUILD_RETAINED`, `FINEGUI_BUILD_SCRIPT`

### Testing

- Unit test `GuiRenderer` with mock ImGui context (verify correct calls)
- Unit test `convertToWidget` with various map structures
- Integration test: build widget tree from script, verify correct rendering
- Network test: simulate server→client script transport and message delivery

### Performance Considerations

- Widget tree walk is O(n) per frame where n = total widget count
- For typical game GUIs (< 100 widgets), this is negligible
- For very large GUIs (inventories with 1000+ slots), consider:
  - Virtual scrolling (only render visible items)
  - Lazy child evaluation (don't recurse into collapsed/hidden subtrees)
  - Already handled: `if (!node.visible) return;` early-out

### Thread Safety

- `GuiRenderer::renderAll()` must be called from the same thread as
  `GuiSystem::beginFrame()`/`endFrame()` (typically the main/GUI thread)
- Script callbacks execute synchronously during `renderAll()`
- Network message delivery (`deliverMessage`) should be deferred to the
  GUI thread via a queue if called from a network thread
