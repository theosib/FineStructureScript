#pragma once

#include "source_location.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace finescript {

enum class AstNodeKind {
    IntLit,
    FloatLit,
    StringLit,
    StringInterp,
    SymbolLit,
    BoolLit,
    NilLit,
    ArrayLit,
    Name,
    DottedName,
    Call,
    Infix,
    UnaryNot,
    UnaryNegate,
    Block,
    If,
    For,
    While,
    Match,
    Set,
    Let,
    Fn,
    On,
    Return,
    Source,
    Index,
    Ref,
};

struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    int64_t intValue = 0;
    double floatValue = 0.0;
    std::string stringValue;
    bool boolValue = false;
    bool hasElse = false;

    std::vector<std::unique_ptr<AstNode>> children;
    std::string op;
    std::vector<std::string> nameParts;
};

// Factory functions
std::unique_ptr<AstNode> makeIntLit(int64_t val, SourceLocation loc);
std::unique_ptr<AstNode> makeFloatLit(double val, SourceLocation loc);
std::unique_ptr<AstNode> makeStringLit(std::string val, SourceLocation loc);
std::unique_ptr<AstNode> makeSymbolLit(std::string name, SourceLocation loc);
std::unique_ptr<AstNode> makeBoolLit(bool val, SourceLocation loc);
std::unique_ptr<AstNode> makeNilLit(SourceLocation loc);
std::unique_ptr<AstNode> makeName(std::string name, SourceLocation loc);
std::unique_ptr<AstNode> makeArrayLit(std::vector<std::unique_ptr<AstNode>> elems, SourceLocation loc);
std::unique_ptr<AstNode> makeStringInterp(std::vector<std::unique_ptr<AstNode>> parts, SourceLocation loc);
std::unique_ptr<AstNode> makeDottedName(std::unique_ptr<AstNode> base, std::vector<std::string> fields, SourceLocation loc);
std::unique_ptr<AstNode> makeCall(std::vector<std::unique_ptr<AstNode>> parts, SourceLocation loc);
std::unique_ptr<AstNode> makeInfix(std::string op, std::unique_ptr<AstNode> left, std::unique_ptr<AstNode> right, SourceLocation loc);
std::unique_ptr<AstNode> makeUnaryNot(std::unique_ptr<AstNode> operand, SourceLocation loc);
std::unique_ptr<AstNode> makeUnaryNegate(std::unique_ptr<AstNode> operand, SourceLocation loc);
std::unique_ptr<AstNode> makeBlock(std::vector<std::unique_ptr<AstNode>> stmts, SourceLocation loc);
std::unique_ptr<AstNode> makeIndex(std::unique_ptr<AstNode> target, std::unique_ptr<AstNode> index, SourceLocation loc);

// Macro nodes
std::unique_ptr<AstNode> makeSet(std::vector<std::string> target, std::unique_ptr<AstNode> value, SourceLocation loc);
std::unique_ptr<AstNode> makeLet(std::string name, std::unique_ptr<AstNode> value, SourceLocation loc);
std::unique_ptr<AstNode> makeFn(std::string name, std::vector<std::string> params, std::unique_ptr<AstNode> body, SourceLocation loc);
std::unique_ptr<AstNode> makeIf(std::vector<std::unique_ptr<AstNode>> conditionsAndBodies, bool hasElse, SourceLocation loc);
std::unique_ptr<AstNode> makeFor(std::string varName, std::unique_ptr<AstNode> iterable, std::unique_ptr<AstNode> body, SourceLocation loc);
std::unique_ptr<AstNode> makeWhile(std::unique_ptr<AstNode> condition, std::unique_ptr<AstNode> body, SourceLocation loc);
std::unique_ptr<AstNode> makeMatch(std::unique_ptr<AstNode> scrutinee, std::vector<std::unique_ptr<AstNode>> arms, SourceLocation loc);
std::unique_ptr<AstNode> makeOn(std::string eventName, std::unique_ptr<AstNode> body, SourceLocation loc);
std::unique_ptr<AstNode> makeReturn(std::unique_ptr<AstNode> value, SourceLocation loc);
std::unique_ptr<AstNode> makeSource(std::unique_ptr<AstNode> filename, SourceLocation loc);
std::unique_ptr<AstNode> makeRef(std::unique_ptr<AstNode> operand, SourceLocation loc);

} // namespace finescript
