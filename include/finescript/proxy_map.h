#pragma once

#include <cstdint>
#include <vector>

namespace finescript {

class Value;

/// A proxy map -- looks like a dictionary to scripts but delegates all
/// reads/writes to external C++ code. Used for tight integration with
/// external storage (DataContainer, entity properties, widget state, etc.).
class ProxyMap {
public:
    virtual ~ProxyMap() = default;

    virtual Value get(uint32_t key) const = 0;
    virtual void set(uint32_t key, Value value) = 0;
    virtual bool has(uint32_t key) const = 0;
    virtual bool remove(uint32_t key) = 0;
    virtual std::vector<uint32_t> keys() const = 0;
};

} // namespace finescript
