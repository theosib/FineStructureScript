#pragma once

#include "ast.h"
#include <memory>
#include <string_view>

namespace finescript {

class Parser {
public:
    /// Parse a full program (series of statements) into a Block AST node.
    static std::unique_ptr<AstNode> parse(std::string_view source, uint16_t fileId = 0);

    /// Parse a single expression (for REPL / command line).
    static std::unique_ptr<AstNode> parseExpression(std::string_view source, uint16_t fileId = 0);
};

} // namespace finescript
