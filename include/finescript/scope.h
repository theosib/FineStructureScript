#pragma once

#include "value.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace finescript {

class Scope : public std::enable_shared_from_this<Scope> {
public:
    static std::shared_ptr<Scope> createGlobal();
    std::shared_ptr<Scope> createChild();

    /// Lookup: walks the scope chain upward. Returns pointer if found, nullptr if not.
    Value* lookup(uint32_t symbolId);

    /// Set with Python semantics: update in nearest enclosing scope if exists,
    /// otherwise create in THIS scope.
    void set(uint32_t symbolId, Value value);

    /// Define: always creates/overwrites in THIS scope.
    void define(uint32_t symbolId, Value value);

    bool hasLocal(uint32_t symbolId) const;
    std::vector<uint32_t> localKeys() const;
    std::shared_ptr<Scope> parent() const { return parent_; }

private:
    explicit Scope(std::shared_ptr<Scope> parent);
    std::shared_ptr<Scope> parent_;
    std::unordered_map<uint32_t, Value> bindings_;
};

} // namespace finescript
