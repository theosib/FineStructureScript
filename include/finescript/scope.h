#pragma once

#include "value.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace finescript {

class Scope : public std::enable_shared_from_this<Scope> {
public:
    static std::shared_ptr<Scope> createGlobal();
    std::shared_ptr<Scope> createChild();

    /// Lookup: walks the scope chain upward. Returns pointer if found, nullptr if not.
    Value* lookup(uint32_t symbolId);

    /// Lookup returning both the Value pointer and the Scope that owns the binding.
    std::pair<Value*, Scope*> lookupWithScope(uint32_t symbolId);

    /// Set with Python semantics: update in nearest enclosing scope if exists,
    /// otherwise create in THIS scope.
    void set(uint32_t symbolId, Value value);

    /// Define: always creates/overwrites in THIS scope.
    void define(uint32_t symbolId, Value value);

    /// Break strong reference cycles from closures in `value` back to THIS scope.
    void breakCyclesIn(Value& value);

    /// Prepare a call frame scope for exit: strengthen any closures escaping via
    /// `returnValue` and weaken local container back-edges into this scope.
    void finalizeCallScope(Value& returnValue);

    /// Clear all bindings in this scope.
    void clear();

    /// Ensure a closure Value holds a strong reference to its captured scope.
    static Value ensureStrong(const Value& value);

    bool hasLocal(uint32_t symbolId) const;
    std::vector<uint32_t> localKeys() const;
    std::shared_ptr<Scope> parent() const {
        if (parent_) return parent_;
        return weakParent_.lock();
    }

private:
    explicit Scope(std::shared_ptr<Scope> parent);
    std::shared_ptr<Scope> parent_;
    std::weak_ptr<Scope> weakParent_;
    std::unordered_map<uint32_t, Value> bindings_;
};

} // namespace finescript
