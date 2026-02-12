#include "finescript/map_data.h"
#include "finescript/value.h"

namespace finescript {

Value MapData::get(uint32_t key) const {
    if (proxy_) return proxy_->get(key);
    auto it = entries_.find(key);
    if (it != entries_.end()) return it->second;
    return Value::nil();
}

void MapData::set(uint32_t key, Value value) {
    if (proxy_) {
        proxy_->set(key, std::move(value));
        return;
    }
    entries_[key] = std::move(value);
}

bool MapData::has(uint32_t key) const {
    if (proxy_) return proxy_->has(key);
    return entries_.count(key) > 0;
}

bool MapData::remove(uint32_t key) {
    methodKeys_.erase(key);
    if (proxy_) return proxy_->remove(key);
    return entries_.erase(key) > 0;
}

std::vector<uint32_t> MapData::keys() const {
    if (proxy_) return proxy_->keys();
    std::vector<uint32_t> result;
    result.reserve(entries_.size());
    for (auto& [k, v] : entries_) {
        result.push_back(k);
    }
    return result;
}

void MapData::setMethod(uint32_t key, Value funcValue) {
    if (proxy_) {
        proxy_->set(key, std::move(funcValue));
    } else {
        entries_[key] = std::move(funcValue);
    }
    methodKeys_.insert(key);
}

void MapData::markMethod(uint32_t key) {
    methodKeys_.insert(key);
}

bool MapData::isMethod(uint32_t key) const {
    return methodKeys_.count(key) > 0;
}

} // namespace finescript
