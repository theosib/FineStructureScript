#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace finescript {

class Value;
class ExecutionContext;

/// A native function object -- a C++ object with state and a callable method.
/// From the script's perspective, it looks like a regular function.
class NativeFunctionObject {
public:
    virtual ~NativeFunctionObject() = default;
    virtual Value call(ExecutionContext& ctx, const std::vector<Value>& args) = 0;
};

/// Convenience: wrap a std::function as a NativeFunctionObject.
class SimpleLambdaFunction : public NativeFunctionObject {
public:
    using Func = std::function<Value(ExecutionContext&, const std::vector<Value>&)>;

    explicit SimpleLambdaFunction(Func fn) : fn_(std::move(fn)) {}

    Value call(ExecutionContext& ctx, const std::vector<Value>& args) override {
        return fn_(ctx, args);
    }

private:
    Func fn_;
};

} // namespace finescript
