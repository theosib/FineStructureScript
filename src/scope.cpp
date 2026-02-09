#include "finescript/scope.h"

namespace finescript {

Scope::Scope(std::shared_ptr<Scope> parent) : parent_(std::move(parent)) {}

std::shared_ptr<Scope> Scope::createGlobal() {
    return std::shared_ptr<Scope>(new Scope(nullptr));
}

std::shared_ptr<Scope> Scope::createChild() {
    return std::shared_ptr<Scope>(new Scope(shared_from_this()));
}

Value* Scope::lookup(uint32_t symbolId) {
    auto it = bindings_.find(symbolId);
    if (it != bindings_.end()) return &it->second;
    if (parent_) return parent_->lookup(symbolId);
    return nullptr;
}

void Scope::set(uint32_t symbolId, Value value) {
    // Python semantics: find in chain, update there; else create here
    Scope* s = this;
    while (s) {
        auto it = s->bindings_.find(symbolId);
        if (it != s->bindings_.end()) {
            it->second = std::move(value);
            return;
        }
        s = s->parent_.get();
    }
    // Not found anywhere — create in this scope
    bindings_[symbolId] = std::move(value);
}

void Scope::define(uint32_t symbolId, Value value) {
    bindings_[symbolId] = std::move(value);
}

bool Scope::hasLocal(uint32_t symbolId) const {
    return bindings_.count(symbolId) > 0;
}

std::vector<uint32_t> Scope::localKeys() const {
    std::vector<uint32_t> result;
    result.reserve(bindings_.size());
    for (auto& [k, v] : bindings_) {
        result.push_back(k);
    }
    return result;
}

} // namespace finescript
