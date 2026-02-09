#pragma once

#include "value.h"
#include "scope.h"
#include <cstdint>
#include <string_view>
#include <vector>

namespace finescript {

class ScriptEngine;

class ExecutionContext {
public:
    explicit ExecutionContext(ScriptEngine& engine);

    void set(std::string_view name, Value value);
    Value get(std::string_view name) const;
    ScriptEngine& engine();

    void setUserData(void* data);
    void* userData() const;

    struct EventHandler {
        uint32_t eventSymbol;
        Value handlerFunction;
    };
    void registerEventHandler(uint32_t eventSymbol, Value handler);
    const std::vector<EventHandler>& eventHandlers() const;

    std::shared_ptr<Scope> scope() const;

private:
    ScriptEngine& engine_;
    std::shared_ptr<Scope> contextScope_;
    std::vector<EventHandler> eventHandlers_;
    void* userData_ = nullptr;
};

} // namespace finescript
