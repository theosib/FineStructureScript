#include "finescript/script_engine.h"
#include "finescript/interner.h"
#include "finescript/scope.h"
#include "finescript/evaluator.h"
#include "finescript/execution_context.h"
#include "finescript/parser.h"
#include "finescript/native_function.h"
#include "finescript/builtins.h"
#include "finescript/resource_finder.h"
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace finescript {

struct ScriptEngine::Impl {
    std::unique_ptr<DefaultInterner> ownedInterner;
    Interner* interner = nullptr;
    std::shared_ptr<Scope> globalScope;
    ResourceFinder* resourceFinder = nullptr;

    struct CachedScript {
        std::unique_ptr<CompiledScript> script;
        std::filesystem::file_time_type lastModified;
    };
    std::unordered_map<std::string, CachedScript> cache;

    Impl() {
        ownedInterner = std::make_unique<DefaultInterner>();
        interner = ownedInterner.get();
        globalScope = Scope::createGlobal();
    }
};

ScriptEngine::ScriptEngine() : impl_(std::make_unique<Impl>()) {
    registerBuiltins(*this);
}

ScriptEngine::~ScriptEngine() = default;

CompiledScript* ScriptEngine::loadScript(const std::filesystem::path& path) {
    auto key = path.string();
    auto modTime = std::filesystem::last_write_time(path);

    auto it = impl_->cache.find(key);
    if (it != impl_->cache.end() && it->second.lastModified == modTime) {
        return it->second.script.get();
    }

    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open script file: " + key);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    auto compiled = parseString(source, key);
    auto* ptr = compiled.get();
    impl_->cache[key] = {std::move(compiled), modTime};
    return ptr;
}

std::unique_ptr<CompiledScript> ScriptEngine::parseString(std::string_view source,
                                                           std::string_view name) {
    auto script = std::make_unique<CompiledScript>();
    script->name = std::string(name);
    script->root = Parser::parse(source);
    return script;
}

void ScriptEngine::invalidateCache(const std::filesystem::path& path) {
    impl_->cache.erase(path.string());
}

void ScriptEngine::invalidateAllCaches() {
    impl_->cache.clear();
}

FullScriptResult ScriptEngine::execute(const CompiledScript& script, ExecutionContext& context) {
    FullScriptResult result;
    try {
        Evaluator evaluator(interner(), impl_->globalScope, this);
        // Execute in context scope so definitions persist across commands
        result.returnValue = evaluator.eval(script.root, context.scope(), &context);
        result.success = true;
    } catch (const ScriptError& e) {
        result.success = false;
        result.error = e.what();
        result.scriptName = script.name;
        result.errorLine = e.location().line;
        result.errorColumn = e.location().column;
    } catch (const ReturnSignal& sig) {
        // Top-level return — use the value as script result
        result.returnValue = sig.value();
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        result.scriptName = script.name;
    }
    return result;
}

FullScriptResult ScriptEngine::executeCommand(std::string_view command, ExecutionContext& context) {
    std::unique_ptr<CompiledScript> script;
    try {
        script = parseString(command, "<command>");
    } catch (const std::exception& e) {
        FullScriptResult result;
        result.success = false;
        result.error = e.what();
        result.scriptName = "<command>";
        return result;
    }
    return execute(*script, context);
}

Value ScriptEngine::callFunction(const Value& callable, std::vector<Value> args,
                                 ExecutionContext& context) {
    if (callable.isNativeFunction()) {
        return const_cast<Value&>(callable).asNativeFunction().call(context, args);
    }
    if (callable.isClosure()) {
        Evaluator evaluator(interner(), impl_->globalScope, this);
        return evaluator.callFunction(callable, std::move(args),
                                      context.scope(), &context, SourceLocation{});
    }
    throw std::runtime_error("callFunction: value is not callable");
}

void ScriptEngine::registerFunction(std::string_view name,
                                    std::function<Value(ExecutionContext&, const std::vector<Value>&)> func) {
    auto nativeObj = std::make_shared<SimpleLambdaFunction>(std::move(func));
    impl_->globalScope->define(intern(name), Value::nativeFunction(std::move(nativeObj)));
}

void ScriptEngine::registerConstant(std::string_view name, Value value) {
    impl_->globalScope->define(intern(name), std::move(value));
}

void ScriptEngine::setResourceFinder(ResourceFinder* finder) {
    impl_->resourceFinder = finder;
}

std::filesystem::path ScriptEngine::resolveScript(std::string_view name) {
    if (impl_->resourceFinder) {
        return impl_->resourceFinder->resolve(name);
    }
    return std::filesystem::path(name);
}

void ScriptEngine::setInterner(Interner* interner) {
    impl_->interner = interner;
}

uint32_t ScriptEngine::intern(std::string_view str) {
    return impl_->interner->intern(str);
}

std::string_view ScriptEngine::lookupSymbol(uint32_t id) const {
    return impl_->interner->lookup(id);
}

Interner& ScriptEngine::interner() {
    return *impl_->interner;
}

std::shared_ptr<Scope> ScriptEngine::globalScope() {
    return impl_->globalScope;
}

} // namespace finescript
