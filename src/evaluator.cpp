#include "finescript/evaluator.h"
#include "finescript/ast.h"
#include "finescript/interner.h"
#include "finescript/error.h"
#include "finescript/native_function.h"
#include "finescript/map_data.h"
#include "finescript/execution_context.h"
#include "finescript/script_engine.h"
#include "finescript/format_util.h"
#include <algorithm>
#include <cmath>

namespace finescript {

Evaluator::Evaluator(Interner& interner, std::shared_ptr<Scope> globalScope,
                     ScriptEngine* engine)
    : interner_(interner), globalScope_(std::move(globalScope)), engine_(engine) {
    preInternSymbols();
}

void Evaluator::preInternSymbols() {
    sym_get_ = interner_.intern("get");
    sym_set_ = interner_.intern("set");
    sym_has_ = interner_.intern("has");
    sym_remove_ = interner_.intern("remove");
    sym_keys_ = interner_.intern("keys");
    sym_values_ = interner_.intern("values");
    sym_length_ = interner_.intern("length");
    sym_push_ = interner_.intern("push");
    sym_pop_ = interner_.intern("pop");
    sym_setMethod_ = interner_.intern("setMethod");
    sym_slice_ = interner_.intern("slice");
    sym_contains_ = interner_.intern("contains");
    sym_sort_ = interner_.intern("sort");
    sym_map_ = interner_.intern("map");
    sym_filter_ = interner_.intern("filter");
    sym_foreach_ = interner_.intern("foreach");
    // String methods
    sym_insert_ = interner_.intern("insert");
    sym_delete_ = interner_.intern("delete");
    sym_replace_ = interner_.intern("replace");
    sym_split_ = interner_.intern("split");
    sym_substr_ = interner_.intern("substr");
    sym_find_ = interner_.intern("find");
    sym_upper_ = interner_.intern("upper");
    sym_lower_ = interner_.intern("lower");
    sym_trim_ = interner_.intern("trim");
    sym_starts_with_ = interner_.intern("starts_with");
    sym_ends_with_ = interner_.intern("ends_with");
    sym_char_at_ = interner_.intern("char_at");
    sym_sort_by_ = interner_.intern("sort_by");
    sym_self_ = interner_.intern("self");
}

bool Evaluator::isAutoMethod(const Value& val) const {
    if (!val.isClosure()) return false;
    auto& closure = const_cast<Value&>(val).asClosure();
    return !closure.paramIds.empty() && closure.paramIds[0] == sym_self_;
}

// -- Main dispatch --

Value Evaluator::eval(std::shared_ptr<AstNode> root, std::shared_ptr<Scope> scope,
                      ExecutionContext* ctx) {
    auto prevRoot = currentAstRoot_;
    currentAstRoot_ = root;
    Value result = eval(*root, scope, ctx);
    currentAstRoot_ = prevRoot;
    return result;
}

Value Evaluator::eval(const AstNode& node, std::shared_ptr<Scope> scope,
                      ExecutionContext* ctx) {
    switch (node.kind) {
        case AstNodeKind::IntLit:      return evalIntLit(node);
        case AstNodeKind::FloatLit:    return evalFloatLit(node);
        case AstNodeKind::StringLit:   return evalStringLit(node);
        case AstNodeKind::StringInterp:return evalStringInterp(node, scope, ctx);
        case AstNodeKind::SymbolLit:   return evalSymbolLit(node);
        case AstNodeKind::BoolLit:     return evalBoolLit(node);
        case AstNodeKind::NilLit:      return evalNilLit(node);
        case AstNodeKind::ArrayLit:    return evalArrayLit(node, scope, ctx);
        case AstNodeKind::Name:        return evalName(node, scope);
        case AstNodeKind::DottedName:  return evalDottedName(node, scope, ctx);
        case AstNodeKind::Call:        return evalCall(node, scope, ctx);
        case AstNodeKind::Infix:       return evalInfix(node, scope, ctx);
        case AstNodeKind::UnaryNot:    return evalUnaryNot(node, scope, ctx);
        case AstNodeKind::UnaryNegate: return evalUnaryNegate(node, scope, ctx);
        case AstNodeKind::Block:       return evalBlock(node, scope, ctx);
        case AstNodeKind::Index:       return evalIndex(node, scope, ctx);
        case AstNodeKind::Ref:        return evalRef(node, scope, ctx);
        case AstNodeKind::MapLit:     return evalMapLit(node, scope, ctx);
        case AstNodeKind::Set:         return evalSet(node, scope, ctx);
        case AstNodeKind::Let:         return evalLet(node, scope, ctx);
        case AstNodeKind::Fn:          return evalFn(node, scope);
        case AstNodeKind::If:          return evalIf(node, scope, ctx);
        case AstNodeKind::For:         return evalFor(node, scope, ctx);
        case AstNodeKind::While:       return evalWhile(node, scope, ctx);
        case AstNodeKind::Match:       return evalMatch(node, scope, ctx);
        case AstNodeKind::On:          return evalOn(node, scope, ctx);
        case AstNodeKind::Return:      return evalReturn(node, scope, ctx);
        case AstNodeKind::Source:       return evalSource(node, scope, ctx);
    }
    throw ScriptError("Unknown AST node kind", node.loc);
}

// -- Literals --

Value Evaluator::evalIntLit(const AstNode& node) {
    return Value::integer(node.intValue);
}

Value Evaluator::evalFloatLit(const AstNode& node) {
    return Value::number(node.floatValue);
}

Value Evaluator::evalStringLit(const AstNode& node) {
    return Value::string(node.stringValue);
}

Value Evaluator::evalStringInterp(const AstNode& node, std::shared_ptr<Scope> scope,
                                   ExecutionContext* ctx) {
    std::string result;
    for (auto& child : node.children) {
        Value v = eval(*child, scope, ctx);
        result += v.toString(&interner_);
    }
    return Value::string(std::move(result));
}

Value Evaluator::evalSymbolLit(const AstNode& node) {
    uint32_t sym = interner_.intern(node.stringValue);
    return Value::symbol(sym);
}

Value Evaluator::evalBoolLit(const AstNode& node) {
    return Value::boolean(node.boolValue);
}

Value Evaluator::evalNilLit(const AstNode& /*node*/) {
    return Value::nil();
}

Value Evaluator::evalArrayLit(const AstNode& node, std::shared_ptr<Scope> scope,
                               ExecutionContext* ctx) {
    std::vector<Value> elems;
    elems.reserve(node.children.size());
    for (auto& child : node.children) {
        elems.push_back(eval(*child, scope, ctx));
    }
    return Value::array(std::move(elems));
}

// -- Name lookup --

Value Evaluator::evalName(const AstNode& node, std::shared_ptr<Scope> scope) {
    uint32_t sym = interner_.intern(node.stringValue);
    Value* v = scope->lookup(sym);
    if (v) return *v;
    return Value::nil(); // unbound = nil
}

// -- Dotted name (field access chain) --

Value Evaluator::evalDottedName(const AstNode& node, std::shared_ptr<Scope> scope,
                                 ExecutionContext* ctx) {
    Value current = eval(*node.children[0], scope, ctx);

    for (const auto& field : node.nameParts) {
        uint32_t sym = interner_.intern(field);

        if (current.isMap()) {
            // Built-in zero-arg map properties
            if (sym == sym_keys_) {
                auto keys = current.asMap().keys();
                std::vector<Value> result;
                for (uint32_t k : keys) result.push_back(Value::symbol(k));
                current = Value::array(std::move(result));
            } else if (sym == sym_values_) {
                auto keys = current.asMap().keys();
                std::vector<Value> result;
                for (uint32_t k : keys) result.push_back(current.asMap().get(k));
                current = Value::array(std::move(result));
            } else {
                current = current.asMap().get(sym);
            }
        } else if (current.isArray()) {
            if (sym == sym_length_) {
                current = Value::integer(static_cast<int64_t>(current.asArray().size()));
            } else if (sym == sym_pop_) {
                auto& arr = current.asArrayMut();
                if (arr.empty()) throw ScriptError("Cannot pop from empty array", node.loc);
                Value last = arr.back();
                arr.pop_back();
                current = last;
            } else {
                throw ScriptError("Cannot access field '" + field + "' on array", node.loc);
            }
        } else if (current.isString()) {
            if (sym == sym_length_) {
                current = Value::integer(static_cast<int64_t>(current.asString().size()));
            } else {
                throw ScriptError("Cannot access field '" + field + "' on string", node.loc);
            }
        } else {
            throw ScriptError("Cannot access field '" + field + "' on " + current.typeName(), node.loc);
        }
    }

    return current;
}

// -- Call (prefix call + method dispatch) --

Value Evaluator::evalCall(const AstNode& node, std::shared_ptr<Scope> scope,
                           ExecutionContext* ctx) {
    auto& verbNode = *node.children[0];
    size_t numNamed = node.nameParts.size();  // named arg keys stored in Call's nameParts
    size_t totalChildren = node.children.size();
    size_t numPosArgs = totalChildren - 1 - numNamed; // exclude verb

    // Helper: evaluate named args into pairs
    auto evalNamedArgs = [&]() -> std::vector<std::pair<uint32_t, Value>> {
        std::vector<std::pair<uint32_t, Value>> result;
        for (size_t i = 0; i < numNamed; i++) {
            uint32_t sym = interner_.intern(node.nameParts[i]);
            Value val = eval(*node.children[numPosArgs + 1 + i], scope, ctx);
            result.push_back({sym, std::move(val)});
        }
        return result;
    };

    // Method call: verb is DottedName
    if (verbNode.kind == AstNodeKind::DottedName && !verbNode.nameParts.empty()) {
        // Evaluate base and navigate to receiver
        Value receiver = eval(*verbNode.children[0], scope, ctx);

        // Navigate through all but last field
        for (size_t i = 0; i + 1 < verbNode.nameParts.size(); i++) {
            uint32_t sym = interner_.intern(verbNode.nameParts[i]);
            if (receiver.isMap()) {
                receiver = receiver.asMap().get(sym);
            } else {
                throw ScriptError("Cannot access field '" + verbNode.nameParts[i] +
                    "' on " + receiver.typeName(), node.loc);
            }
        }

        const std::string& methodName = verbNode.nameParts.back();
        uint32_t methodSym = interner_.intern(methodName);

        // Evaluate positional arguments only
        std::vector<Value> args;
        for (size_t i = 1; i <= numPosArgs; i++) {
            args.push_back(eval(*node.children[i], scope, ctx));
        }

        // Check built-in methods (named args not supported for built-ins)
        if (receiver.isMap() && isBuiltinMapMethod(methodSym)) {
            return dispatchBuiltinMethod(receiver, methodSym, std::move(args), scope, ctx, node.loc);
        }
        if (receiver.isArray() && isBuiltinArrayMethod(methodSym)) {
            return dispatchBuiltinMethod(receiver, methodSym, std::move(args), scope, ctx, node.loc);
        }
        if (receiver.isString() && isBuiltinStringMethod(methodSym)) {
            return dispatchBuiltinMethod(receiver, methodSym, std::move(args), scope, ctx, node.loc);
        }

        // Check map field (user-defined method or stored function)
        if (receiver.isMap()) {
            MapData& map = receiver.asMap();
            if (map.has(methodSym)) {
                Value func = map.get(methodSym);
                if (map.isMethod(methodSym)) {
                    // Auto-inject self as first argument
                    args.insert(args.begin(), receiver);
                }
                // Zero-arg access on non-callable field: return value directly
                if (args.empty() && numNamed == 0 && !func.isCallable()) {
                    return func;
                }
                // Named arg dispatch for closures
                if (numNamed > 0 && func.isClosure()) {
                    auto namedArgs = evalNamedArgs();
                    auto& closure = func.asClosure();
                    return callClosureWithNamed(closure, std::move(args), std::move(namedArgs), ctx, node.loc);
                }
                // Named arg dispatch for native functions: collect into kwargs map
                if (numNamed > 0 && func.isNativeFunction()) {
                    auto namedArgs = evalNamedArgs();
                    auto kwargsMap = Value::map();
                    for (auto& [sym, val] : namedArgs) {
                        kwargsMap.asMap().set(sym, std::move(val));
                    }
                    args.push_back(std::move(kwargsMap));
                }
                return callFunction(func, std::move(args), scope, ctx, node.loc);
            }
        }

        // Zero-arg call with no method found: fall back to evalDottedName for property access
        if (args.empty() && numNamed == 0) {
            return evalDottedName(verbNode, scope, ctx);
        }

        throw ScriptError("No method '" + methodName + "' on " + receiver.typeName(), node.loc);
    }

    // Regular prefix call
    Value verb = eval(verbNode, scope, ctx);

    std::vector<Value> args;
    for (size_t i = 1; i <= numPosArgs; i++) {
        args.push_back(eval(*node.children[i], scope, ctx));
    }

    // Zero-arg call on non-callable: return the value
    if (args.empty() && numNamed == 0 && !verb.isCallable()) {
        return verb;
    }

    // Named arg dispatch for closures
    if (numNamed > 0 && verb.isClosure()) {
        auto namedArgs = evalNamedArgs();
        auto& closure = const_cast<Value&>(verb).asClosure();
        return callClosureWithNamed(closure, std::move(args), std::move(namedArgs), ctx, node.loc);
    }

    // Named arg dispatch for native functions: collect into kwargs map
    if (numNamed > 0 && verb.isNativeFunction()) {
        auto namedArgs = evalNamedArgs();
        auto kwargsMap = Value::map();
        for (auto& [sym, val] : namedArgs) {
            kwargsMap.asMap().set(sym, std::move(val));
        }
        args.push_back(std::move(kwargsMap));
    }

    return callFunction(verb, std::move(args), scope, ctx, node.loc);
}

// -- Infix --

Value Evaluator::evalInfix(const AstNode& node, std::shared_ptr<Scope> scope,
                            ExecutionContext* ctx) {
    const auto& op = node.op;

    // Short-circuit operators
    if (op == "and") {
        Value left = eval(*node.children[0], scope, ctx);
        if (!left.truthy()) return left;
        return eval(*node.children[1], scope, ctx);
    }
    if (op == "or") {
        Value left = eval(*node.children[0], scope, ctx);
        if (left.truthy()) return left;
        return eval(*node.children[1], scope, ctx);
    }
    if (op == "??") {
        Value left = eval(*node.children[0], scope, ctx);
        if (!left.isNil()) return left;
        return eval(*node.children[1], scope, ctx);
    }
    if (op == "?:") {
        Value left = eval(*node.children[0], scope, ctx);
        if (left.truthy()) return left;
        return eval(*node.children[1], scope, ctx);
    }

    Value left = eval(*node.children[0], scope, ctx);
    Value right = eval(*node.children[1], scope, ctx);

    return applyBinOp(op, left, right, node.loc);
}

// -- Unary --

Value Evaluator::evalUnaryNot(const AstNode& node, std::shared_ptr<Scope> scope,
                               ExecutionContext* ctx) {
    Value val = eval(*node.children[0], scope, ctx);
    return Value::boolean(!val.truthy());
}

Value Evaluator::evalUnaryNegate(const AstNode& node, std::shared_ptr<Scope> scope,
                                  ExecutionContext* ctx) {
    Value val = eval(*node.children[0], scope, ctx);
    if (val.isInt()) return Value::integer(-val.asInt());
    if (val.isFloat()) return Value::number(-val.asFloat());
    throw ScriptError("Cannot negate " + val.typeName(), node.loc);
}

// -- Block --

Value Evaluator::evalBlock(const AstNode& node, std::shared_ptr<Scope> scope,
                            ExecutionContext* ctx) {
    Value result;
    for (auto& child : node.children) {
        result = eval(*child, scope, ctx);
    }
    return result;
}

// -- Index --

Value Evaluator::evalIndex(const AstNode& node, std::shared_ptr<Scope> scope,
                            ExecutionContext* ctx) {
    Value target = eval(*node.children[0], scope, ctx);
    Value index = eval(*node.children[1], scope, ctx);

    if (target.isArray()) {
        if (!index.isInt()) {
            throw ScriptError("Array index must be an integer", node.loc);
        }
        int64_t idx = index.asInt();
        auto& arr = target.asArray();
        if (idx < 0) idx += static_cast<int64_t>(arr.size());
        if (idx < 0 || idx >= static_cast<int64_t>(arr.size())) {
            throw ScriptError("Array index out of bounds: " + std::to_string(index.asInt()), node.loc);
        }
        return arr[static_cast<size_t>(idx)];
    }

    if (target.isString()) {
        if (!index.isInt()) {
            throw ScriptError("String index must be an integer", node.loc);
        }
        int64_t idx = index.asInt();
        const auto& str = target.asString();
        if (idx < 0) idx += static_cast<int64_t>(str.size());
        if (idx < 0 || idx >= static_cast<int64_t>(str.size())) {
            throw ScriptError("String index out of bounds: " + std::to_string(index.asInt()), node.loc);
        }
        return Value::string(std::string(1, str[static_cast<size_t>(idx)]));
    }

    if (target.isMap()) {
        if (!index.isSymbol()) {
            throw ScriptError("Map key must be a symbol", node.loc);
        }
        return target.asMap().get(index.asSymbol());
    }

    throw ScriptError("Cannot index " + target.typeName(), node.loc);
}

// -- Ref (tilde: get value without auto-calling) --

Value Evaluator::evalRef(const AstNode& node, std::shared_ptr<Scope> scope,
                          ExecutionContext* ctx) {
    return eval(*node.children[0], scope, ctx);
}

// -- MapLit --

Value Evaluator::evalMapLit(const AstNode& node, std::shared_ptr<Scope> scope,
                             ExecutionContext* ctx) {
    Value mapVal = Value::map();
    MapData& map = mapVal.asMap();

    for (size_t i = 0; i < node.nameParts.size(); i++) {
        uint32_t sym = interner_.intern(node.nameParts[i]);
        Value val = eval(*node.children[i], scope, ctx);
        map.set(sym, val);
        // Auto-detect methods: closures with first param named "self"
        if (isAutoMethod(val)) {
            map.markMethod(sym);
        }
    }

    return mapVal;
}

// -- Set --

Value Evaluator::evalSet(const AstNode& node, std::shared_ptr<Scope> scope,
                          ExecutionContext* ctx) {
    Value val = eval(*node.children[0], scope, ctx);

    if (node.nameParts.size() == 1) {
        // Simple: set x 5
        uint32_t sym = interner_.intern(node.nameParts[0]);
        scope->set(sym, val);
    } else {
        // Dotted: set a.b.c 5
        // Look up root, navigate to penultimate map, set field on it
        uint32_t rootSym = interner_.intern(node.nameParts[0]);
        Value* root = scope->lookup(rootSym);
        if (!root) {
            throw ScriptError("Undefined variable '" + node.nameParts[0] + "'", node.loc);
        }

        // Navigate to penultimate map (maps use shared_ptr so mutations are visible)
        Value current = *root;
        for (size_t i = 1; i + 1 < node.nameParts.size(); i++) {
            if (!current.isMap()) {
                throw ScriptError("Cannot access field '" + node.nameParts[i] +
                    "' on " + current.typeName(), node.loc);
            }
            uint32_t sym = interner_.intern(node.nameParts[i]);
            current = current.asMap().get(sym);
        }

        if (!current.isMap()) {
            throw ScriptError("Cannot set field on " + current.typeName(), node.loc);
        }
        uint32_t lastSym = interner_.intern(node.nameParts.back());
        current.asMap().set(lastSym, val);
        // Auto-detect methods: closures with first param named "self"
        if (isAutoMethod(val)) {
            current.asMap().markMethod(lastSym);
        }
    }

    return val;
}

// -- Let (local define) --

Value Evaluator::evalLet(const AstNode& node, std::shared_ptr<Scope> scope,
                          ExecutionContext* ctx) {
    Value val = eval(*node.children[0], scope, ctx);
    uint32_t sym = interner_.intern(node.nameParts[0]);
    scope->define(sym, val);
    return val;
}

// -- Fn --

Value Evaluator::evalFn(const AstNode& node, std::shared_ptr<Scope> scope) {
    auto closure = std::make_shared<Closure>();
    closure->name = node.stringValue;
    closure->body = node.children[0].get();
    closure->astRoot = currentAstRoot_;  // keeps AST alive
    closure->capturedScope = scope;
    closure->numRequired = static_cast<size_t>(node.intValue);

    for (const auto& param : node.nameParts) {
        closure->paramIds.push_back(interner_.intern(param));
    }

    // Default expressions (children[1..] are defaults for optional params)
    for (size_t i = 1; i < node.children.size(); i++) {
        closure->defaultExprs.push_back(node.children[i].get());
    }

    // Variadic params: op = "restName|kwargsName" (pipe-delimited)
    if (!node.op.empty()) {
        auto pipe = node.op.find('|');
        if (pipe == std::string::npos) {
            // rest only (no pipe)
            closure->hasRestParam = true;
            closure->restParamId = interner_.intern(node.op);
        } else {
            std::string restName = node.op.substr(0, pipe);
            std::string kwargsName = node.op.substr(pipe + 1);
            if (!restName.empty()) {
                closure->hasRestParam = true;
                closure->restParamId = interner_.intern(restName);
            }
            if (!kwargsName.empty()) {
                closure->hasKwargsParam = true;
                closure->kwargsParamId = interner_.intern(kwargsName);
            }
        }
    }

    Value closureVal = Value::closure(closure);

    // Named function: define in current scope
    if (!node.stringValue.empty()) {
        uint32_t nameSym = interner_.intern(node.stringValue);
        scope->define(nameSym, closureVal);
    }

    return closureVal;
}

// -- If --

Value Evaluator::evalIf(const AstNode& node, std::shared_ptr<Scope> scope,
                         ExecutionContext* ctx) {
    // children: [cond1, body1, cond2, body2, ...] with optional else body at end
    size_t numChildren = node.children.size();
    size_t pairs = node.hasElse ? (numChildren - 1) / 2 : numChildren / 2;

    for (size_t i = 0; i < pairs; i++) {
        Value cond = eval(*node.children[i * 2], scope, ctx);
        if (cond.truthy()) {
            return eval(*node.children[i * 2 + 1], scope, ctx);
        }
    }

    if (node.hasElse) {
        return eval(*node.children.back(), scope, ctx);
    }

    return Value::nil();
}

// -- For --

Value Evaluator::evalFor(const AstNode& node, std::shared_ptr<Scope> scope,
                          ExecutionContext* ctx) {
    uint32_t varSym = interner_.intern(node.nameParts[0]);
    Value iterable = eval(*node.children[0], scope, ctx);

    auto loopScope = scope->createChild();
    loopScope->define(varSym, Value::nil());

    Value result;

    if (iterable.isArray()) {
        for (const auto& elem : iterable.asArray()) {
            loopScope->define(varSym, elem);
            result = eval(*node.children[1], loopScope, ctx);
        }
    } else {
        throw ScriptError("Cannot iterate over " + iterable.typeName(), node.loc);
    }

    return result;
}

// -- While --

Value Evaluator::evalWhile(const AstNode& node, std::shared_ptr<Scope> scope,
                            ExecutionContext* ctx) {
    Value result;
    while (true) {
        Value cond = eval(*node.children[0], scope, ctx);
        if (!cond.truthy()) break;
        result = eval(*node.children[1], scope, ctx);
    }
    return result;
}

// -- Match --

Value Evaluator::evalMatch(const AstNode& node, std::shared_ptr<Scope> scope,
                            ExecutionContext* ctx) {
    // children[0] = scrutinee, then pairs: [pattern, body, pattern, body, ...]
    Value scrutinee = eval(*node.children[0], scope, ctx);

    for (size_t i = 1; i + 1 < node.children.size(); i += 2) {
        auto& pattern = *node.children[i];

        // Wildcard: _ matches anything
        if (pattern.kind == AstNodeKind::Name && pattern.stringValue == "_") {
            return eval(*node.children[i + 1], scope, ctx);
        }

        // Evaluate pattern and compare
        Value patVal = eval(pattern, scope, ctx);
        if (scrutinee == patVal) {
            return eval(*node.children[i + 1], scope, ctx);
        }
    }

    return Value::nil(); // no match
}

// -- On --

Value Evaluator::evalOn(const AstNode& node, std::shared_ptr<Scope> scope,
                         ExecutionContext* ctx) {
    if (!ctx) {
        throw ScriptError("'on' requires an execution context", node.loc);
    }

    auto closure = std::make_shared<Closure>();
    closure->name = "on:" + node.stringValue;
    closure->body = node.children[0].get();
    closure->astRoot = currentAstRoot_;  // keeps AST alive
    closure->capturedScope = scope;

    Value handlerVal = Value::closure(closure);
    uint32_t eventSym = interner_.intern(node.stringValue);
    ctx->registerEventHandler(eventSym, handlerVal);

    return Value::nil();
}

// -- Return --

Value Evaluator::evalReturn(const AstNode& node, std::shared_ptr<Scope> scope,
                              ExecutionContext* ctx) {
    Value val;
    if (!node.children.empty()) {
        val = eval(*node.children[0], scope, ctx);
    }
    throw ReturnSignal(std::move(val));
}

// -- Source --

Value Evaluator::evalSource(const AstNode& node, std::shared_ptr<Scope> scope,
                              ExecutionContext* ctx) {
    if (!engine_) {
        throw ScriptError("'source' not available (no ScriptEngine configured)", node.loc);
    }

    Value filenameVal = eval(*node.children[0], scope, ctx);
    if (!filenameVal.isString()) {
        throw ScriptError("source requires a string filename", node.loc);
    }

    auto resolved = engine_->resolveScript(filenameVal.asString());
    if (resolved.empty()) {
        throw ScriptError("Cannot resolve script: " + filenameVal.asString(), node.loc);
    }
    auto* compiled = engine_->loadScript(resolved);

    // Execute in the current scope (like bash source)
    auto prevRoot = currentAstRoot_;
    currentAstRoot_ = compiled->root;
    Value result = eval(*compiled->root, scope, ctx);
    currentAstRoot_ = prevRoot;
    return result;
}

// -- Function calling --

Value Evaluator::callFunction(const Value& callable, std::vector<Value> args,
                               std::shared_ptr<Scope> scope, ExecutionContext* ctx,
                               SourceLocation callSite) {
    if (callable.isClosure()) {
        // const_cast safe: we need non-const access to the closure's internals
        auto& closure = const_cast<Value&>(callable).asClosure();
        return callClosure(closure, std::move(args), ctx, callSite);
    }

    if (callable.isNativeFunction()) {
        if (!ctx) {
            throw ScriptError("Cannot call native function without execution context", callSite);
        }
        auto& native = const_cast<Value&>(callable).asNativeFunction();
        return native.call(*ctx, args);
    }

    throw ScriptError("Value is not callable: " + callable.typeName(), callSite);
}

Value Evaluator::callClosure(Closure& closure, std::vector<Value> args,
                              ExecutionContext* ctx, SourceLocation /*callSite*/) {
    auto callScope = closure.capturedScope->createChild();

    // Bind parameters (with default support)
    for (size_t i = 0; i < closure.paramIds.size(); i++) {
        if (i < args.size()) {
            callScope->define(closure.paramIds[i], args[i]);
        } else if (i >= closure.numRequired &&
                   (i - closure.numRequired) < closure.defaultExprs.size() &&
                   closure.defaultExprs[i - closure.numRequired] != nullptr) {
            // Evaluate default expression at call time
            Value def = eval(*closure.defaultExprs[i - closure.numRequired], callScope, ctx);
            callScope->define(closure.paramIds[i], def);
        } else {
            callScope->define(closure.paramIds[i], Value::nil());
        }
    }

    // Collect remaining positional args into [rest] array
    if (closure.hasRestParam) {
        std::vector<Value> restArgs;
        for (size_t i = closure.paramIds.size(); i < args.size(); i++) {
            restArgs.push_back(args[i]);
        }
        callScope->define(closure.restParamId, Value::array(std::move(restArgs)));
    }
    // No named args in this path, but define empty map for consistency
    if (closure.hasKwargsParam) {
        callScope->define(closure.kwargsParamId, Value::map());
    }

    // Evaluate body, catching ReturnSignal at function boundary
    try {
        return eval(*closure.body, callScope, ctx);
    } catch (ReturnSignal& sig) {
        return std::move(sig.value());
    }
}

Value Evaluator::callClosureWithNamed(Closure& closure, std::vector<Value> posArgs,
                                       std::vector<std::pair<uint32_t, Value>> namedArgs,
                                       ExecutionContext* ctx, SourceLocation /*callSite*/) {
    auto callScope = closure.capturedScope->createChild();

    // Track which named args get matched to regular params
    std::vector<bool> namedArgUsed(namedArgs.size(), false);

    for (size_t i = 0; i < closure.paramIds.size(); i++) {
        if (i < posArgs.size()) {
            // Positionally provided
            callScope->define(closure.paramIds[i], posArgs[i]);
        } else {
            // Check named args
            bool found = false;
            for (size_t j = 0; j < namedArgs.size(); j++) {
                if (namedArgs[j].first == closure.paramIds[i]) {
                    callScope->define(closure.paramIds[i], namedArgs[j].second);
                    namedArgUsed[j] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Try default
                if (i >= closure.numRequired &&
                    (i - closure.numRequired) < closure.defaultExprs.size() &&
                    closure.defaultExprs[i - closure.numRequired] != nullptr) {
                    Value def = eval(*closure.defaultExprs[i - closure.numRequired], callScope, ctx);
                    callScope->define(closure.paramIds[i], def);
                } else {
                    callScope->define(closure.paramIds[i], Value::nil());
                }
            }
        }
    }

    // Collect remaining positional args into [rest] array
    if (closure.hasRestParam) {
        std::vector<Value> restArgs;
        for (size_t i = closure.paramIds.size(); i < posArgs.size(); i++) {
            restArgs.push_back(posArgs[i]);
        }
        callScope->define(closure.restParamId, Value::array(std::move(restArgs)));
    }

    // Collect unmatched named args into {kwargs} map
    if (closure.hasKwargsParam) {
        auto kwargsMap = Value::map();
        for (size_t j = 0; j < namedArgs.size(); j++) {
            if (!namedArgUsed[j]) {
                kwargsMap.asMap().set(namedArgs[j].first, namedArgs[j].second);
            }
        }
        callScope->define(closure.kwargsParamId, kwargsMap);
    }

    try {
        return eval(*closure.body, callScope, ctx);
    } catch (ReturnSignal& sig) {
        return std::move(sig.value());
    }
}

// -- Built-in method dispatch --

bool Evaluator::isBuiltinMapMethod(uint32_t sym) const {
    return sym == sym_get_ || sym == sym_set_ || sym == sym_has_ ||
           sym == sym_remove_ || sym == sym_keys_ || sym == sym_values_ ||
           sym == sym_setMethod_;
}

bool Evaluator::isBuiltinArrayMethod(uint32_t sym) const {
    return sym == sym_length_ || sym == sym_push_ || sym == sym_pop_ ||
           sym == sym_get_ || sym == sym_set_ || sym == sym_slice_ ||
           sym == sym_contains_ || sym == sym_sort_ || sym == sym_sort_by_ ||
           sym == sym_map_ || sym == sym_filter_ || sym == sym_foreach_;
}

bool Evaluator::isBuiltinStringMethod(uint32_t sym) const {
    return sym == sym_length_ || sym == sym_get_ || sym == sym_set_ ||
           sym == sym_char_at_ || sym == sym_insert_ || sym == sym_delete_ ||
           sym == sym_replace_ || sym == sym_find_ || sym == sym_contains_ ||
           sym == sym_substr_ || sym == sym_slice_ || sym == sym_split_ ||
           sym == sym_upper_ || sym == sym_lower_ || sym == sym_trim_ ||
           sym == sym_starts_with_ || sym == sym_ends_with_ || sym == sym_push_;
}

Value Evaluator::dispatchBuiltinMethod(const Value& object, uint32_t methodSym,
                                        std::vector<Value> args, std::shared_ptr<Scope> scope,
                                        ExecutionContext* ctx, SourceLocation loc) {
    // -- Map built-in methods --
    if (object.isMap()) {
        MapData& map = const_cast<Value&>(object).asMap();

        if (methodSym == sym_get_) {
            if (args.empty()) throw ScriptError("map.get requires a key argument", loc);
            if (!args[0].isSymbol()) throw ScriptError("Map key must be a symbol", loc);
            return map.get(args[0].asSymbol());
        }
        if (methodSym == sym_set_) {
            if (args.size() < 2) throw ScriptError("map.set requires key and value arguments", loc);
            if (!args[0].isSymbol()) throw ScriptError("Map key must be a symbol", loc);
            uint32_t key = args[0].asSymbol();
            map.set(key, args[1]);
            // Auto-detect methods: closures with first param named "self"
            if (isAutoMethod(args[1])) {
                map.markMethod(key);
            }
            return args[1];
        }
        if (methodSym == sym_has_) {
            if (args.empty()) throw ScriptError("map.has requires a key argument", loc);
            if (!args[0].isSymbol()) throw ScriptError("Map key must be a symbol", loc);
            return Value::boolean(map.has(args[0].asSymbol()));
        }
        if (methodSym == sym_remove_) {
            if (args.empty()) throw ScriptError("map.remove requires a key argument", loc);
            if (!args[0].isSymbol()) throw ScriptError("Map key must be a symbol", loc);
            return Value::boolean(map.remove(args[0].asSymbol()));
        }
        if (methodSym == sym_keys_) {
            auto keys = map.keys();
            std::vector<Value> result;
            result.reserve(keys.size());
            for (uint32_t k : keys) {
                result.push_back(Value::symbol(k));
            }
            return Value::array(std::move(result));
        }
        if (methodSym == sym_values_) {
            auto keys = map.keys();
            std::vector<Value> result;
            result.reserve(keys.size());
            for (uint32_t k : keys) {
                result.push_back(map.get(k));
            }
            return Value::array(std::move(result));
        }
        if (methodSym == sym_setMethod_) {
            if (args.size() < 2) throw ScriptError("map.setMethod requires name and function arguments", loc);
            if (!args[0].isSymbol()) throw ScriptError("Method name must be a symbol", loc);
            map.setMethod(args[0].asSymbol(), args[1]);
            return args[1];
        }
    }

    // -- Array built-in methods --
    if (object.isArray()) {
        auto& arr = const_cast<Value&>(object).asArrayMut();

        if (methodSym == sym_length_) {
            return Value::integer(static_cast<int64_t>(arr.size()));
        }
        if (methodSym == sym_push_) {
            for (auto& a : args) {
                arr.push_back(a);
            }
            return Value::integer(static_cast<int64_t>(arr.size()));
        }
        if (methodSym == sym_pop_) {
            if (arr.empty()) throw ScriptError("Cannot pop from empty array", loc);
            Value last = arr.back();
            arr.pop_back();
            return last;
        }
        if (methodSym == sym_get_) {
            if (args.empty()) throw ScriptError("array.get requires an index", loc);
            if (!args[0].isInt()) throw ScriptError("Array index must be an integer", loc);
            int64_t idx = args[0].asInt();
            if (idx < 0) idx += static_cast<int64_t>(arr.size());
            if (idx < 0 || idx >= static_cast<int64_t>(arr.size())) {
                throw ScriptError("Array index out of bounds", loc);
            }
            return arr[static_cast<size_t>(idx)];
        }
        if (methodSym == sym_set_) {
            if (args.size() < 2) throw ScriptError("array.set requires index and value", loc);
            if (!args[0].isInt()) throw ScriptError("Array index must be an integer", loc);
            int64_t idx = args[0].asInt();
            if (idx < 0) idx += static_cast<int64_t>(arr.size());
            if (idx < 0 || idx >= static_cast<int64_t>(arr.size())) {
                throw ScriptError("Array index out of bounds", loc);
            }
            arr[static_cast<size_t>(idx)] = args[1];
            return args[1];
        }
        if (methodSym == sym_slice_) {
            if (args.empty()) throw ScriptError("array.slice requires start index", loc);
            if (!args[0].isInt()) throw ScriptError("Slice start must be an integer", loc);
            int64_t start = args[0].asInt();
            int64_t end = static_cast<int64_t>(arr.size());
            if (args.size() > 1 && args[1].isInt()) end = args[1].asInt();
            if (start < 0) start += static_cast<int64_t>(arr.size());
            if (end < 0) end += static_cast<int64_t>(arr.size());
            start = std::max(int64_t(0), std::min(start, static_cast<int64_t>(arr.size())));
            end = std::max(int64_t(0), std::min(end, static_cast<int64_t>(arr.size())));
            if (start > end) start = end;
            std::vector<Value> result(arr.begin() + start, arr.begin() + end);
            return Value::array(std::move(result));
        }
        if (methodSym == sym_contains_) {
            if (args.empty()) throw ScriptError("array.contains requires a value", loc);
            for (const auto& elem : arr) {
                if (elem == args[0]) return Value::boolean(true);
            }
            return Value::boolean(false);
        }
        if (methodSym == sym_sort_) {
            std::sort(arr.begin(), arr.end(), [](const Value& a, const Value& b) {
                if (a.isInt() && b.isInt()) return a.asInt() < b.asInt();
                if (a.isNumeric() && b.isNumeric()) return a.asNumber() < b.asNumber();
                if (a.isString() && b.isString()) return a.asString() < b.asString();
                return false;
            });
            return object;
        }
        if (methodSym == sym_sort_by_) {
            if (args.empty() || !args[0].isCallable()) {
                throw ScriptError("array.sort_by requires a comparator function", loc);
            }
            auto& comparator = args[0];
            std::sort(arr.begin(), arr.end(), [&](const Value& a, const Value& b) {
                Value result = callFunction(comparator, {a, b}, scope, ctx, loc);
                return result.truthy();
            });
            return object;
        }
        if (methodSym == sym_map_) {
            if (args.empty() || !args[0].isCallable()) {
                throw ScriptError("array.map requires a function argument", loc);
            }
            std::vector<Value> result;
            result.reserve(arr.size());
            for (const auto& elem : arr) {
                result.push_back(callFunction(args[0], {elem}, scope, ctx, loc));
            }
            return Value::array(std::move(result));
        }
        if (methodSym == sym_filter_) {
            if (args.empty() || !args[0].isCallable()) {
                throw ScriptError("array.filter requires a function argument", loc);
            }
            std::vector<Value> result;
            for (const auto& elem : arr) {
                Value keep = callFunction(args[0], {elem}, scope, ctx, loc);
                if (keep.truthy()) result.push_back(elem);
            }
            return Value::array(std::move(result));
        }
        if (methodSym == sym_foreach_) {
            if (args.empty() || !args[0].isCallable()) {
                throw ScriptError("array.foreach requires a function argument", loc);
            }
            for (const auto& elem : arr) {
                callFunction(args[0], {elem}, scope, ctx, loc);
            }
            return Value::nil();
        }
    }

    // -- String built-in methods --
    if (object.isString()) {
        std::string& str = const_cast<Value&>(object).asStringMut();

        if (methodSym == sym_length_) {
            return Value::integer(static_cast<int64_t>(str.size()));
        }
        if (methodSym == sym_get_ || methodSym == sym_char_at_) {
            if (args.empty()) throw ScriptError("string.get requires an index", loc);
            if (!args[0].isInt()) throw ScriptError("String index must be an integer", loc);
            int64_t idx = args[0].asInt();
            if (idx < 0) idx += static_cast<int64_t>(str.size());
            if (idx < 0 || idx >= static_cast<int64_t>(str.size())) {
                throw ScriptError("String index out of bounds", loc);
            }
            return Value::string(std::string(1, str[static_cast<size_t>(idx)]));
        }
        if (methodSym == sym_set_) {
            if (args.size() < 2) throw ScriptError("string.set requires index and character", loc);
            if (!args[0].isInt()) throw ScriptError("String index must be an integer", loc);
            if (!args[1].isString()) throw ScriptError("string.set value must be a string", loc);
            int64_t idx = args[0].asInt();
            if (idx < 0) idx += static_cast<int64_t>(str.size());
            if (idx < 0 || idx >= static_cast<int64_t>(str.size())) {
                throw ScriptError("String index out of bounds", loc);
            }
            const auto& replacement = args[1].asString();
            str.replace(static_cast<size_t>(idx), 1, replacement);
            return object;
        }
        if (methodSym == sym_push_) {
            if (args.empty()) throw ScriptError("string.push requires a string argument", loc);
            if (!args[0].isString()) throw ScriptError("string.push argument must be a string", loc);
            str += args[0].asString();
            return object;
        }
        if (methodSym == sym_insert_) {
            if (args.size() < 2) throw ScriptError("string.insert requires index and string", loc);
            if (!args[0].isInt()) throw ScriptError("Insert index must be an integer", loc);
            if (!args[1].isString()) throw ScriptError("Insert value must be a string", loc);
            int64_t idx = args[0].asInt();
            if (idx < 0) idx += static_cast<int64_t>(str.size());
            if (idx < 0 || idx > static_cast<int64_t>(str.size())) {
                throw ScriptError("String insert index out of bounds", loc);
            }
            str.insert(static_cast<size_t>(idx), args[1].asString());
            return object;
        }
        if (methodSym == sym_delete_) {
            if (args.empty()) throw ScriptError("string.delete requires a start index", loc);
            if (!args[0].isInt()) throw ScriptError("Delete start must be an integer", loc);
            int64_t start = args[0].asInt();
            if (start < 0) start += static_cast<int64_t>(str.size());
            if (start < 0 || start >= static_cast<int64_t>(str.size())) {
                throw ScriptError("String delete index out of bounds", loc);
            }
            size_t count = 1;
            if (args.size() > 1 && args[1].isInt()) {
                count = static_cast<size_t>(args[1].asInt());
            }
            str.erase(static_cast<size_t>(start), count);
            return object;
        }
        if (methodSym == sym_replace_) {
            if (args.size() < 2) throw ScriptError("string.replace requires old and new strings", loc);
            if (!args[0].isString() || !args[1].isString()) {
                throw ScriptError("string.replace arguments must be strings", loc);
            }
            const auto& oldStr = args[0].asString();
            const auto& newStr = args[1].asString();
            if (oldStr.empty()) return object;
            size_t pos = 0;
            while ((pos = str.find(oldStr, pos)) != std::string::npos) {
                str.replace(pos, oldStr.size(), newStr);
                pos += newStr.size();
            }
            return object;
        }
        if (methodSym == sym_find_) {
            if (args.empty()) throw ScriptError("string.find requires a search string", loc);
            if (!args[0].isString()) throw ScriptError("string.find argument must be a string", loc);
            size_t start = 0;
            if (args.size() > 1 && args[1].isInt()) {
                start = static_cast<size_t>(args[1].asInt());
            }
            auto pos = str.find(args[0].asString(), start);
            if (pos == std::string::npos) return Value::integer(-1);
            return Value::integer(static_cast<int64_t>(pos));
        }
        if (methodSym == sym_contains_) {
            if (args.empty()) throw ScriptError("string.contains requires a search string", loc);
            if (!args[0].isString()) throw ScriptError("string.contains argument must be a string", loc);
            return Value::boolean(str.find(args[0].asString()) != std::string::npos);
        }
        if (methodSym == sym_substr_) {
            if (args.empty()) throw ScriptError("string.substr requires a start index", loc);
            if (!args[0].isInt()) throw ScriptError("Substr start must be an integer", loc);
            int64_t start = args[0].asInt();
            if (start < 0) start += static_cast<int64_t>(str.size());
            if (start < 0) start = 0;
            if (start >= static_cast<int64_t>(str.size())) return Value::string("");
            if (args.size() > 1 && args[1].isInt()) {
                auto len = static_cast<size_t>(args[1].asInt());
                return Value::string(str.substr(static_cast<size_t>(start), len));
            }
            return Value::string(str.substr(static_cast<size_t>(start)));
        }
        if (methodSym == sym_slice_) {
            if (args.empty()) throw ScriptError("string.slice requires a start index", loc);
            if (!args[0].isInt()) throw ScriptError("Slice start must be an integer", loc);
            int64_t start = args[0].asInt();
            int64_t end = static_cast<int64_t>(str.size());
            if (args.size() > 1 && args[1].isInt()) end = args[1].asInt();
            if (start < 0) start += static_cast<int64_t>(str.size());
            if (end < 0) end += static_cast<int64_t>(str.size());
            start = std::max(int64_t(0), std::min(start, static_cast<int64_t>(str.size())));
            end = std::max(int64_t(0), std::min(end, static_cast<int64_t>(str.size())));
            if (start > end) start = end;
            return Value::string(str.substr(static_cast<size_t>(start),
                                            static_cast<size_t>(end - start)));
        }
        if (methodSym == sym_split_) {
            if (args.empty()) throw ScriptError("string.split requires a delimiter", loc);
            if (!args[0].isString()) throw ScriptError("Split delimiter must be a string", loc);
            const auto& delim = args[0].asString();
            std::vector<Value> parts;
            if (delim.empty()) {
                // Split into individual characters
                for (char c : str) {
                    parts.push_back(Value::string(std::string(1, c)));
                }
            } else {
                size_t pos = 0;
                size_t found;
                while ((found = str.find(delim, pos)) != std::string::npos) {
                    parts.push_back(Value::string(str.substr(pos, found - pos)));
                    pos = found + delim.size();
                }
                parts.push_back(Value::string(str.substr(pos)));
            }
            return Value::array(std::move(parts));
        }
        if (methodSym == sym_upper_) {
            std::string result = str;
            for (auto& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return Value::string(std::move(result));
        }
        if (methodSym == sym_lower_) {
            std::string result = str;
            for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return Value::string(std::move(result));
        }
        if (methodSym == sym_trim_) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) start++;
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) end--;
            return Value::string(str.substr(start, end - start));
        }
        if (methodSym == sym_starts_with_) {
            if (args.empty()) throw ScriptError("string.starts_with requires a string argument", loc);
            if (!args[0].isString()) throw ScriptError("string.starts_with argument must be a string", loc);
            const auto& prefix = args[0].asString();
            return Value::boolean(str.size() >= prefix.size() &&
                                  str.compare(0, prefix.size(), prefix) == 0);
        }
        if (methodSym == sym_ends_with_) {
            if (args.empty()) throw ScriptError("string.ends_with requires a string argument", loc);
            if (!args[0].isString()) throw ScriptError("string.ends_with argument must be a string", loc);
            const auto& suffix = args[0].asString();
            return Value::boolean(str.size() >= suffix.size() &&
                                  str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
        }
    }

    throw ScriptError("Unknown built-in method", loc);
}

// -- Binary operator application --

Value Evaluator::applyBinOp(const std::string& op, const Value& left, const Value& right,
                             SourceLocation loc) {
    // Range operators
    if (op == "..") {
        if (!left.isInt() || !right.isInt()) {
            throw ScriptError("Range operands must be integers", loc);
        }
        int64_t start = left.asInt();
        int64_t end = right.asInt();
        std::vector<Value> range;
        for (int64_t i = start; i < end; i++) {
            range.push_back(Value::integer(i));
        }
        return Value::array(std::move(range));
    }
    if (op == "..=") {
        if (!left.isInt() || !right.isInt()) {
            throw ScriptError("Range operands must be integers", loc);
        }
        int64_t start = left.asInt();
        int64_t end = right.asInt();
        std::vector<Value> range;
        for (int64_t i = start; i <= end; i++) {
            range.push_back(Value::integer(i));
        }
        return Value::array(std::move(range));
    }

    // Equality (works on all types)
    if (op == "==") return Value::boolean(left == right);
    if (op == "!=") return Value::boolean(left != right);

    // String concatenation with +
    if (op == "+" && left.isString() && right.isString()) {
        return Value::string(left.asString() + right.asString());
    }

    // Array concatenation with +
    if (op == "+" && left.isArray() && right.isArray()) {
        auto& leftArr = left.asArray();
        auto& rightArr = right.asArray();
        std::vector<Value> result;
        result.reserve(leftArr.size() + rightArr.size());
        result.insert(result.end(), leftArr.begin(), leftArr.end());
        result.insert(result.end(), rightArr.begin(), rightArr.end());
        return Value::array(std::move(result));
    }

    // String format with % (printf-style)
    // Single value: "%.2f" % 3.14
    // Multiple values: "%d/%d" % [10 20]
    if (op == "%" && left.isString()) {
        const auto& fmt = left.asString();
        if (right.isArray()) {
            return Value::string(formatMulti(fmt, right.asArray(), &interner_));
        }
        // Single value — use formatMulti with a one-element vector
        return Value::string(formatMulti(fmt, {right}, &interner_));
    }

    // Arithmetic and comparison (numeric)
    if (left.isNumeric() && right.isNumeric()) {
        bool useFloat = left.isFloat() || right.isFloat();

        if (op == "+") {
            if (useFloat) return Value::number(left.asNumber() + right.asNumber());
            return Value::integer(left.asInt() + right.asInt());
        }
        if (op == "-") {
            if (useFloat) return Value::number(left.asNumber() - right.asNumber());
            return Value::integer(left.asInt() - right.asInt());
        }
        if (op == "*") {
            if (useFloat) return Value::number(left.asNumber() * right.asNumber());
            return Value::integer(left.asInt() * right.asInt());
        }
        if (op == "/") {
            if (useFloat) {
                if (right.asNumber() == 0.0) throw ScriptError("Division by zero", loc);
                return Value::number(left.asNumber() / right.asNumber());
            }
            if (right.asInt() == 0) throw ScriptError("Division by zero", loc);
            return Value::integer(left.asInt() / right.asInt()); // truncating
        }
        if (op == "%") {
            if (useFloat) {
                if (right.asNumber() == 0.0) throw ScriptError("Modulo by zero", loc);
                return Value::number(std::fmod(left.asNumber(), right.asNumber()));
            }
            if (right.asInt() == 0) throw ScriptError("Modulo by zero", loc);
            return Value::integer(left.asInt() % right.asInt());
        }
        if (op == "<") return Value::boolean(left.asNumber() < right.asNumber());
        if (op == ">") return Value::boolean(left.asNumber() > right.asNumber());
        if (op == "<=") return Value::boolean(left.asNumber() <= right.asNumber());
        if (op == ">=") return Value::boolean(left.asNumber() >= right.asNumber());
    }

    // String comparison
    if (left.isString() && right.isString()) {
        if (op == "<") return Value::boolean(left.asString() < right.asString());
        if (op == ">") return Value::boolean(left.asString() > right.asString());
        if (op == "<=") return Value::boolean(left.asString() <= right.asString());
        if (op == ">=") return Value::boolean(left.asString() >= right.asString());
    }

    throw ScriptError("Cannot apply '" + op + "' to " + left.typeName() +
        " and " + right.typeName(), loc);
}

} // namespace finescript
