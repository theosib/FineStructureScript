#pragma once

#include "value.h"
#include "error.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace finescript {

class Interner;
class Scope;
class ExecutionContext;

struct CompiledScript {
    std::shared_ptr<AstNode> root;
    std::string name;
};

/// Extended script result that includes the return value.
struct FullScriptResult {
    bool success = true;
    Value returnValue;
    std::string error;
    std::string scriptName;
    int errorLine = 0;
    int errorColumn = 0;
};

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Parsing
    CompiledScript* loadScript(const std::filesystem::path& path);
    std::unique_ptr<CompiledScript> parseString(std::string_view source,
                                                 std::string_view name = "<inline>");
    void invalidateCache(const std::filesystem::path& path);
    void invalidateAllCaches();

    // Execution
    FullScriptResult execute(const CompiledScript& script, ExecutionContext& context);
    FullScriptResult executeCommand(std::string_view command, ExecutionContext& context);

    /// Call a script closure or native function from C++.
    /// Returns the function's return value, or throws on error.
    Value callFunction(const Value& callable, std::vector<Value> args,
                       ExecutionContext& context);

    // Registration
    void registerFunction(std::string_view name,
                          std::function<Value(ExecutionContext&, const std::vector<Value>&)> func);
    void registerConstant(std::string_view name, Value value);

    // Interner
    void setInterner(Interner* interner);
    uint32_t intern(std::string_view str);
    std::string_view lookupSymbol(uint32_t id) const;
    Interner& interner();

    // Global scope (for evaluator access)
    std::shared_ptr<Scope> globalScope();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace finescript
