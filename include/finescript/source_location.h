#pragma once

#include <cstdint>
#include <string>

namespace finescript {

struct SourceLocation {
    uint16_t fileId = 0;
    uint16_t line = 0;
    uint16_t column = 0;

    std::string toString() const;
};

} // namespace finescript
