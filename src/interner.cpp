#include "finescript/interner.h"
#include <stdexcept>

namespace finescript {

uint32_t DefaultInterner::intern(std::string_view str) {
    auto it = index_.find(str);
    if (it != index_.end()) return it->second;

    uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.emplace_back(str);
    // string_view points into the deque element — stable across growth
    index_[std::string_view(strings_.back())] = id;
    return id;
}

std::string_view DefaultInterner::lookup(uint32_t id) const {
    if (id >= strings_.size()) {
        throw std::out_of_range("DefaultInterner::lookup: invalid id " + std::to_string(id));
    }
    return strings_[id];
}

} // namespace finescript
