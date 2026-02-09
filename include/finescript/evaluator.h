#pragma once

#include "value.h"
#include "scope.h"
#include "source_location.h"
#include <memory>

namespace finescript {

struct AstNode;
class Interner;
class ScriptEngine;
class ExecutionContext;

class Evaluator {
public:
    Evaluator(Interner& interner, std::shared_ptr<Scope> globalScope,
              ScriptEngine* engine = nullptr);

    Value eval(const AstNode& node, std::shared_ptr<Scope> scope,
               ExecutionContext* ctx = nullptr);

    /// Evaluate a program rooted at a shared AST. Closures created during
    /// evaluation will hold a reference to this root, keeping the AST alive.
    Value eval(std::shared_ptr<AstNode> root, std::shared_ptr<Scope> scope,
               ExecutionContext* ctx = nullptr);

    /// Call a closure or native function with the given arguments.
    Value callFunction(const Value& callable, std::vector<Value> args,
                       std::shared_ptr<Scope> scope, ExecutionContext* ctx,
                       SourceLocation callSite);

private:
    Interner& interner_;
    std::shared_ptr<Scope> globalScope_;
    ScriptEngine* engine_;
    std::shared_ptr<const AstNode> currentAstRoot_;

    // Pre-interned common symbols for fast dispatch
    uint32_t sym_get_, sym_set_, sym_has_, sym_remove_, sym_keys_;
    uint32_t sym_values_, sym_length_, sym_push_, sym_pop_;
    uint32_t sym_setMethod_, sym_slice_, sym_contains_, sym_sort_;
    uint32_t sym_map_, sym_filter_, sym_foreach_;
    // String method symbols
    uint32_t sym_insert_, sym_delete_, sym_replace_, sym_split_;
    uint32_t sym_substr_, sym_find_, sym_upper_, sym_lower_, sym_trim_;
    uint32_t sym_starts_with_, sym_ends_with_, sym_char_at_;
    uint32_t sym_sort_by_;

    void preInternSymbols();

    Value evalIntLit(const AstNode& node);
    Value evalFloatLit(const AstNode& node);
    Value evalStringLit(const AstNode& node);
    Value evalStringInterp(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalSymbolLit(const AstNode& node);
    Value evalBoolLit(const AstNode& node);
    Value evalNilLit(const AstNode& node);
    Value evalArrayLit(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalName(const AstNode& node, std::shared_ptr<Scope> scope);
    Value evalDottedName(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalCall(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalInfix(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalUnaryNot(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalUnaryNegate(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalBlock(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalIndex(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalRef(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalMapLit(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);

    Value evalSet(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalLet(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalFn(const AstNode& node, std::shared_ptr<Scope> scope);
    Value evalIf(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalFor(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalWhile(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalMatch(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalOn(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalReturn(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);
    Value evalSource(const AstNode& node, std::shared_ptr<Scope> scope, ExecutionContext* ctx);

    Value callClosure(Closure& closure, std::vector<Value> args,
                      ExecutionContext* ctx, SourceLocation callSite);
    Value callClosureWithNamed(Closure& closure, std::vector<Value> posArgs,
                               std::vector<std::pair<uint32_t, Value>> namedArgs,
                               ExecutionContext* ctx, SourceLocation callSite);

    // Built-in map/array method dispatch
    Value dispatchBuiltinMethod(const Value& object, uint32_t methodSymbol,
                                std::vector<Value> args, std::shared_ptr<Scope> scope,
                                ExecutionContext* ctx, SourceLocation loc);
    bool isBuiltinMapMethod(uint32_t sym) const;
    bool isBuiltinArrayMethod(uint32_t sym) const;
    bool isBuiltinStringMethod(uint32_t sym) const;

    Value applyBinOp(const std::string& op, const Value& left, const Value& right,
                     SourceLocation loc);
};

} // namespace finescript
