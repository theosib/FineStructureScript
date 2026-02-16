#include "finescript/parser.h"
#include "finescript/ast.h"
#include "finescript/lexer.h"
#include <stdexcept>
#include <initializer_list>

namespace finescript {

// -- AST factory functions --

std::unique_ptr<AstNode> makeIntLit(int64_t val, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::IntLit;
    n->loc = loc;
    n->intValue = val;
    return n;
}

std::unique_ptr<AstNode> makeFloatLit(double val, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::FloatLit;
    n->loc = loc;
    n->floatValue = val;
    return n;
}

std::unique_ptr<AstNode> makeStringLit(std::string val, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::StringLit;
    n->loc = loc;
    n->stringValue = std::move(val);
    return n;
}

std::unique_ptr<AstNode> makeSymbolLit(std::string name, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::SymbolLit;
    n->loc = loc;
    n->stringValue = std::move(name);
    return n;
}

std::unique_ptr<AstNode> makeBoolLit(bool val, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::BoolLit;
    n->loc = loc;
    n->boolValue = val;
    return n;
}

std::unique_ptr<AstNode> makeNilLit(SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::NilLit;
    n->loc = loc;
    return n;
}

std::unique_ptr<AstNode> makeName(std::string name, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Name;
    n->loc = loc;
    n->stringValue = std::move(name);
    return n;
}

std::unique_ptr<AstNode> makeArrayLit(std::vector<std::unique_ptr<AstNode>> elems, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::ArrayLit;
    n->loc = loc;
    n->children = std::move(elems);
    return n;
}

std::unique_ptr<AstNode> makeStringInterp(std::vector<std::unique_ptr<AstNode>> parts, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::StringInterp;
    n->loc = loc;
    n->children = std::move(parts);
    return n;
}

std::unique_ptr<AstNode> makeDottedName(std::unique_ptr<AstNode> base, std::vector<std::string> fields, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::DottedName;
    n->loc = loc;
    n->children.push_back(std::move(base));
    n->nameParts = std::move(fields);
    return n;
}

std::unique_ptr<AstNode> makeCall(std::vector<std::unique_ptr<AstNode>> parts, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Call;
    n->loc = loc;
    n->children = std::move(parts);
    return n;
}

std::unique_ptr<AstNode> makeInfix(std::string op, std::unique_ptr<AstNode> left, std::unique_ptr<AstNode> right, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Infix;
    n->loc = loc;
    n->op = std::move(op);
    n->children.push_back(std::move(left));
    n->children.push_back(std::move(right));
    return n;
}

std::unique_ptr<AstNode> makeUnaryNot(std::unique_ptr<AstNode> operand, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::UnaryNot;
    n->loc = loc;
    n->children.push_back(std::move(operand));
    return n;
}

std::unique_ptr<AstNode> makeUnaryNegate(std::unique_ptr<AstNode> operand, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::UnaryNegate;
    n->loc = loc;
    n->children.push_back(std::move(operand));
    return n;
}

std::unique_ptr<AstNode> makeBlock(std::vector<std::unique_ptr<AstNode>> stmts, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Block;
    n->loc = loc;
    n->children = std::move(stmts);
    return n;
}

std::unique_ptr<AstNode> makeIndex(std::unique_ptr<AstNode> target, std::unique_ptr<AstNode> index, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Index;
    n->loc = loc;
    n->children.push_back(std::move(target));
    n->children.push_back(std::move(index));
    return n;
}

std::unique_ptr<AstNode> makeSet(std::vector<std::string> target, std::unique_ptr<AstNode> value, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Set;
    n->loc = loc;
    n->nameParts = std::move(target);
    n->children.push_back(std::move(value));
    return n;
}

std::unique_ptr<AstNode> makeLet(std::string name, std::unique_ptr<AstNode> value, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Let;
    n->loc = loc;
    n->nameParts.push_back(std::move(name));
    n->children.push_back(std::move(value));
    return n;
}

std::unique_ptr<AstNode> makeFn(std::string name, std::vector<std::string> params, std::unique_ptr<AstNode> body, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Fn;
    n->loc = loc;
    n->stringValue = std::move(name);
    n->intValue = static_cast<int64_t>(params.size()); // all params required (no defaults)
    n->nameParts = std::move(params);
    n->children.push_back(std::move(body));
    return n;
}

std::unique_ptr<AstNode> makeIf(std::vector<std::unique_ptr<AstNode>> conditionsAndBodies, bool hasElse, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::If;
    n->loc = loc;
    n->hasElse = hasElse;
    n->children = std::move(conditionsAndBodies);
    return n;
}

std::unique_ptr<AstNode> makeFor(std::string varName, std::unique_ptr<AstNode> iterable, std::unique_ptr<AstNode> body, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::For;
    n->loc = loc;
    n->nameParts.push_back(std::move(varName));
    n->children.push_back(std::move(iterable));
    n->children.push_back(std::move(body));
    return n;
}

std::unique_ptr<AstNode> makeWhile(std::unique_ptr<AstNode> condition, std::unique_ptr<AstNode> body, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::While;
    n->loc = loc;
    n->children.push_back(std::move(condition));
    n->children.push_back(std::move(body));
    return n;
}

std::unique_ptr<AstNode> makeMatch(std::unique_ptr<AstNode> scrutinee, std::vector<std::unique_ptr<AstNode>> arms, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Match;
    n->loc = loc;
    n->children.push_back(std::move(scrutinee));
    for (auto& arm : arms) {
        n->children.push_back(std::move(arm));
    }
    return n;
}

std::unique_ptr<AstNode> makeOn(std::string eventName, std::unique_ptr<AstNode> body, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::On;
    n->loc = loc;
    n->stringValue = std::move(eventName);
    n->children.push_back(std::move(body));
    return n;
}

std::unique_ptr<AstNode> makeReturn(std::unique_ptr<AstNode> value, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Return;
    n->loc = loc;
    if (value) n->children.push_back(std::move(value));
    return n;
}

std::unique_ptr<AstNode> makeSource(std::unique_ptr<AstNode> filename, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Source;
    n->loc = loc;
    n->children.push_back(std::move(filename));
    return n;
}

std::unique_ptr<AstNode> makeRef(std::unique_ptr<AstNode> operand, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::Ref;
    n->loc = loc;
    n->children.push_back(std::move(operand));
    return n;
}

std::unique_ptr<AstNode> makeMapLit(std::vector<std::string> keys, std::vector<std::unique_ptr<AstNode>> values, SourceLocation loc) {
    auto n = std::make_unique<AstNode>();
    n->kind = AstNodeKind::MapLit;
    n->loc = loc;
    n->nameParts = std::move(keys);
    n->children = std::move(values);
    return n;
}

// -- Parser implementation --

namespace {

class ParserImpl {
    Lexer lexer_;

public:
    ParserImpl(std::string_view source, uint16_t fileId)
        : lexer_(source, fileId) {}

    std::unique_ptr<AstNode> parseProgram() {
        auto loc = peekLoc();
        auto stmts = parseStatementsUntil({TokenType::Eof});
        expect(TokenType::Eof, "Expected end of input");
        return makeBlock(std::move(stmts), loc);
    }

    std::unique_ptr<AstNode> parseSingleExpression() {
        skipNewlines();
        if (lexer_.peek().type == TokenType::Eof) {
            return makeNilLit(peekLoc());
        }
        return parseStatement();
    }

private:
    // ---- Statement parsing ----

    std::vector<std::unique_ptr<AstNode>> parseStatementsUntil(
        std::initializer_list<TokenType> terminators)
    {
        std::vector<std::unique_ptr<AstNode>> stmts;
        skipNewlines();
        while (!isOneOf(lexer_.peek().type, terminators)) {
            stmts.push_back(parseStatement());
            while (lexer_.peek().type == TokenType::Newline ||
                   lexer_.peek().type == TokenType::Semicolon) {
                lexer_.next();
            }
        }
        return stmts;
    }

    std::unique_ptr<AstNode> parseStatement() {
        switch (lexer_.peek().type) {
            case TokenType::Set:       return parseSet();
            case TokenType::Let:       return parseLet();
            case TokenType::Fn:        return parseFn();
            case TokenType::If:        return parseIf();
            case TokenType::For:       return parseFor();
            case TokenType::While:     return parseWhile();
            case TokenType::Match:     return parseMatch();
            case TokenType::On:        return parseOn();
            case TokenType::Return:    return parseReturn();
            case TokenType::Source:    return parseSource();
            case TokenType::Do:        return parseDoBlock();
            case TokenType::LeftBrace: return parseBraceExpr();
            case TokenType::NullCoalesce:  return parseCoalescePrefix();
            case TokenType::FalsyCoalesce: return parseCoalescePrefix();
            default:                   return parsePrefixCall();
        }
    }

    // ---- Prefix call ----

    std::unique_ptr<AstNode> parsePrefixCall() {
        auto loc = peekLoc();
        std::vector<std::unique_ptr<AstNode>> parts;
        parts.push_back(parseAtom());
        while (isAtomStart()) {
            parts.push_back(parseAtom());
        }

        // Check for named arguments (=key value pairs)
        std::vector<std::string> namedKeys;
        while (lexer_.peek().type == TokenType::KeyName) {
            namedKeys.push_back(lexer_.next().text);
            parts.push_back(parseAtom());
        }

        if (parts.size() == 1 && namedKeys.empty()) {
            // A bare Name or DottedName in statement position is a zero-arg call.
            // Literals (int, string, etc.) remain as-is.
            auto kind = parts[0]->kind;
            if (kind == AstNodeKind::Name || kind == AstNodeKind::DottedName) {
                return makeCall(std::move(parts), loc);
            }
            return std::move(parts[0]);
        }
        auto callNode = makeCall(std::move(parts), loc);
        if (!namedKeys.empty()) {
            callNode->nameParts = std::move(namedKeys);
        }
        return callNode;
    }

    // ---- Atom parsing ----

    std::unique_ptr<AstNode> parseAtom() {
        auto tok = lexer_.peek();
        std::unique_ptr<AstNode> node;

        switch (tok.type) {
            case TokenType::IntLiteral:
                lexer_.next();
                node = makeIntLit(tok.intValue, tok.location);
                break;
            case TokenType::FloatLiteral:
                lexer_.next();
                node = makeFloatLit(tok.floatValue, tok.location);
                break;
            case TokenType::StringLiteral:
                lexer_.next();
                node = makeStringLit(tok.text, tok.location);
                break;
            case TokenType::StringInterpStart:
                node = parseStringInterpolation();
                break;
            case TokenType::SymbolLiteral:
                lexer_.next();
                node = makeSymbolLit(tok.text, tok.location);
                break;
            case TokenType::BoolTrue:
                lexer_.next();
                node = makeBoolLit(true, tok.location);
                break;
            case TokenType::BoolFalse:
                lexer_.next();
                node = makeBoolLit(false, tok.location);
                break;
            case TokenType::NilLiteral:
                lexer_.next();
                node = makeNilLit(tok.location);
                break;
            case TokenType::Name:
                lexer_.next();
                node = makeName(tok.text, tok.location);
                break;
            case TokenType::Underscore:
                lexer_.next();
                node = makeName("_", tok.location);
                break;
            case TokenType::LeftParen:
                node = parseParenExpr();
                break;
            case TokenType::LeftBrace:
                node = parseBraceExpr();
                break;
            case TokenType::LeftBracket:
                node = parseArrayLiteral();
                break;
            case TokenType::Minus: {
                lexer_.next();
                auto operand = parseAtom();
                node = makeUnaryNegate(std::move(operand), tok.location);
                break;
            }
            case TokenType::Not: {
                lexer_.next();
                auto operand = parseAtom();
                node = makeUnaryNot(std::move(operand), tok.location);
                break;
            }
            case TokenType::Tilde: {
                lexer_.next();
                auto operand = parseAtom();
                node = makeRef(std::move(operand), tok.location);
                break;
            }
            case TokenType::Fn:
                node = parseFn();
                break;
            case TokenType::Do:
                node = parseDoBlock();
                break;
            default:
                throw std::runtime_error(
                    std::string("Unexpected token: ") + tokenTypeName(tok.type) +
                    " at line " + std::to_string(tok.location.line) +
                    ", column " + std::to_string(tok.location.column));
        }

        return parsePostfix(std::move(node));
    }

    std::unique_ptr<AstNode> parsePostfix(std::unique_ptr<AstNode> base) {
        while (true) {
            if (lexer_.peek().type == TokenType::Dot) {
                lexer_.next();
                auto field = expectFieldName("Expected field name after '.'");
                if (base->kind == AstNodeKind::DottedName) {
                    base->nameParts.push_back(field.text);
                } else {
                    std::vector<std::string> fields;
                    fields.push_back(field.text);
                    auto loc = base->loc;
                    base = makeDottedName(std::move(base), std::move(fields), loc);
                }
            } else if (lexer_.peek().type == TokenType::LeftBracket &&
                       !lexer_.peek().hasLeadingSpace) {
                auto loc = lexer_.peek().location;
                lexer_.next(); // consume '['
                auto index = parseInfix(0);
                expect(TokenType::RightBracket, "Expected ']'");
                base = makeIndex(std::move(base), std::move(index), loc);
            } else {
                break;
            }
        }
        return base;
    }

    // ---- Delimited expressions ----

    std::unique_ptr<AstNode> parseParenExpr() {
        lexer_.next(); // consume '('
        auto expr = parseInfix(0);
        expect(TokenType::RightParen, "Expected ')'");
        return expr;
    }

    std::unique_ptr<AstNode> parseBraceExpr() {
        auto loc = peekLoc();
        lexer_.next(); // consume '{'

        // Check for map literal: {=key val =key val ...}
        if (lexer_.peek().type == TokenType::KeyName) {
            return parseMapLiteralBody(loc);
        }

        auto stmts = parseStatementsUntil({TokenType::RightBrace});
        expect(TokenType::RightBrace, "Expected '}'");
        if (stmts.size() == 1) {
            return std::move(stmts[0]);
        }
        return makeBlock(std::move(stmts), loc);
    }

    std::unique_ptr<AstNode> parseMapLiteralBody(SourceLocation loc) {
        std::vector<std::string> keys;
        std::vector<std::unique_ptr<AstNode>> values;

        while (lexer_.peek().type == TokenType::KeyName) {
            keys.push_back(lexer_.next().text);
            values.push_back(parseAtom());
        }

        expect(TokenType::RightBrace, "Expected '}'");
        return makeMapLit(std::move(keys), std::move(values), loc);
    }

    std::unique_ptr<AstNode> parseArrayLiteral() {
        auto loc = peekLoc();
        lexer_.next(); // consume '['
        std::vector<std::unique_ptr<AstNode>> elems;
        while (lexer_.peek().type != TokenType::RightBracket) {
            elems.push_back(parseAtom());
        }
        expect(TokenType::RightBracket, "Expected ']'");
        return makeArrayLit(std::move(elems), loc);
    }

    std::unique_ptr<AstNode> parseDoBlock() {
        auto loc = peekLoc();
        lexer_.next(); // consume 'do'
        auto stmts = parseStatementsUntil({TokenType::End});
        expect(TokenType::End, "Expected 'end'");
        return makeBlock(std::move(stmts), loc);
    }

    // ---- String interpolation ----

    std::unique_ptr<AstNode> parseStringInterpolation() {
        auto startTok = lexer_.next(); // consume StringInterpStart
        auto loc = startTok.location;
        std::vector<std::unique_ptr<AstNode>> parts;

        if (!startTok.text.empty()) {
            parts.push_back(makeStringLit(startTok.text, loc));
        }

        while (true) {
            parts.push_back(parsePrefixCall());

            if (lexer_.peek().type == TokenType::StringInterpMiddle) {
                auto mid = lexer_.next();
                if (!mid.text.empty()) {
                    parts.push_back(makeStringLit(mid.text, mid.location));
                }
            } else if (lexer_.peek().type == TokenType::StringInterpEnd) {
                auto endTok = lexer_.next();
                if (!endTok.text.empty()) {
                    parts.push_back(makeStringLit(endTok.text, endTok.location));
                }
                break;
            } else {
                throw std::runtime_error("Expected string interpolation continuation");
            }
        }

        return makeStringInterp(std::move(parts), loc);
    }

    // ---- Infix expression parsing (Pratt parser) ----

    std::unique_ptr<AstNode> parseInfix(int minPrec) {
        auto left = parseInfixPrimary();

        while (true) {
            int prec = infixPrecedence(lexer_.peek().type);
            if (prec < 0 || prec < minPrec) break;

            auto opTok = lexer_.next();
            auto right = parseInfix(prec + 1); // left-associative
            left = makeInfix(opTok.text, std::move(left), std::move(right), opTok.location);
        }

        return left;
    }

    std::unique_ptr<AstNode> parseInfixPrimary() {
        auto tok = lexer_.peek();
        if (tok.type == TokenType::Not) {
            lexer_.next();
            auto operand = parseInfixPrimary();
            return makeUnaryNot(std::move(operand), tok.location);
        }
        if (tok.type == TokenType::Minus) {
            lexer_.next();
            auto operand = parseInfixPrimary();
            return makeUnaryNegate(std::move(operand), tok.location);
        }
        return parseAtom();
    }

    // ---- Macro parsers ----

    std::unique_ptr<AstNode> parseSet() {
        auto loc = lexer_.next().location; // consume 'set'
        auto nameTok = expect(TokenType::Name, "Expected variable name after 'set'");
        std::vector<std::string> target;
        target.push_back(nameTok.text);
        while (lexer_.peek().type == TokenType::Dot) {
            lexer_.next();
            auto field = expectFieldName("Expected field name after '.'");
            target.push_back(field.text);
        }
        auto value = parseAtom();
        return makeSet(std::move(target), std::move(value), loc);
    }

    std::unique_ptr<AstNode> parseLet() {
        auto loc = lexer_.next().location; // consume 'let'
        auto nameTok = expect(TokenType::Name, "Expected variable name after 'let'");
        auto value = parseAtom();
        return makeLet(nameTok.text, std::move(value), loc);
    }

    std::unique_ptr<AstNode> parseFn() {
        auto loc = lexer_.next().location; // consume 'fn'
        std::string name;

        if (lexer_.peek().type == TokenType::Name) {
            auto nameTok = lexer_.next();
            if (lexer_.peek().type == TokenType::LeftBracket) {
                name = nameTok.text;
            } else {
                throw std::runtime_error(
                    "Expected '[' after function name at line " +
                    std::to_string(nameTok.location.line));
            }
        }

        expect(TokenType::LeftBracket, "Expected '[' for parameter list");
        std::vector<std::string> params;
        std::vector<std::unique_ptr<AstNode>> defaults;
        int numRequired = 0;
        bool seenOptional = false;
        std::string restName;
        std::string kwargsName;
        bool seenRest = false;
        bool seenKwargs = false;

        while (lexer_.peek().type != TokenType::RightBracket) {
            if (seenKwargs) {
                throw std::runtime_error(
                    "No parameters allowed after {kwargs} collector");
            }
            if (lexer_.peek().type == TokenType::LeftBracket) {
                // [rest] — variadic positional args collector
                if (seenRest) {
                    throw std::runtime_error("Only one [rest] parameter allowed");
                }
                lexer_.next(); // consume [
                auto p = expect(TokenType::Name, "Expected rest parameter name");
                expect(TokenType::RightBracket, "Expected ']' after rest parameter name");
                restName = p.text;
                seenRest = true;
            } else if (lexer_.peek().type == TokenType::LeftBrace) {
                // {kwargs} — variadic named args collector
                if (seenKwargs) {
                    throw std::runtime_error("Only one {kwargs} parameter allowed");
                }
                lexer_.next(); // consume {
                auto p = expect(TokenType::Name, "Expected kwargs parameter name");
                expect(TokenType::RightBrace, "Expected '}' after kwargs parameter name");
                kwargsName = p.text;
                seenKwargs = true;
            } else if (lexer_.peek().type == TokenType::KeyName) {
                // Optional param with default: =name default_value
                if (seenRest) {
                    throw std::runtime_error(
                        "Default parameters must come before [rest] collector");
                }
                seenOptional = true;
                auto keyTok = lexer_.next();
                params.push_back(keyTok.text);
                defaults.push_back(parseAtom());
            } else {
                if (seenRest) {
                    throw std::runtime_error(
                        "Required parameters must come before [rest] collector");
                }
                if (seenOptional) {
                    throw std::runtime_error(
                        "Required parameters must come before optional parameters");
                }
                auto p = expect(TokenType::Name, "Expected parameter name");
                params.push_back(p.text);
                numRequired++;
            }
        }
        expect(TokenType::RightBracket, "Expected ']'");

        std::unique_ptr<AstNode> body;
        if (lexer_.peek().type == TokenType::Do) {
            lexer_.next();
            auto stmts = parseStatementsUntil({TokenType::End});
            expect(TokenType::End, "Expected 'end'");
            body = makeBlock(std::move(stmts), loc);
        } else {
            body = parseAtom();
        }

        // Build Fn node: children[0] = body, children[1..] = default exprs
        // intValue = numRequired, nameParts = all param names
        // op = "restName|kwargsName" for variadic params (pipe-delimited)
        auto n = std::make_unique<AstNode>();
        n->kind = AstNodeKind::Fn;
        n->loc = loc;
        n->stringValue = std::move(name);
        n->intValue = defaults.empty()
            ? static_cast<int64_t>(params.size())  // all required
            : static_cast<int64_t>(numRequired);
        n->nameParts = std::move(params);
        n->children.push_back(std::move(body));
        for (auto& def : defaults) {
            n->children.push_back(std::move(def));
        }
        if (!restName.empty() || !kwargsName.empty()) {
            n->op = restName + "|" + kwargsName;
        }
        return n;
    }

    std::unique_ptr<AstNode> parseIf() {
        auto loc = lexer_.next().location; // consume 'if'
        std::vector<std::unique_ptr<AstNode>> parts;
        bool hasElse = false;

        parts.push_back(parseAtom()); // condition

        if (lexer_.peek().type == TokenType::Do) {
            // Multi-line form
            lexer_.next();
            auto stmts = parseStatementsUntil(
                {TokenType::End, TokenType::Elif, TokenType::Else});
            parts.push_back(makeBlock(std::move(stmts), loc));

            while (lexer_.peek().type == TokenType::Elif) {
                lexer_.next();
                parts.push_back(parseAtom()); // elif condition
                expect(TokenType::Do, "Expected 'do' after elif condition");
                auto elifStmts = parseStatementsUntil(
                    {TokenType::End, TokenType::Elif, TokenType::Else});
                parts.push_back(makeBlock(std::move(elifStmts), loc));
            }

            if (lexer_.peek().type == TokenType::Else) {
                lexer_.next();
                expect(TokenType::Do, "Expected 'do' after else");
                auto elseStmts = parseStatementsUntil({TokenType::End});
                parts.push_back(makeBlock(std::move(elseStmts), loc));
                hasElse = true;
            }

            expect(TokenType::End, "Expected 'end'");
        } else if (lexer_.peek().type == TokenType::LeftBrace) {
            // One-line form: if COND {then} [{else}]
            parts.push_back(parseBraceExpr());
            if (lexer_.peek().type == TokenType::LeftBrace) {
                parts.push_back(parseBraceExpr());
                hasElse = true;
            }
        } else {
            throw std::runtime_error(
                "Expected '{' or 'do' after if condition at line " +
                std::to_string(loc.line));
        }

        return makeIf(std::move(parts), hasElse, loc);
    }

    std::unique_ptr<AstNode> parseFor() {
        auto loc = lexer_.next().location; // consume 'for'
        auto varTok = expect(TokenType::Name, "Expected loop variable");
        expect(TokenType::In, "Expected 'in'");
        auto iterable = parseRangeOrAtom();
        expect(TokenType::Do, "Expected 'do'");
        auto stmts = parseStatementsUntil({TokenType::End});
        expect(TokenType::End, "Expected 'end'");
        return makeFor(varTok.text, std::move(iterable),
                       makeBlock(std::move(stmts), loc), loc);
    }

    std::unique_ptr<AstNode> parseWhile() {
        auto loc = lexer_.next().location; // consume 'while'
        auto condition = parseAtom();
        expect(TokenType::Do, "Expected 'do'");
        auto stmts = parseStatementsUntil({TokenType::End});
        expect(TokenType::End, "Expected 'end'");
        return makeWhile(std::move(condition),
                         makeBlock(std::move(stmts), loc), loc);
    }

    std::unique_ptr<AstNode> parseMatch() {
        auto loc = lexer_.next().location; // consume 'match'
        auto scrutinee = parseAtom();
        skipNewlines();

        std::vector<std::unique_ptr<AstNode>> arms;
        while (lexer_.peek().type != TokenType::End) {
            // Each arm: PATTERN BODY_STATEMENT
            arms.push_back(parseAtom()); // pattern
            arms.push_back(parseStatement()); // body (terminated by newline)
            // Consume newlines between arms
            while (lexer_.peek().type == TokenType::Newline ||
                   lexer_.peek().type == TokenType::Semicolon) {
                lexer_.next();
            }
        }
        expect(TokenType::End, "Expected 'end' after match");

        return makeMatch(std::move(scrutinee), std::move(arms), loc);
    }

    std::unique_ptr<AstNode> parseOn() {
        auto loc = lexer_.next().location; // consume 'on'
        std::string eventName;
        if (lexer_.peek().type == TokenType::SymbolLiteral) {
            eventName = lexer_.next().text;
        } else if (lexer_.peek().type == TokenType::Name) {
            eventName = lexer_.next().text;
        } else {
            throw std::runtime_error("Expected event name after 'on'");
        }

        std::unique_ptr<AstNode> body;
        if (lexer_.peek().type == TokenType::Do) {
            lexer_.next();
            auto stmts = parseStatementsUntil({TokenType::End});
            expect(TokenType::End, "Expected 'end'");
            body = makeBlock(std::move(stmts), loc);
        } else {
            body = parseAtom();
        }

        return makeOn(std::move(eventName), std::move(body), loc);
    }

    std::unique_ptr<AstNode> parseReturn() {
        auto loc = lexer_.next().location; // consume 'return'
        if (isStatementTerminator()) {
            return makeReturn(nullptr, loc);
        }
        return makeReturn(parseAtom(), loc);
    }

    std::unique_ptr<AstNode> parseSource() {
        auto loc = lexer_.next().location; // consume 'source'
        return makeSource(parseAtom(), loc);
    }

    std::unique_ptr<AstNode> parseCoalescePrefix() {
        auto tok = lexer_.next(); // consume '??' or '?:'
        auto expr = parseAtom();
        auto fallback = parseAtom();
        return makeInfix(tok.text, std::move(expr), std::move(fallback), tok.location);
    }

    // ---- Helpers ----

    std::unique_ptr<AstNode> parseRangeOrAtom() {
        auto left = parseAtom();
        if (lexer_.peek().type == TokenType::DotDot ||
            lexer_.peek().type == TokenType::DotDotEqual) {
            auto opTok = lexer_.next();
            auto right = parseAtom();
            return makeInfix(opTok.text, std::move(left), std::move(right), opTok.location);
        }
        return left;
    }

    Token expect(TokenType type, const char* msg) {
        auto tok = lexer_.next();
        if (tok.type != type) {
            throw std::runtime_error(
                std::string(msg) + " (got " + tokenTypeName(tok.type) +
                " at line " + std::to_string(tok.location.line) +
                ", column " + std::to_string(tok.location.column) + ")");
        }
        return tok;
    }

    // Accept Name or any keyword as a field/method name (after '.')
    Token expectFieldName(const char* msg) {
        auto tok = lexer_.next();
        if (tok.type == TokenType::Name || isKeywordToken(tok.type)) {
            return tok;
        }
        throw std::runtime_error(
            std::string(msg) + " (got " + tokenTypeName(tok.type) +
            " at line " + std::to_string(tok.location.line) +
            ", column " + std::to_string(tok.location.column) + ")");
    }

    static bool isKeywordToken(TokenType type) {
        switch (type) {
            case TokenType::Set: case TokenType::Let: case TokenType::Fn: case TokenType::If:
            case TokenType::Elif: case TokenType::Else: case TokenType::For:
            case TokenType::In: case TokenType::While: case TokenType::Match:
            case TokenType::On: case TokenType::Do: case TokenType::End:
            case TokenType::Return: case TokenType::Source: case TokenType::And:
            case TokenType::Or: case TokenType::Not: case TokenType::BoolTrue:
            case TokenType::BoolFalse: case TokenType::NilLiteral:
            case TokenType::Underscore:
                return true;
            default:
                return false;
        }
    }

    SourceLocation peekLoc() {
        return lexer_.peek().location;
    }

    void skipNewlines() {
        while (lexer_.peek().type == TokenType::Newline) {
            lexer_.next();
        }
    }

    bool isStatementTerminator() {
        auto type = lexer_.peek().type;
        return type == TokenType::Newline ||
               type == TokenType::Semicolon ||
               type == TokenType::Eof ||
               type == TokenType::End ||
               type == TokenType::Elif ||
               type == TokenType::Else ||
               type == TokenType::RightBrace ||
               type == TokenType::RightParen ||
               type == TokenType::RightBracket ||
               type == TokenType::StringInterpMiddle ||
               type == TokenType::StringInterpEnd;
    }

    bool isAtomStart() {
        auto type = lexer_.peek().type;
        return type == TokenType::IntLiteral ||
               type == TokenType::FloatLiteral ||
               type == TokenType::StringLiteral ||
               type == TokenType::StringInterpStart ||
               type == TokenType::SymbolLiteral ||
               type == TokenType::BoolTrue ||
               type == TokenType::BoolFalse ||
               type == TokenType::NilLiteral ||
               type == TokenType::Name ||
               type == TokenType::Underscore ||
               type == TokenType::LeftParen ||
               type == TokenType::LeftBrace ||
               type == TokenType::LeftBracket ||
               type == TokenType::Minus ||
               type == TokenType::Not ||
               type == TokenType::Tilde ||
               type == TokenType::Fn ||
               type == TokenType::Do;
    }

    static int infixPrecedence(TokenType type) {
        switch (type) {
            case TokenType::NullCoalesce:
            case TokenType::FalsyCoalesce: return 0;
            case TokenType::Or: return 1;
            case TokenType::And: return 2;
            case TokenType::EqualEqual:
            case TokenType::BangEqual: return 3;
            case TokenType::Less:
            case TokenType::Greater:
            case TokenType::LessEqual:
            case TokenType::GreaterEqual: return 4;
            case TokenType::DotDot:
            case TokenType::DotDotEqual: return 5;
            case TokenType::Plus:
            case TokenType::Minus: return 6;
            case TokenType::Star:
            case TokenType::Slash:
            case TokenType::Percent: return 7;
            default: return -1;
        }
    }

    static bool isOneOf(TokenType type, std::initializer_list<TokenType> list) {
        for (auto t : list) {
            if (type == t) return true;
        }
        return false;
    }
};

} // anonymous namespace

std::unique_ptr<AstNode> Parser::parse(std::string_view source, uint16_t fileId) {
    ParserImpl parser(source, fileId);
    return parser.parseProgram();
}

std::unique_ptr<AstNode> Parser::parseExpression(std::string_view source, uint16_t fileId) {
    ParserImpl parser(source, fileId);
    return parser.parseSingleExpression();
}

} // namespace finescript
