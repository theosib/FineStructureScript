#include <catch2/catch_test_macros.hpp>
#include "finescript/lexer.h"

using namespace finescript;

// Helper to collect all tokens
static std::vector<Token> tokenize(std::string_view source) {
    Lexer lexer(source);
    std::vector<Token> tokens;
    while (true) {
        Token t = lexer.next();
        tokens.push_back(t);
        if (t.type == TokenType::Eof) break;
    }
    return tokens;
}

TEST_CASE("Lexer empty input", "[lexer]") {
    auto tokens = tokenize("");
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::Eof);
}

TEST_CASE("Lexer whitespace only", "[lexer]") {
    auto tokens = tokenize("   \t  ");
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::Eof);
}

TEST_CASE("Lexer simple set statement", "[lexer]") {
    auto tokens = tokenize("set x 5");
    REQUIRE(tokens.size() >= 4);
    CHECK(tokens[0].type == TokenType::Set);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "x");
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[2].intValue == 5);
    CHECK(tokens[3].type == TokenType::Eof);
}

TEST_CASE("Lexer integer literals", "[lexer]") {
    auto tokens = tokenize("42 0 999");
    CHECK(tokens[0].type == TokenType::IntLiteral);
    CHECK(tokens[0].intValue == 42);
    CHECK(tokens[1].type == TokenType::IntLiteral);
    CHECK(tokens[1].intValue == 0);
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[2].intValue == 999);
}

TEST_CASE("Lexer float literals", "[lexer]") {
    auto tokens = tokenize("3.14 0.5 100.0");
    CHECK(tokens[0].type == TokenType::FloatLiteral);
    CHECK(tokens[0].floatValue == 3.14);
    CHECK(tokens[1].type == TokenType::FloatLiteral);
    CHECK(tokens[1].floatValue == 0.5);
    CHECK(tokens[2].type == TokenType::FloatLiteral);
    CHECK(tokens[2].floatValue == 100.0);
}

TEST_CASE("Lexer plain string", "[lexer]") {
    auto tokens = tokenize("\"hello world\"");
    REQUIRE(tokens.size() >= 2);
    CHECK(tokens[0].type == TokenType::StringLiteral);
    CHECK(tokens[0].text == "hello world");
}

TEST_CASE("Lexer string escape sequences", "[lexer]") {
    auto tokens = tokenize("\"hello\\nworld\"");
    CHECK(tokens[0].type == TokenType::StringLiteral);
    CHECK(tokens[0].text == "hello\nworld");

    tokens = tokenize("\"tab\\there\"");
    CHECK(tokens[0].text == "tab\there");

    tokens = tokenize("\"escaped\\\\backslash\"");
    CHECK(tokens[0].text == "escaped\\backslash");

    tokens = tokenize("\"escaped\\{brace\\}\"");
    CHECK(tokens[0].text == "escaped{brace}");
}

TEST_CASE("Lexer string interpolation simple", "[lexer]") {
    auto tokens = tokenize("\"Hello {name}\"");
    REQUIRE(tokens.size() >= 4);
    CHECK(tokens[0].type == TokenType::StringInterpStart);
    CHECK(tokens[0].text == "Hello ");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "name");
    CHECK(tokens[2].type == TokenType::StringInterpEnd);
    CHECK(tokens[2].text == "");
}

TEST_CASE("Lexer string interpolation with text after", "[lexer]") {
    auto tokens = tokenize("\"Hello {name}!\"");
    CHECK(tokens[0].type == TokenType::StringInterpStart);
    CHECK(tokens[0].text == "Hello ");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "name");
    CHECK(tokens[2].type == TokenType::StringInterpEnd);
    CHECK(tokens[2].text == "!");
}

TEST_CASE("Lexer string interpolation multiple", "[lexer]") {
    auto tokens = tokenize("\"x={x}, y={y}\"");
    CHECK(tokens[0].type == TokenType::StringInterpStart);
    CHECK(tokens[0].text == "x=");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "x");
    CHECK(tokens[2].type == TokenType::StringInterpMiddle);
    CHECK(tokens[2].text == ", y=");
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[3].text == "y");
    CHECK(tokens[4].type == TokenType::StringInterpEnd);
    CHECK(tokens[4].text == "");
}

TEST_CASE("Lexer string interpolation with expression", "[lexer]") {
    auto tokens = tokenize("\"Result: {add 3 4}\"");
    CHECK(tokens[0].type == TokenType::StringInterpStart);
    CHECK(tokens[0].text == "Result: ");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "add");
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[2].intValue == 3);
    CHECK(tokens[3].type == TokenType::IntLiteral);
    CHECK(tokens[3].intValue == 4);
    CHECK(tokens[4].type == TokenType::StringInterpEnd);
    CHECK(tokens[4].text == "");
}

TEST_CASE("Lexer symbol literals", "[lexer]") {
    auto tokens = tokenize(":stone :interact :_hidden");
    CHECK(tokens[0].type == TokenType::SymbolLiteral);
    CHECK(tokens[0].text == "stone");
    CHECK(tokens[1].type == TokenType::SymbolLiteral);
    CHECK(tokens[1].text == "interact");
    CHECK(tokens[2].type == TokenType::SymbolLiteral);
    CHECK(tokens[2].text == "_hidden");
}

TEST_CASE("Lexer boolean and nil", "[lexer]") {
    auto tokens = tokenize("true false nil");
    CHECK(tokens[0].type == TokenType::BoolTrue);
    CHECK(tokens[1].type == TokenType::BoolFalse);
    CHECK(tokens[2].type == TokenType::NilLiteral);
}

TEST_CASE("Lexer keywords", "[lexer]") {
    auto tokens = tokenize("set fn if elif else for in while match on do end return source");
    CHECK(tokens[0].type == TokenType::Set);
    CHECK(tokens[1].type == TokenType::Fn);
    CHECK(tokens[2].type == TokenType::If);
    CHECK(tokens[3].type == TokenType::Elif);
    CHECK(tokens[4].type == TokenType::Else);
    CHECK(tokens[5].type == TokenType::For);
    CHECK(tokens[6].type == TokenType::In);
    CHECK(tokens[7].type == TokenType::While);
    CHECK(tokens[8].type == TokenType::Match);
    CHECK(tokens[9].type == TokenType::On);
    CHECK(tokens[10].type == TokenType::Do);
    CHECK(tokens[11].type == TokenType::End);
    CHECK(tokens[12].type == TokenType::Return);
    CHECK(tokens[13].type == TokenType::Source);
}

TEST_CASE("Lexer logical operators as keywords", "[lexer]") {
    auto tokens = tokenize("and or not");
    CHECK(tokens[0].type == TokenType::And);
    CHECK(tokens[1].type == TokenType::Or);
    CHECK(tokens[2].type == TokenType::Not);
}

TEST_CASE("Lexer underscore wildcard", "[lexer]") {
    auto tokens = tokenize("_");
    CHECK(tokens[0].type == TokenType::Underscore);
}

TEST_CASE("Lexer identifiers not keywords", "[lexer]") {
    auto tokens = tokenize("x player health_bar my_fn");
    CHECK(tokens[0].type == TokenType::Name);
    CHECK(tokens[0].text == "x");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "player");
    CHECK(tokens[2].type == TokenType::Name);
    CHECK(tokens[2].text == "health_bar");
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[3].text == "my_fn");
}

TEST_CASE("Lexer arithmetic operators", "[lexer]") {
    auto tokens = tokenize("+ - * / %");
    CHECK(tokens[0].type == TokenType::Plus);
    CHECK(tokens[1].type == TokenType::Minus);
    CHECK(tokens[2].type == TokenType::Star);
    CHECK(tokens[3].type == TokenType::Slash);
    CHECK(tokens[4].type == TokenType::Percent);
}

TEST_CASE("Lexer comparison operators", "[lexer]") {
    auto tokens = tokenize("< > <= >= == !=");
    CHECK(tokens[0].type == TokenType::Less);
    CHECK(tokens[1].type == TokenType::Greater);
    CHECK(tokens[2].type == TokenType::LessEqual);
    CHECK(tokens[3].type == TokenType::GreaterEqual);
    CHECK(tokens[4].type == TokenType::EqualEqual);
    CHECK(tokens[5].type == TokenType::BangEqual);
}

TEST_CASE("Lexer range operators", "[lexer]") {
    auto tokens = tokenize("0..10");
    CHECK(tokens[0].type == TokenType::IntLiteral);
    CHECK(tokens[0].intValue == 0);
    CHECK(tokens[1].type == TokenType::DotDot);
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[2].intValue == 10);

    tokens = tokenize("0..=10");
    CHECK(tokens[0].type == TokenType::IntLiteral);
    CHECK(tokens[1].type == TokenType::DotDotEqual);
    CHECK(tokens[2].type == TokenType::IntLiteral);
}

TEST_CASE("Lexer dot vs dotdot", "[lexer]") {
    // a.b should be dot
    auto tokens = tokenize("a.b");
    CHECK(tokens[0].type == TokenType::Name);
    CHECK(tokens[1].type == TokenType::Dot);
    CHECK(tokens[2].type == TokenType::Name);

    // a..b should be dotdot
    tokens = tokenize("a..b");
    CHECK(tokens[0].type == TokenType::Name);
    CHECK(tokens[1].type == TokenType::DotDot);
    CHECK(tokens[2].type == TokenType::Name);
}

TEST_CASE("Lexer delimiters", "[lexer]") {
    auto tokens = tokenize("{ } ( ) [ ] ;");
    CHECK(tokens[0].type == TokenType::LeftBrace);
    CHECK(tokens[1].type == TokenType::RightBrace);
    CHECK(tokens[2].type == TokenType::LeftParen);
    CHECK(tokens[3].type == TokenType::RightParen);
    CHECK(tokens[4].type == TokenType::LeftBracket);
    CHECK(tokens[5].type == TokenType::RightBracket);
    CHECK(tokens[6].type == TokenType::Semicolon);
}

TEST_CASE("Lexer newline as statement separator", "[lexer]") {
    auto tokens = tokenize("set x 5\nset y 10");
    // Should have: Set x 5 Newline Set y 10 Eof
    CHECK(tokens[0].type == TokenType::Set);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[3].type == TokenType::Newline);
    CHECK(tokens[4].type == TokenType::Set);
    CHECK(tokens[5].type == TokenType::Name);
    CHECK(tokens[6].type == TokenType::IntLiteral);
}

TEST_CASE("Lexer newline suppressed inside parens", "[lexer]") {
    auto tokens = tokenize("(x +\ny)");
    // Newline should be suppressed
    bool hasNewline = false;
    for (auto& t : tokens) {
        if (t.type == TokenType::Newline) hasNewline = true;
    }
    CHECK_FALSE(hasNewline);
}

TEST_CASE("Lexer newline suppressed inside braces", "[lexer]") {
    auto tokens = tokenize("{add\n3\n4}");
    bool hasNewline = false;
    for (auto& t : tokens) {
        if (t.type == TokenType::Newline) hasNewline = true;
    }
    CHECK_FALSE(hasNewline);
}

TEST_CASE("Lexer newline suppressed inside brackets", "[lexer]") {
    auto tokens = tokenize("[1\n2\n3]");
    bool hasNewline = false;
    for (auto& t : tokens) {
        if (t.type == TokenType::Newline) hasNewline = true;
    }
    CHECK_FALSE(hasNewline);
}

TEST_CASE("Lexer multiple newlines collapse", "[lexer]") {
    auto tokens = tokenize("x\n\n\ny");
    int newlineCount = 0;
    for (auto& t : tokens) {
        if (t.type == TokenType::Newline) newlineCount++;
    }
    CHECK(newlineCount == 1);
}

TEST_CASE("Lexer comments", "[lexer]") {
    auto tokens = tokenize("set x 5 # this is a comment");
    CHECK(tokens[0].type == TokenType::Set);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[3].type == TokenType::Eof);
}

TEST_CASE("Lexer comment only", "[lexer]") {
    auto tokens = tokenize("# just a comment");
    CHECK(tokens[0].type == TokenType::Eof);
}

TEST_CASE("Lexer source locations", "[lexer]") {
    auto tokens = tokenize("set x 5");
    CHECK(tokens[0].location.line == 1);
    CHECK(tokens[0].location.column == 1);
    CHECK(tokens[1].location.line == 1);
    CHECK(tokens[1].location.column == 5);
    CHECK(tokens[2].location.line == 1);
    CHECK(tokens[2].location.column == 7);
}

TEST_CASE("Lexer hasLeadingSpace", "[lexer]") {
    auto tokens = tokenize("a[0]");
    // 'a' has leading space (start of line), '[' does NOT
    CHECK(tokens[0].text == "a");
    CHECK(tokens[1].type == TokenType::LeftBracket);
    CHECK_FALSE(tokens[1].hasLeadingSpace);

    tokens = tokenize("print [1 2]");
    // 'print' has leading space, '[' has leading space (after 'print ')
    CHECK(tokens[0].text == "print");
    CHECK(tokens[1].type == TokenType::LeftBracket);
    CHECK(tokens[1].hasLeadingSpace);
}

TEST_CASE("Lexer peek does not consume", "[lexer]") {
    Lexer lexer("set x");
    Token p = lexer.peek();
    CHECK(p.type == TokenType::Set);
    Token n = lexer.next();
    CHECK(n.type == TokenType::Set);
    CHECK(lexer.next().type == TokenType::Name);
}

TEST_CASE("Lexer unterminated string", "[lexer]") {
    CHECK_THROWS(tokenize("\"unterminated"));
}

TEST_CASE("Lexer complex expression", "[lexer]") {
    auto tokens = tokenize("if (x > 5) do\n    print x\nend");
    CHECK(tokens[0].type == TokenType::If);
    CHECK(tokens[1].type == TokenType::LeftParen);
    CHECK(tokens[2].type == TokenType::Name);
    CHECK(tokens[3].type == TokenType::Greater);
    CHECK(tokens[4].type == TokenType::IntLiteral);
    CHECK(tokens[5].type == TokenType::RightParen);
    CHECK(tokens[6].type == TokenType::Do);
    CHECK(tokens[7].type == TokenType::Newline);
    CHECK(tokens[8].type == TokenType::Name); // print
    CHECK(tokens[9].type == TokenType::Name); // x
    CHECK(tokens[10].type == TokenType::Newline);
    CHECK(tokens[11].type == TokenType::End);
}

TEST_CASE("Lexer fn definition", "[lexer]") {
    auto tokens = tokenize("fn double [x] (x * 2)");
    CHECK(tokens[0].type == TokenType::Fn);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "double");
    CHECK(tokens[2].type == TokenType::LeftBracket);
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[3].text == "x");
    CHECK(tokens[4].type == TokenType::RightBracket);
    CHECK(tokens[5].type == TokenType::LeftParen);
    CHECK(tokens[6].type == TokenType::Name);
    CHECK(tokens[7].type == TokenType::Star);
    CHECK(tokens[8].type == TokenType::IntLiteral);
    CHECK(tokens[9].type == TokenType::RightParen);
}

TEST_CASE("Lexer tilde token", "[lexer]") {
    auto tokens = tokenize("~myFunc");
    CHECK(tokens[0].type == TokenType::Tilde);
    CHECK(tokens[0].text == "~");
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[1].text == "myFunc");
}

TEST_CASE("Lexer tilde with space", "[lexer]") {
    auto tokens = tokenize("apply ~fn_ref 5");
    CHECK(tokens[0].type == TokenType::Name);
    CHECK(tokens[1].type == TokenType::Tilde);
    CHECK(tokens[2].type == TokenType::Name);
    CHECK(tokens[3].type == TokenType::IntLiteral);
}

TEST_CASE("Lexer KeyName token", "[lexer]") {
    auto tokens = tokenize("=foo =bar_baz");
    CHECK(tokens[0].type == TokenType::KeyName);
    CHECK(tokens[0].text == "foo");
    CHECK(tokens[1].type == TokenType::KeyName);
    CHECK(tokens[1].text == "bar_baz");
}

TEST_CASE("Lexer KeyName in context", "[lexer]") {
    auto tokens = tokenize("{=x 10 =y 20}");
    CHECK(tokens[0].type == TokenType::LeftBrace);
    CHECK(tokens[1].type == TokenType::KeyName);
    CHECK(tokens[1].text == "x");
    CHECK(tokens[2].type == TokenType::IntLiteral);
    CHECK(tokens[3].type == TokenType::KeyName);
    CHECK(tokens[3].text == "y");
    CHECK(tokens[4].type == TokenType::IntLiteral);
    CHECK(tokens[5].type == TokenType::RightBrace);
}

TEST_CASE("Lexer == vs =name", "[lexer]") {
    auto tokens = tokenize("(a == b) =opt 5");
    CHECK(tokens[0].type == TokenType::LeftParen);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::EqualEqual);
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[4].type == TokenType::RightParen);
    CHECK(tokens[5].type == TokenType::KeyName);
    CHECK(tokens[5].text == "opt");
    CHECK(tokens[6].type == TokenType::IntLiteral);
}

TEST_CASE("Lexer null coalesce ??", "[lexer]") {
    auto tokens = tokenize("(a ?? b)");
    CHECK(tokens[0].type == TokenType::LeftParen);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::NullCoalesce);
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[4].type == TokenType::RightParen);
}

TEST_CASE("Lexer falsy coalesce ?:", "[lexer]") {
    auto tokens = tokenize("(a ?: b)");
    CHECK(tokens[0].type == TokenType::LeftParen);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::FalsyCoalesce);
    CHECK(tokens[3].type == TokenType::Name);
    CHECK(tokens[4].type == TokenType::RightParen);
}

TEST_CASE("Lexer ?: followed by :symbol", "[lexer]") {
    auto tokens = tokenize("(a ?: :default)");
    CHECK(tokens[0].type == TokenType::LeftParen);
    CHECK(tokens[1].type == TokenType::Name);
    CHECK(tokens[2].type == TokenType::FalsyCoalesce);
    CHECK(tokens[3].type == TokenType::SymbolLiteral);
    CHECK(tokens[3].text == "default");
    CHECK(tokens[4].type == TokenType::RightParen);
}
