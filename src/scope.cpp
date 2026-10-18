#include "finescript/scope.h"
#include "finescript/map_data.h"
#include "finescript/proxy_map.h"
#include <unordered_set>

namespace finescript {
namespace {

/// Non-owning ProxyMap used when a local callScope holds a variable referencing
/// a MapData that also contains closures capturing that same callScope.
class WeakMapProxy : public ProxyMap {
public:
    explicit WeakMapProxy(std::shared_ptr<MapData> map) : map_(map) {}

    Value get(uint32_t key) const override {
        auto m = map_.lock();
        return m ? m->get(key) : Value::nil();
    }
    void set(uint32_t key, Value value) override {
        if (auto m = map_.lock()) m->set(key, std::move(value));
    }
    bool has(uint32_t key) const override {
        auto m = map_.lock();
        return m ? m->has(key) : false;
    }
    bool remove(uint32_t key) override {
        auto m = map_.lock();
        return m ? m->remove(key) : false;
    }
    std::vector<uint32_t> keys() const override {
        auto m = map_.lock();
        return m ? m->keys() : std::vector<uint32_t>{};
    }

private:
    std::weak_ptr<MapData> map_;
};

bool isDescendantOrSelf(const std::shared_ptr<Scope>& candidate, const Scope* ancestor,
                        std::vector<std::shared_ptr<Scope>>* outChain = nullptr) {
    if (!candidate || !ancestor) return false;
    if (candidate.get() == ancestor) return true;
    std::shared_ptr<Scope> cur = candidate;
    while (cur) {
        auto p = cur->parent();
        if (!p) break;
        if (outChain) outChain->push_back(cur);
        if (p.get() == ancestor) return true;
        cur = std::move(p);
    }
    if (outChain) outChain->clear();
    return false;
}

void breakCyclesImpl(Value& val, Scope* owner, std::unordered_set<const void*>& visited) {
    if (val.isClosure()) {
        const auto& orig = val.asClosure();
        auto cs = orig.scope();
        if (!cs) return;

        if (cs.get() == owner) {
            if (orig.capturedScope) {
                auto copy = std::make_shared<Closure>(orig);
                copy->capturedScope.reset();
                copy->weakCapturedScope = cs;
                val = Value::closure(std::move(copy));
            }
            return;
        }

        std::vector<std::shared_ptr<Scope>> chain;
        if (isDescendantOrSelf(cs, owner, &chain) && !chain.empty()) {
            // Closure captures a descendant scope of `owner`. Keep `cs` and
            // intermediate scopes strongly alive, but weaken the direct child's
            // parent pointer to `owner` so `owner -> ... -> child -> owner` cannot cycle.
            if (!orig.capturedScope) {
                auto copy = std::make_shared<Closure>(orig);
                copy->capturedScope = cs;
                copy->weakCapturedScope = cs;
                val = Value::closure(std::move(copy));
            }
            for (size_t i = 0; i + 1 < chain.size(); ++i) {
                auto p = chain[i]->parent();
                (void)p;
            }
            // Note: friend/member access via Scope::breakCyclesIn handles parent_ reset
        }
        return;
    }

    if (val.isArray()) {
        auto& arr = val.asArrayMut();
        if (!visited.insert(&arr).second) return;
        for (auto& elem : arr) {
            breakCyclesImpl(elem, owner, visited);
        }
        return;
    }

    if (val.isMap()) {
        auto& map = val.asMap();
        if (map.isProxy()) return;
        if (!visited.insert(&map).second) return;
        for (uint32_t k : map.keys()) {
            Value entry = map.get(k);
            Value updated = entry;
            breakCyclesImpl(updated, owner, visited);
            if (updated.isClosure() && entry.isClosure() &&
                updated.closurePtr().get() != entry.closurePtr().get()) {
                bool wasMethod = map.isMethod(k);
                map.set(k, std::move(updated));
                if (wasMethod) map.markMethod(k);
            }
        }
    }
}

bool containsStrongClosureReaching(const Value& val, const Scope* target,
                                   std::unordered_set<const void*>& visited) {
    if (val.isClosure()) {
        const auto& c = val.asClosure();
        if (!c.capturedScope) return false;
        return isDescendantOrSelf(c.capturedScope, target);
    }
    if (val.isArray()) {
        const auto& arr = val.asArray();
        if (!visited.insert(&arr).second) return false;
        for (const auto& elem : arr) {
            if (containsStrongClosureReaching(elem, target, visited)) return true;
        }
        return false;
    }
    if (val.isMap()) {
        const auto& map = val.asMap();
        if (map.isProxy()) return false;
        if (!visited.insert(&map).second) return false;
        for (uint32_t k : map.keys()) {
            if (containsStrongClosureReaching(map.get(k), target, visited)) return true;
        }
        return false;
    }
    return false;
}

} // anonymous namespace

Scope::Scope(std::shared_ptr<Scope> parent)
    : parent_(parent), weakParent_(parent) {}

std::shared_ptr<Scope> Scope::createGlobal() {
    return std::shared_ptr<Scope>(new Scope(nullptr));
}

std::shared_ptr<Scope> Scope::createChild() {
    return std::shared_ptr<Scope>(new Scope(shared_from_this()));
}

Value* Scope::lookup(uint32_t symbolId) {
    auto it = bindings_.find(symbolId);
    if (it != bindings_.end()) return &it->second;
    if (auto p = parent()) return p->lookup(symbolId);
    return nullptr;
}

std::pair<Value*, Scope*> Scope::lookupWithScope(uint32_t symbolId) {
    auto it = bindings_.find(symbolId);
    if (it != bindings_.end()) return {&it->second, this};
    if (auto p = parent()) return p->lookupWithScope(symbolId);
    return {nullptr, nullptr};
}

void Scope::set(uint32_t symbolId, Value value) {
    // Python semantics: find in chain, update there; else create here
    Scope* s = this;
    std::shared_ptr<Scope> holder;
    while (s) {
        auto it = s->bindings_.find(symbolId);
        if (it != s->bindings_.end()) {
            s->breakCyclesIn(value);
            it->second = std::move(value);
            return;
        }
        holder = s->parent();
        s = holder.get();
    }
    // Not found anywhere — create in this scope
    breakCyclesIn(value);
    bindings_[symbolId] = std::move(value);
}

void Scope::define(uint32_t symbolId, Value value) {
    breakCyclesIn(value);
    bindings_[symbolId] = std::move(value);
}

void Scope::breakCyclesIn(Value& value) {
    std::unordered_set<const void*> visited;
    breakCyclesImpl(value, this, visited);

    // Also weaken any ancestor parent_ link from descendant scopes back to `this`
    std::unordered_set<const void*> visitedAnc;
    auto weakenAncestorLinks = [&](auto& self, const Value& v) -> void {
        if (v.isClosure()) {
            auto cs = v.asClosure().scope();
            std::vector<std::shared_ptr<Scope>> chain;
            if (isDescendantOrSelf(cs, this, &chain) && !chain.empty()) {
                for (size_t i = 0; i + 1 < chain.size(); ++i) {
                    if (!chain[i]->parent_) {
                        chain[i]->parent_ = chain[i]->weakParent_.lock();
                    }
                }
                chain.back()->parent_.reset();
            }
            return;
        }
        if (v.isArray()) {
            const auto& arr = v.asArray();
            if (!visitedAnc.insert(&arr).second) return;
            for (const auto& elem : arr) self(self, elem);
            return;
        }
        if (v.isMap()) {
            const auto& map = v.asMap();
            if (map.isProxy()) return;
            if (!visitedAnc.insert(&map).second) return;
            for (uint32_t k : map.keys()) self(self, map.get(k));
        }
    };
    weakenAncestorLinks(weakenAncestorLinks, value);
}

void Scope::finalizeCallScope(Value& returnValue) {
    // 1. Strengthen any closures escaping in `returnValue` whose scope is `this`
    //    or a descendant of `this`.
    std::unordered_set<const void*> visited;
    auto strengthenEscaping = [&](auto& self, Value& v) -> void {
        if (v.isClosure()) {
            const auto& orig = v.asClosure();
            auto cs = orig.scope();
            std::vector<std::shared_ptr<Scope>> chain;
            if (isDescendantOrSelf(cs, this, &chain)) {
                if (!orig.capturedScope) {
                    auto copy = std::make_shared<Closure>(orig);
                    copy->capturedScope = cs;
                    copy->weakCapturedScope = cs;
                    v = Value::closure(std::move(copy));
                }
                for (auto& s : chain) {
                    if (!s->parent_) {
                        s->parent_ = s->weakParent_.lock();
                    }
                }
            }
            return;
        }
        if (v.isArray()) {
            auto& arr = v.asArrayMut();
            if (!visited.insert(&arr).second) return;
            for (auto& elem : arr) self(self, elem);
            return;
        }
        if (v.isMap()) {
            auto& map = v.asMap();
            if (map.isProxy()) return;
            if (!visited.insert(&map).second) return;
            for (uint32_t k : map.keys()) {
                Value entry = map.get(k);
                Value updated = entry;
                self(self, updated);
                if (updated.isClosure() && entry.isClosure() &&
                    updated.closurePtr().get() != entry.closurePtr().get()) {
                    bool wasMethod = map.isMethod(k);
                    map.set(k, std::move(updated));
                    if (wasMethod) map.markMethod(k);
                }
            }
        }
    };
    strengthenEscaping(strengthenEscaping, returnValue);

    // 2. For any local Map or Array in `this->bindings_` that now holds a strong
    //    closure back to `this`, replace the local binding with a weak proxy (Map)
    //    or nil (Array) so `this` does not form a strong cycle with the escaped container.
    for (auto& [sym, bval] : bindings_) {
        std::unordered_set<const void*> checkVisited;
        if (!containsStrongClosureReaching(bval, this, checkVisited)) continue;

        if (bval.isMap() && !bval.asMap().isProxy()) {
            auto origMapPtr = bval.mapPtr();
            Value proxyVal = Value::proxyMap(std::make_shared<WeakMapProxy>(origMapPtr));
            for (uint32_t k : origMapPtr->keys()) {
                if (origMapPtr->isMethod(k)) {
                    proxyVal.asMap().markMethod(k);
                }
            }
            bval = std::move(proxyVal);
        } else if (bval.isArray()) {
            bval = Value::nil();
        }
    }
}

void Scope::clear() {
    bindings_.clear();
}

Value Scope::ensureStrong(const Value& value) {
    if (value.isClosure()) {
        const auto& c = value.asClosure();
        if (!c.capturedScope) {
            if (auto s = c.weakCapturedScope.lock()) {
                auto copy = std::make_shared<Closure>(c);
                copy->capturedScope = std::move(s);
                return Value::closure(std::move(copy));
            }
        }
    }
    return value;
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
