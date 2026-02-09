#pragma once

#include "source_location.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace finescript {

enum class TokenType {
    // Literals
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    StringInterpStart,
    StringInterpMiddle,
    StringInterpEnd,
    SymbolLiteral,
    BoolTrue,
    BoolFalse,
    NilLiteral,

    // Identifiers
    Name,

    // Punctuation
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Dot,
    Semicolon,
    Newline,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    EqualEqual,
    BangEqual,
    DotDot,
    DotDotEqual,
    And,
    Or,
    Not,
    Tilde,
    NullCoalesce,    // ??
    FalsyCoalesce,   // ?:
    KeyName,         // =identifier (key specifier for named params / map literals)

    // Keywords
    Do,
    End,
    If,
    Elif,
    Else,
    For,
    In,
    While,
    Match,
    On,
    Fn,
    Set,
    Let,
    Return,
    Source,
    Underscore,

    Eof,
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string text;
    SourceLocation location;
    int64_t intValue = 0;
    double floatValue = 0.0;
    bool hasLeadingSpace = false;
};

const char* tokenTypeName(TokenType type);

} // namespace finescript
