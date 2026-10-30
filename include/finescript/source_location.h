#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace finescript {

/// Assign a stable id to a source name, for SourceLocation::fileId. Id 0 is unnamed.
uint16_t registerSourceFile(std::string_view name);

struct SourceLocation {
    uint16_t fileId = 0;
    uint16_t line = 0;
    uint16_t column = 0;

    std::string toString() const;
};

} // namespace finescript
