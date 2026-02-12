#pragma once

#include "proxy_map.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace finescript {

class Value;

/// Unified map storage -- either a regular hash map or a proxy map.
/// The evaluator uses this without checking which kind it is.
/// Method flags are always stored locally (even for proxy maps).
class MapData {
public:
    /// Create an empty regular map.
    MapData() = default;

    /// Create a proxy-backed map.
    explicit MapData(std::shared_ptr<ProxyMap> proxy) : proxy_(std::move(proxy)) {}

    Value get(uint32_t key) const;
    void set(uint32_t key, Value value);
    bool has(uint32_t key) const;
    bool remove(uint32_t key);
    std::vector<uint32_t> keys() const;

    /// Store a value and mark the key as a method (auto-passes self on dot-call).
    void setMethod(uint32_t key, Value funcValue);

    /// Mark an existing key as a method (without changing the stored value).
    void markMethod(uint32_t key);

    /// Check if a key is marked as a method.
    bool isMethod(uint32_t key) const;

    bool isProxy() const { return proxy_ != nullptr; }

private:
    std::shared_ptr<ProxyMap> proxy_;
    std::unordered_map<uint32_t, Value> entries_;
    std::unordered_set<uint32_t> methodKeys_;
};

} // namespace finescript
