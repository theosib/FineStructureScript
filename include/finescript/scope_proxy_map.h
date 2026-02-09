#pragma once

#include "proxy_map.h"
#include "scope.h"
#include "value.h"
#include <memory>

namespace finescript {

/// ProxyMap backed by a Scope — delegates get/set/has/keys to the scope's bindings.
/// Used to expose a scope as a map (e.g. the `global` built-in).
class ScopeProxyMap : public ProxyMap {
public:
    explicit ScopeProxyMap(std::shared_ptr<Scope> scope) : scope_(scope) {}

    Value get(uint32_t key) const override {
        auto s = scope_.lock();
        if (!s) return Value::nil();
        Value* v = s->lookup(key);
        return v ? *v : Value::nil();
    }
    void set(uint32_t key, Value value) override {
        auto s = scope_.lock();
        if (!s) return;
        s->define(key, std::move(value));
    }
    bool has(uint32_t key) const override {
        auto s = scope_.lock();
        if (!s) return false;
        return s->lookup(key) != nullptr;
    }
    bool remove(uint32_t key) override {
        return false; // scopes don't support removal
    }
    std::vector<uint32_t> keys() const override {
        auto s = scope_.lock();
        if (!s) return {};
        return s->localKeys();
    }
private:
    std::weak_ptr<Scope> scope_;
};

} // namespace finescript
