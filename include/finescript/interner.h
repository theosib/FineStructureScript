#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace finescript {

/// Abstract string interner interface. finescript ships with DefaultInterner.
/// Host applications can provide their own by subclassing this.
class Interner {
public:
    virtual ~Interner() = default;

    /// Intern a string, returning a unique ID. Same string -> same ID.
    virtual uint32_t intern(std::string_view str) = 0;

    /// Look up the string for an interned ID.
    virtual std::string_view lookup(uint32_t id) const = 0;
};

/// Built-in interner using deque for stable string storage.
class DefaultInterner : public Interner {
public:
    uint32_t intern(std::string_view str) override;
    std::string_view lookup(uint32_t id) const override;

private:
    // deque doesn't invalidate pointers on growth, so string_view keys stay valid
    std::deque<std::string> strings_;
    std::unordered_map<std::string_view, uint32_t> index_;
};

} // namespace finescript
