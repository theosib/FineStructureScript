#pragma once

#include "source_location.h"
#include <stdexcept>
#include <string>

namespace finescript {

class Value;

/// Runtime script error with source location.
class ScriptError : public std::runtime_error {
public:
    ScriptError(const std::string& message, SourceLocation loc)
        : std::runtime_error(loc.toString() + ": " + message), location_(loc) {}

    const SourceLocation& location() const { return location_; }

private:
    SourceLocation location_;
};

/// Result of script execution.
struct ScriptResult {
    bool success = true;
    std::string error;
    std::string scriptName;
    int errorLine = 0;
    int errorColumn = 0;
    // returnValue is stored separately since Value isn't defined here
};

/// Non-local jump signal for `return`. NOT derived from std::exception
/// to avoid accidental catch in generic error handlers.
class ReturnSignal {
public:
    // Default constructor needed because Value isn't complete here.
    // Implementation in value.cpp.
    ReturnSignal();
    ~ReturnSignal();
    ReturnSignal(ReturnSignal&& other) noexcept;
    ReturnSignal& operator=(ReturnSignal&& other) noexcept;

    // These are implemented in value.cpp where Value is complete
    explicit ReturnSignal(Value value);
    Value& value();
    const Value& value() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace finescript
