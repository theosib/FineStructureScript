#include "finescript/lexer.h"
#include <stdexcept>
#include <cctype>

namespace finescript {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral: return "IntLiteral";
        case TokenType::FloatLiteral: return "FloatLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::StringInterpStart: return "StringInterpStart";
        case TokenType::StringInterpMiddle: return "StringInterpMiddle";
        case TokenType::StringInterpEnd: return "StringInterpEnd";
        case TokenType::SymbolLiteral: return "SymbolLiteral";
        case TokenType::BoolTrue: return "BoolTrue";
        case TokenType::BoolFalse: return "BoolFalse";
        case TokenType::NilLiteral: return "NilLiteral";
        case TokenType::Name: return "Name";
        case TokenType::LeftBrace: return "LeftBrace";
        case TokenType::RightBrace: return "RightBrace";
        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::LeftBracket: return "LeftBracket";
        case TokenType::RightBracket: return "RightBracket";
        case TokenType::Dot: return "Dot";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Newline: return "Newline";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Percent: return "Percent";
        case TokenType::Less: return "Less";
        case TokenType::Greater: return "Greater";
        case TokenType::LessEqual: return "LessEqual";
        case TokenType::GreaterEqual: return "GreaterEqual";
        case TokenType::EqualEqual: return "EqualEqual";
        case TokenType::BangEqual: return "BangEqual";
        case TokenType::DotDot: return "DotDot";
        case TokenType::DotDotEqual: return "DotDotEqual";
        case TokenType::And: return "And";
        case TokenType::Or: return "Or";
        case TokenType::Not: return "Not";
        case TokenType::Tilde: return "Tilde";
        case TokenType::Do: return "Do";
        case TokenType::End: return "End";
        case TokenType::If: return "If";
        case TokenType::Elif: return "Elif";
        case TokenType::Else: return "Else";
        case TokenType::For: return "For";
        case TokenType::In: return "In";
        case TokenType::While: return "While";
        case TokenType::Match: return "Match";
        case TokenType::On: return "On";
        case TokenType::Fn: return "Fn";
        case TokenType::Set: return "Set";
        case TokenType::Let: return "Let";
        case TokenType::Return: return "Return";
        case TokenType::Source: return "Source";
        case TokenType::Underscore: return "Underscore";
        case TokenType::Eof: return "Eof";
    }
    return "Unknown";
}

Lexer::Lexer(std::string_view source, uint16_t fileId)
    : source_(source), fileId_(fileId) {}

Token Lexer::next() {
    if (peeked_) {
        Token t = std::move(*peeked_);
        peeked_.reset();
        return t;
    }
    return scanToken();
}

Token Lexer::peek() {
    if (!peeked_) {
        peeked_ = scanToken();
    }
    return *peeked_;
}

bool Lexer::atEnd() const {
    if (peeked_) return peeked_->type == TokenType::Eof;
    return pos_ >= source_.size();
}

SourceLocation Lexer::currentLocation() const {
    return {fileId_, line_, column_};
}

char Lexer::current() const {
    return pos_ < source_.size() ? source_[pos_] : '\0';
}

char Lexer::peekChar() const {
    return (pos_ + 1) < source_.size() ? source_[pos_ + 1] : '\0';
}

char Lexer::peekChar(size_t offset) const {
    size_t idx = pos_ + offset;
    return idx < source_.size() ? source_[idx] : '\0';
}

char Lexer::advance() {
    char c = current();
    pos_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (current() == expected) {
        advance();
        return true;
    }
    return false;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

SourceLocation Lexer::loc() const {
    return {fileId_, line_, column_};
}

Token Lexer::makeToken(TokenType type, std::string text, SourceLocation location) const {
    Token t;
    t.type = type;
    t.text = std::move(text);
    t.location = location;
    t.hasLeadingSpace = lastWasSpace_;
    return t;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') {
            lastWasSpace_ = true;
            advance();
        } else if (c == '#') {
            // Line comment — skip to end of line
            while (!isAtEnd() && current() != '\n') advance();
        } else {
            break;
        }
    }
}

TokenType Lexer::classifyKeyword(std::string_view text) const {
    if (text == "do") return TokenType::Do;
    if (text == "end") return TokenType::End;
    if (text == "if") return TokenType::If;
    if (text == "elif") return TokenType::Elif;
    if (text == "else") return TokenType::Else;
    if (text == "for") return TokenType::For;
    if (text == "in") return TokenType::In;
    if (text == "while") return TokenType::While;
    if (text == "match") return TokenType::Match;
    if (text == "on") return TokenType::On;
    if (text == "fn") return TokenType::Fn;
    if (text == "set") return TokenType::Set;
    if (text == "let") return TokenType::Let;
    if (text == "return") return TokenType::Return;
    if (text == "source") return TokenType::Source;
    if (text == "true") return TokenType::BoolTrue;
    if (text == "false") return TokenType::BoolFalse;
    if (text == "nil") return TokenType::NilLiteral;
    if (text == "and") return TokenType::And;
    if (text == "or") return TokenType::Or;
    if (text == "not") return TokenType::Not;
    if (text == "_") return TokenType::Underscore;
    return TokenType::Name;
}

static bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Token Lexer::scanNumber(size_t start) {
    auto startLoc = loc();
    bool isFloat = false;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
        advance();
    }

    // Check for decimal point (but not .. range operator)
    if (current() == '.' && peekChar() != '.') {
        isFloat = true;
        advance(); // consume '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
            advance();
        }
    }

    std::string text = source_.substr(start, pos_ - start);

    Token t = makeToken(isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral,
                        text, startLoc);
    if (isFloat) {
        t.floatValue = std::stod(text);
    } else {
        t.intValue = std::stoll(text);
    }
    return t;
}

Token Lexer::scanName(size_t start) {
    auto startLoc = loc();

    while (!isAtEnd() && isIdentChar(current())) {
        advance();
    }

    std::string text = source_.substr(start, pos_ - start);
    TokenType type = classifyKeyword(text);

    return makeToken(type, std::move(text), startLoc);
}

Token Lexer::scanSymbolLiteral() {
    auto startLoc = loc();
    advance(); // consume ':'

    size_t nameStart = pos_;
    while (!isAtEnd() && isIdentChar(current())) {
        advance();
    }
    std::string name = source_.substr(nameStart, pos_ - nameStart);
    return makeToken(TokenType::SymbolLiteral, std::move(name), startLoc);
}

// Process escape sequences in a string segment
static std::string processEscapes(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            i++;
            switch (raw[i]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case '{': result += '{'; break;
                case '}': result += '}'; break;
                default: result += '\\'; result += raw[i]; break;
            }
        } else {
            result += raw[i];
        }
    }
    return result;
}

Token Lexer::scanString() {
    auto startLoc = loc();
    advance(); // consume opening '"'

    std::string text;

    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            text += advance(); // the backslash
            if (!isAtEnd()) text += advance(); // the escaped char
        } else if (current() == '{') {
            // Start of interpolation
            inString_ = true;
            interpBraceDepth_ = 1;
            advance(); // consume '{'

            std::string processed = processEscapes(text);
            return makeToken(TokenType::StringInterpStart, std::move(processed), startLoc);
        } else {
            text += advance();
        }
    }

    if (isAtEnd()) {
        throw std::runtime_error("Unterminated string literal");
    }
    advance(); // consume closing '"'

    std::string processed = processEscapes(text);
    return makeToken(TokenType::StringLiteral, std::move(processed), startLoc);
}

Token Lexer::scanStringContinuation() {
    // We've just consumed a '}' that closed an interpolation expression.
    // Continue scanning the string.
    auto startLoc = loc();
    std::string text;

    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            text += advance();
            if (!isAtEnd()) text += advance();
        } else if (current() == '{') {
            // Another interpolation
            interpBraceDepth_ = 1;
            advance(); // consume '{'

            std::string processed = processEscapes(text);
            return makeToken(TokenType::StringInterpMiddle, std::move(processed), startLoc);
        } else {
            text += advance();
        }
    }

    if (isAtEnd()) {
        throw std::runtime_error("Unterminated string literal");
    }
    advance(); // consume closing '"'
    inString_ = false;

    std::string processed = processEscapes(text);
    return makeToken(TokenType::StringInterpEnd, std::move(processed), startLoc);
}

Token Lexer::scanToken() {
    // If we're inside a string interpolation and the brace depth has returned to 0,
    // continue scanning the string.
    if (inString_ && interpBraceDepth_ == 0) {
        return scanStringContinuation();
    }

    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return makeToken(TokenType::Eof, "", loc());
    }

    auto startLoc = loc();
    char c = current();

    // Newline handling
    if (c == '\n') {
        advance();
        lastWasSpace_ = true;
        // Suppress newlines inside nesting delimiters
        if (nestingDepth_ > 0) {
            return scanToken(); // skip and get next token
        }
        // Collapse multiple newlines
        skipWhitespaceAndComments();
        while (!isAtEnd() && current() == '\n') {
            advance();
            skipWhitespaceAndComments();
        }
        return makeToken(TokenType::Newline, "\\n", startLoc);
    }

    bool hadLeadingSpace = lastWasSpace_;
    lastWasSpace_ = false;

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scanNumber(pos_);
    }

    // Identifiers and keywords
    if (isIdentStart(c)) {
        return scanName(pos_);
    }

    // String literals
    if (c == '"') {
        return scanString();
    }

    // Symbol literals (:name)
    if (c == ':' && !isAtEnd() && isIdentStart(peekChar())) {
        return scanSymbolLiteral();
    }

    // Single and multi-character tokens
    // Restore the captured leading space for punctuation tokens
    lastWasSpace_ = hadLeadingSpace;
    advance(); // consume the character

    switch (c) {
        case '{':
            if (inString_) {
                interpBraceDepth_++;
            } else {
                nestingDepth_++;
            }
            return makeToken(TokenType::LeftBrace, "{", startLoc);

        case '}':
            if (inString_) {
                interpBraceDepth_--;
                if (interpBraceDepth_ == 0) {
                    // End of interpolation expression — immediately continue scanning string
                    return scanStringContinuation();
                }
            } else {
                if (nestingDepth_ > 0) nestingDepth_--;
            }
            return makeToken(TokenType::RightBrace, "}", startLoc);

        case '(':
            nestingDepth_++;
            return makeToken(TokenType::LeftParen, "(", startLoc);

        case ')':
            if (nestingDepth_ > 0) nestingDepth_--;
            return makeToken(TokenType::RightParen, ")", startLoc);

        case '[':
            nestingDepth_++;
            return makeToken(TokenType::LeftBracket, "[", startLoc);

        case ']':
            if (nestingDepth_ > 0) nestingDepth_--;
            return makeToken(TokenType::RightBracket, "]", startLoc);

        case ';':
            return makeToken(TokenType::Semicolon, ";", startLoc);

        case '+':
            return makeToken(TokenType::Plus, "+", startLoc);

        case '-':
            return makeToken(TokenType::Minus, "-", startLoc);

        case '*':
            return makeToken(TokenType::Star, "*", startLoc);

        case '/':
            return makeToken(TokenType::Slash, "/", startLoc);

        case '%':
            return makeToken(TokenType::Percent, "%", startLoc);

        case '.':
            if (current() == '.') {
                advance();
                if (current() == '=') {
                    advance();
                    return makeToken(TokenType::DotDotEqual, "..=", startLoc);
                }
                return makeToken(TokenType::DotDot, "..", startLoc);
            }
            return makeToken(TokenType::Dot, ".", startLoc);

        case '<':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::LessEqual, "<=", startLoc);
            }
            return makeToken(TokenType::Less, "<", startLoc);

        case '>':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::GreaterEqual, ">=", startLoc);
            }
            return makeToken(TokenType::Greater, ">", startLoc);

        case '=':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::EqualEqual, "==", startLoc);
            }
            throw std::runtime_error("Unexpected '=' — did you mean '=='?");

        case '!':
            if (current() == '=') {
                advance();
                return makeToken(TokenType::BangEqual, "!=", startLoc);
            }
            throw std::runtime_error("Unexpected '!' — did you mean '!='?");

        case '~':
            return makeToken(TokenType::Tilde, "~", startLoc);

        default:
            throw std::runtime_error(std::string("Unexpected character: '") + c + "'");
    }
}

} // namespace finescript
