#pragma once

#include "token.h"
#include <optional>
#include <string>
#include <string_view>

namespace finescript {

class Lexer {
public:
    explicit Lexer(std::string_view source, uint16_t fileId = 0);

    Token next();
    Token peek();
    bool atEnd() const;
    SourceLocation currentLocation() const;

private:
    std::string source_;  // own a copy for stable storage
    size_t pos_ = 0;
    uint16_t fileId_;
    uint16_t line_ = 1;
    uint16_t column_ = 1;

    // Nesting depth for newline suppression
    int nestingDepth_ = 0;

    // String interpolation state
    bool inString_ = false;
    int interpBraceDepth_ = 0;

    // Whether previous non-whitespace character was a space (for hasLeadingSpace)
    bool lastWasSpace_ = true;

    // Peek cache
    std::optional<Token> peeked_;

    Token scanToken();
    Token scanNumber(size_t start);
    Token scanString();
    Token scanStringContinuation();
    Token scanName(size_t start);
    Token scanSymbolLiteral();

    void skipWhitespaceAndComments();
    char current() const;
    char peekChar() const;
    char peekChar(size_t offset) const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;
    SourceLocation loc() const;

    TokenType classifyKeyword(std::string_view text) const;
    Token makeToken(TokenType type, std::string text, SourceLocation location) const;
};

} // namespace finescript
