#include "finescript/execution_context.h"
#include "finescript/script_engine.h"
#include "finescript/scope_proxy_map.h"

namespace finescript {

ExecutionContext::ExecutionContext(ScriptEngine& engine)
    : engine_(engine) {
    contextScope_ = engine.globalScope()->createChild();

    // Register 'global' as a proxy map backed by the context scope.
    // This lets scripts read/write top-level variables via global.name.
    auto globalProxy = std::make_shared<ScopeProxyMap>(contextScope_);
    contextScope_->define(engine_.intern("global"), Value::proxyMap(std::move(globalProxy)));
}

void ExecutionContext::set(std::string_view name, Value value) {
    contextScope_->define(engine_.intern(name), std::move(value));
}

Value ExecutionContext::get(std::string_view name) const {
    auto* val = contextScope_->lookup(engine_.intern(name));
    return val ? *val : Value::nil();
}

ScriptEngine& ExecutionContext::engine() { return engine_; }

void ExecutionContext::setUserData(void* data) { userData_ = data; }
void* ExecutionContext::userData() const { return userData_; }

void ExecutionContext::registerEventHandler(uint32_t eventSymbol, Value handler) {
    eventHandlers_.push_back({eventSymbol, std::move(handler)});
}

const std::vector<ExecutionContext::EventHandler>& ExecutionContext::eventHandlers() const {
    return eventHandlers_;
}

std::shared_ptr<Scope> ExecutionContext::scope() const {
    return contextScope_;
}

} // namespace finescript
