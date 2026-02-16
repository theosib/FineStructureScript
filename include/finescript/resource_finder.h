#pragma once

#include <filesystem>
#include <string_view>

namespace finescript {

/// Abstract interface for resolving script names to filesystem paths.
/// Host applications subclass this to implement custom search logic
/// (mod directories, asset bundles, search paths, etc.).
class ResourceFinder {
public:
    virtual ~ResourceFinder() = default;

    /// Resolve a script name (e.g., "blocks/torch") to a filesystem path.
    /// Returns an empty path if the resource cannot be found.
    virtual std::filesystem::path resolve(std::string_view name) = 0;
};

} // namespace finescript
