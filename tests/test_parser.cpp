#include <catch2/catch_test_macros.hpp>
#include "finescript/parser.h"
#include "finescript/ast.h"

using namespace finescript;

// Helper: parse a program (returns Block node)
static std::unique_ptr<AstNode> parse(std::string_view src) {
    return Parser::parse(src);
}

// Helper: parse a single expression
static std::unique_ptr<AstNode> parseExpr(std::string_view src) {
    return Parser::parseExpression(src);
}

// ---- Basic parsing ----

TEST_CASE("Parser empty program", "[parser]") {
    auto ast = parse("");
    REQUIRE(ast->kind == AstNodeKind::Block);
    CHECK(ast->children.empty());
}

TEST_CASE("Parser single integer", "[parser]") {
    auto ast = parse("42");
    REQUIRE(ast->kind == AstNodeKind::Block);
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::IntLit);
    CHECK(ast->children[0]->intValue == 42);
}

TEST_CASE("Parser single float", "[parser]") {
    auto ast = parse("3.14");
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::FloatLit);
    CHECK(ast->children[0]->floatValue == 3.14);
}

TEST_CASE("Parser single string", "[parser]") {
    auto ast = parse("\"hello\"");
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::StringLit);
    CHECK(ast->children[0]->stringValue == "hello");
}

TEST_CASE("Parser boolean and nil", "[parser]") {
    auto ast = parse("true\nfalse\nnil");
    REQUIRE(ast->children.size() == 3);
    CHECK(ast->children[0]->kind == AstNodeKind::BoolLit);
    CHECK(ast->children[0]->boolValue == true);
    CHECK(ast->children[1]->kind == AstNodeKind::BoolLit);
    CHECK(ast->children[1]->boolValue == false);
    CHECK(ast->children[2]->kind == AstNodeKind::NilLit);
}

TEST_CASE("Parser symbol literal", "[parser]") {
    auto ast = parse(":stone");
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::SymbolLit);
    CHECK(ast->children[0]->stringValue == "stone");
}

TEST_CASE("Parser name auto-call wrapping", "[parser]") {
    // Bare name in statement position is wrapped in Call (auto-call semantics)
    auto ast = parse("x");
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::Call);
    REQUIRE(ast->children[0]->children.size() == 1);
    CHECK(ast->children[0]->children[0]->kind == AstNodeKind::Name);
    CHECK(ast->children[0]->children[0]->stringValue == "x");
}

// ---- Prefix calls ----

TEST_CASE("Parser simple prefix call", "[parser]") {
    auto ast = parse("print 42");
    REQUIRE(ast->children.size() == 1);
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 2);
    CHECK(call->children[0]->kind == AstNodeKind::Name);
    CHECK(call->children[0]->stringValue == "print");
    CHECK(call->children[1]->kind == AstNodeKind::IntLit);
    CHECK(call->children[1]->intValue == 42);
}

TEST_CASE("Parser multi-arg prefix call", "[parser]") {
    auto ast = parse("add 3 4");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 3);
    CHECK(call->children[0]->stringValue == "add");
    CHECK(call->children[1]->intValue == 3);
    CHECK(call->children[2]->intValue == 4);
}

TEST_CASE("Parser nested prefix call", "[parser]") {
    auto ast = parse("print {add 3 4}");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 2);
    CHECK(call->children[0]->stringValue == "print");
    auto& inner = call->children[1];
    CHECK(inner->kind == AstNodeKind::Call);
    REQUIRE(inner->children.size() == 3);
    CHECK(inner->children[0]->stringValue == "add");
}

TEST_CASE("Parser multiple statements", "[parser]") {
    auto ast = parse("set x 5\nprint x");
    REQUIRE(ast->children.size() == 2);
    CHECK(ast->children[0]->kind == AstNodeKind::Set);
    CHECK(ast->children[1]->kind == AstNodeKind::Call);
}

TEST_CASE("Parser semicolon separator", "[parser]") {
    auto ast = parse("{set x 5; print x}");
    // Single brace expression containing two statements
    REQUIRE(ast->children.size() == 1);
    auto& block = ast->children[0];
    CHECK(block->kind == AstNodeKind::Block);
    REQUIRE(block->children.size() == 2);
}

// ---- Array literals ----

TEST_CASE("Parser empty array", "[parser]") {
    auto ast = parseExpr("[]");
    CHECK(ast->kind == AstNodeKind::ArrayLit);
    CHECK(ast->children.empty());
}

TEST_CASE("Parser array literal", "[parser]") {
    auto ast = parseExpr("[1 2 3]");
    CHECK(ast->kind == AstNodeKind::ArrayLit);
    REQUIRE(ast->children.size() == 3);
    CHECK(ast->children[0]->intValue == 1);
    CHECK(ast->children[1]->intValue == 2);
    CHECK(ast->children[2]->intValue == 3);
}

TEST_CASE("Parser array with expressions", "[parser]") {
    auto ast = parseExpr("[x (x + 1) {add 3 4}]");
    CHECK(ast->kind == AstNodeKind::ArrayLit);
    REQUIRE(ast->children.size() == 3);
    CHECK(ast->children[0]->kind == AstNodeKind::Name);
    CHECK(ast->children[1]->kind == AstNodeKind::Infix);
    CHECK(ast->children[2]->kind == AstNodeKind::Call);
}

// ---- Infix expressions ----

TEST_CASE("Parser simple infix", "[parser]") {
    auto ast = parseExpr("(x + 5)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "+");
    CHECK(ast->children[0]->kind == AstNodeKind::Name);
    CHECK(ast->children[1]->kind == AstNodeKind::IntLit);
}

TEST_CASE("Parser infix precedence mul over add", "[parser]") {
    auto ast = parseExpr("(a + b * c)");
    // Should be: a + (b * c)
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "+");
    CHECK(ast->children[0]->kind == AstNodeKind::Name);
    auto& rhs = ast->children[1];
    CHECK(rhs->kind == AstNodeKind::Infix);
    CHECK(rhs->op == "*");
}

TEST_CASE("Parser infix left associative", "[parser]") {
    auto ast = parseExpr("(a - b - c)");
    // Should be: (a - b) - c
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "-");
    auto& lhs = ast->children[0];
    CHECK(lhs->kind == AstNodeKind::Infix);
    CHECK(lhs->op == "-");
    CHECK(ast->children[1]->stringValue == "c");
}

TEST_CASE("Parser infix comparison", "[parser]") {
    auto ast = parseExpr("(x > 5)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == ">");
}

TEST_CASE("Parser infix logical", "[parser]") {
    auto ast = parseExpr("(x > 0 and x < 10)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "and");
    CHECK(ast->children[0]->op == ">");
    CHECK(ast->children[1]->op == "<");
}

TEST_CASE("Parser infix equality", "[parser]") {
    auto ast = parseExpr("(a == b)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "==");
}

TEST_CASE("Parser infix range inside parens", "[parser]") {
    auto ast = parseExpr("(0..10)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "..");
    CHECK(ast->children[0]->intValue == 0);
    CHECK(ast->children[1]->intValue == 10);
}

TEST_CASE("Parser unary not in parens", "[parser]") {
    auto ast = parseExpr("(not true)");
    CHECK(ast->kind == AstNodeKind::UnaryNot);
    CHECK(ast->children[0]->kind == AstNodeKind::BoolLit);
}

TEST_CASE("Parser unary negate in parens", "[parser]") {
    auto ast = parseExpr("(-5)");
    CHECK(ast->kind == AstNodeKind::UnaryNegate);
    CHECK(ast->children[0]->intValue == 5);
}

TEST_CASE("Parser nested parens", "[parser]") {
    auto ast = parseExpr("((x + 1) * 2)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "*");
    CHECK(ast->children[0]->kind == AstNodeKind::Infix);
    CHECK(ast->children[0]->op == "+");
    CHECK(ast->children[1]->intValue == 2);
}

TEST_CASE("Parser brace expr inside parens", "[parser]") {
    auto ast = parseExpr("({add 3 4} + 5)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "+");
    CHECK(ast->children[0]->kind == AstNodeKind::Call);
    CHECK(ast->children[1]->intValue == 5);
}

// ---- Dot notation ----

TEST_CASE("Parser dot access auto-call wrapping", "[parser]") {
    // DottedName in statement position is wrapped in Call (auto-call semantics)
    auto ast = parseExpr("player.health");
    CHECK(ast->kind == AstNodeKind::Call);
    REQUIRE(ast->children.size() == 1);
    auto& dotted = ast->children[0];
    CHECK(dotted->kind == AstNodeKind::DottedName);
    CHECK(dotted->children[0]->stringValue == "player");
    REQUIRE(dotted->nameParts.size() == 1);
    CHECK(dotted->nameParts[0] == "health");
}

TEST_CASE("Parser chained dot access auto-call wrapping", "[parser]") {
    auto ast = parseExpr("player.inv.size");
    CHECK(ast->kind == AstNodeKind::Call);
    REQUIRE(ast->children.size() == 1);
    auto& dotted = ast->children[0];
    CHECK(dotted->kind == AstNodeKind::DottedName);
    CHECK(dotted->children[0]->stringValue == "player");
    REQUIRE(dotted->nameParts.size() == 2);
    CHECK(dotted->nameParts[0] == "inv");
    CHECK(dotted->nameParts[1] == "size");
}

TEST_CASE("Parser dot method call", "[parser]") {
    auto ast = parse("player.damage 10");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 2);
    CHECK(call->children[0]->kind == AstNodeKind::DottedName);
    CHECK(call->children[1]->intValue == 10);
}

// ---- Array indexing ----

TEST_CASE("Parser array index no space", "[parser]") {
    auto ast = parseExpr("a[0]");
    CHECK(ast->kind == AstNodeKind::Index);
    CHECK(ast->children[0]->stringValue == "a");
    CHECK(ast->children[1]->intValue == 0);
}

TEST_CASE("Parser array literal with space", "[parser]") {
    // print [1 2] — the [1 2] is an array argument, not indexing
    auto ast = parse("print [1 2]");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    CHECK(call->children[1]->kind == AstNodeKind::ArrayLit);
}

TEST_CASE("Parser chained index", "[parser]") {
    auto ast = parseExpr("a[0][1]");
    CHECK(ast->kind == AstNodeKind::Index);
    CHECK(ast->children[0]->kind == AstNodeKind::Index);
}

// ---- Set ----

TEST_CASE("Parser set simple", "[parser]") {
    auto ast = parse("set x 5");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Set);
    REQUIRE(node->nameParts.size() == 1);
    CHECK(node->nameParts[0] == "x");
    CHECK(node->children[0]->intValue == 5);
}

TEST_CASE("Parser set dotted", "[parser]") {
    auto ast = parse("set player.health 100");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Set);
    REQUIRE(node->nameParts.size() == 2);
    CHECK(node->nameParts[0] == "player");
    CHECK(node->nameParts[1] == "health");
    CHECK(node->children[0]->intValue == 100);
}

TEST_CASE("Parser set with expression", "[parser]") {
    auto ast = parse("set x (x + 1)");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Set);
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
}

// ---- Fn ----

TEST_CASE("Parser named fn one-line", "[parser]") {
    auto ast = parse("fn double [x] (x * 2)");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Fn);
    CHECK(node->stringValue == "double");
    REQUIRE(node->nameParts.size() == 1);
    CHECK(node->nameParts[0] == "x");
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
    CHECK(node->children[0]->op == "*");
}

TEST_CASE("Parser named fn multi-line", "[parser]") {
    auto ast = parse("fn greet [name] do\n  print name\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Fn);
    CHECK(node->stringValue == "greet");
    CHECK(node->children[0]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser anonymous fn", "[parser]") {
    auto ast = parseExpr("fn [x] (x * 2)");
    CHECK(ast->kind == AstNodeKind::Fn);
    CHECK(ast->stringValue.empty());
    REQUIRE(ast->nameParts.size() == 1);
    CHECK(ast->nameParts[0] == "x");
}

TEST_CASE("Parser fn no params", "[parser]") {
    auto ast = parse("fn greet [] {print \"hello\"}");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Fn);
    CHECK(node->nameParts.empty());
}

TEST_CASE("Parser fn as argument", "[parser]") {
    auto ast = parse("apply fn [x] (x + 1) 5");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 3);
    CHECK(call->children[0]->stringValue == "apply");
    CHECK(call->children[1]->kind == AstNodeKind::Fn);
    CHECK(call->children[2]->intValue == 5);
}

// ---- If ----

TEST_CASE("Parser if do end", "[parser]") {
    auto ast = parse("if (x > 5) do\n  print x\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::If);
    CHECK_FALSE(node->hasElse);
    REQUIRE(node->children.size() == 2);
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
    CHECK(node->children[1]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser if elif else", "[parser]") {
    auto ast = parse(
        "if (x > 10) do\n"
        "  print :big\n"
        "elif (x > 0) do\n"
        "  print :small\n"
        "else do\n"
        "  print :non_positive\n"
        "end");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::If);
    CHECK(node->hasElse);
    REQUIRE(node->children.size() == 5);
}

TEST_CASE("Parser if one-line", "[parser]") {
    auto ast = parse("if (x > 5) {print :big}");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::If);
    CHECK_FALSE(node->hasElse);
    REQUIRE(node->children.size() == 2);
}

TEST_CASE("Parser if one-line with else", "[parser]") {
    auto ast = parse("if (x > 5) {print :big} {print :small}");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::If);
    CHECK(node->hasElse);
    REQUIRE(node->children.size() == 3);
}

// ---- For ----

TEST_CASE("Parser for loop", "[parser]") {
    auto ast = parse("for i in 0..10 do\n  print i\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::For);
    REQUIRE(node->nameParts.size() == 1);
    CHECK(node->nameParts[0] == "i");
    REQUIRE(node->children.size() == 2);
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
    CHECK(node->children[0]->op == "..");
    CHECK(node->children[1]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser for inclusive range", "[parser]") {
    auto ast = parse("for i in 0..=10 do\n  print i\nend");
    auto& node = ast->children[0];
    CHECK(node->children[0]->op == "..=");
}

TEST_CASE("Parser for over expression", "[parser]") {
    auto ast = parse("for item in {get_items player} do\n  print item\nend");
    auto& node = ast->children[0];
    CHECK(node->children[0]->kind == AstNodeKind::Call);
}

// ---- While ----

TEST_CASE("Parser while loop", "[parser]") {
    auto ast = parse("while (x > 0) do\n  set x (x - 1)\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::While);
    REQUIRE(node->children.size() == 2);
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
    CHECK(node->children[1]->kind == AstNodeKind::Block);
}

// ---- Match (new syntax: PATTERN BODY_STATEMENT per arm) ----

TEST_CASE("Parser match simple", "[parser]") {
    auto ast = parse(
        "match x\n"
        "  :a print \"alpha\"\n"
        "  :b print \"beta\"\n"
        "  _ print \"other\"\n"
        "end");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Match);
    // children[0] = scrutinee, then pairs of (pattern, body)
    // scrutinee + 3 arms * 2 = 7
    REQUIRE(node->children.size() == 7);
    CHECK(node->children[0]->kind == AstNodeKind::Name); // scrutinee
    CHECK(node->children[1]->kind == AstNodeKind::SymbolLit); // :a
    CHECK(node->children[2]->kind == AstNodeKind::Call); // print "alpha"
    CHECK(node->children[3]->kind == AstNodeKind::SymbolLit); // :b
}

TEST_CASE("Parser match with brace bodies", "[parser]") {
    auto ast = parse(
        "match x\n"
        "  1 {set y 5; y}\n"
        "  2 {set y 10; y}\n"
        "end");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Match);
    // scrutinee + 2 arms * 2 = 5
    CHECK(node->children.size() == 5);
    CHECK(node->children[2]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser match with do-end body", "[parser]") {
    auto ast = parse(
        "match x\n"
        "  1 do\n"
        "    set y 5\n"
        "    print y\n"
        "  end\n"
        "  _ nil\n"
        "end");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Match);
    // scrutinee + 2 arms * 2 = 5
    CHECK(node->children.size() == 5);
    CHECK(node->children[2]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser match wildcard", "[parser]") {
    auto ast = parse(
        "match x\n"
        "  _ nil\n"
        "end");
    auto& node = ast->children[0];
    // scrutinee + 1 arm * 2 = 3
    REQUIRE(node->children.size() == 3);
    CHECK(node->children[1]->kind == AstNodeKind::Name);
    CHECK(node->children[1]->stringValue == "_");
}

// ---- On ----

TEST_CASE("Parser on event", "[parser]") {
    auto ast = parse("on :interact do\n  print \"interacted\"\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::On);
    CHECK(node->stringValue == "interact");
    CHECK(node->children[0]->kind == AstNodeKind::Block);
}

TEST_CASE("Parser on one-line", "[parser]") {
    auto ast = parse("on :click {print \"clicked\"}");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::On);
    CHECK(node->stringValue == "click");
}

// ---- Return ----

TEST_CASE("Parser return with value", "[parser]") {
    auto ast = parse("return 5");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Return);
    REQUIRE(node->children.size() == 1);
    CHECK(node->children[0]->intValue == 5);
}

TEST_CASE("Parser return no value", "[parser]") {
    auto ast = parse("return");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Return);
    CHECK(node->children.empty());
}

// ---- Source ----

TEST_CASE("Parser source", "[parser]") {
    auto ast = parse("source \"utils.script\"");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Source);
    REQUIRE(node->children.size() == 1);
    CHECK(node->children[0]->kind == AstNodeKind::StringLit);
}

// ---- Do...end block ----

TEST_CASE("Parser do block", "[parser]") {
    auto ast = parse("do\n  set x 5\n  print x\nend");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Block);
    REQUIRE(node->children.size() == 2);
}

// ---- String interpolation ----

TEST_CASE("Parser string interpolation simple", "[parser]") {
    auto ast = parseExpr("\"Hello {name}\"");
    CHECK(ast->kind == AstNodeKind::StringInterp);
    REQUIRE(ast->children.size() == 2);
    CHECK(ast->children[0]->kind == AstNodeKind::StringLit);
    CHECK(ast->children[0]->stringValue == "Hello ");
    // name inside interpolation is wrapped in Call (auto-call semantics)
    CHECK(ast->children[1]->kind == AstNodeKind::Call);
    CHECK(ast->children[1]->children[0]->kind == AstNodeKind::Name);
    CHECK(ast->children[1]->children[0]->stringValue == "name");
}

TEST_CASE("Parser string interpolation with suffix", "[parser]") {
    auto ast = parseExpr("\"Hello {name}!\"");
    CHECK(ast->kind == AstNodeKind::StringInterp);
    REQUIRE(ast->children.size() == 3);
    CHECK(ast->children[0]->stringValue == "Hello ");
    CHECK(ast->children[1]->kind == AstNodeKind::Call);
    CHECK(ast->children[1]->children[0]->stringValue == "name");
    CHECK(ast->children[2]->stringValue == "!");
}

TEST_CASE("Parser string interpolation multiple", "[parser]") {
    auto ast = parseExpr("\"x={x}, y={y}\"");
    CHECK(ast->kind == AstNodeKind::StringInterp);
    // "x=", Call(name(x)), ", y=", Call(name(y))
    REQUIRE(ast->children.size() == 4);
    CHECK(ast->children[0]->stringValue == "x=");
    CHECK(ast->children[1]->kind == AstNodeKind::Call);
    CHECK(ast->children[1]->children[0]->stringValue == "x");
    CHECK(ast->children[2]->stringValue == ", y=");
    CHECK(ast->children[3]->kind == AstNodeKind::Call);
    CHECK(ast->children[3]->children[0]->stringValue == "y");
}

TEST_CASE("Parser string interpolation with call", "[parser]") {
    auto ast = parseExpr("\"Result: {add 3 4}\"");
    CHECK(ast->kind == AstNodeKind::StringInterp);
    REQUIRE(ast->children.size() >= 2);
    CHECK(ast->children[0]->stringValue == "Result: ");
    CHECK(ast->children[1]->kind == AstNodeKind::Call);
}

// ---- Unary operators ----

TEST_CASE("Parser unary negate", "[parser]") {
    auto ast = parse("print -5");
    auto& call = ast->children[0];
    REQUIRE(call->children.size() == 2);
    CHECK(call->children[1]->kind == AstNodeKind::UnaryNegate);
    CHECK(call->children[1]->children[0]->intValue == 5);
}

TEST_CASE("Parser unary not", "[parser]") {
    auto ast = parse("print not true");
    auto& call = ast->children[0];
    REQUIRE(call->children.size() == 2);
    CHECK(call->children[1]->kind == AstNodeKind::UnaryNot);
}

// ---- Brace expressions ----

TEST_CASE("Parser brace single expr", "[parser]") {
    auto ast = parseExpr("{add 3 4}");
    CHECK(ast->kind == AstNodeKind::Call);
}

TEST_CASE("Parser brace multi statement", "[parser]") {
    auto ast = parseExpr("{set x 5; set y 10; add x y}");
    CHECK(ast->kind == AstNodeKind::Block);
    REQUIRE(ast->children.size() == 3);
}

TEST_CASE("Parser multiple brace exprs as statements", "[parser]") {
    auto ast = parse("{set x 5} {set y 10}");
    REQUIRE(ast->children.size() == 2);
    CHECK(ast->children[0]->kind == AstNodeKind::Set);
    CHECK(ast->children[1]->kind == AstNodeKind::Set);
}

// ---- Complex / integration tests ----

TEST_CASE("Parser fn with closure", "[parser]") {
    auto ast = parse(
        "fn make_counter [start] do\n"
        "  set n start\n"
        "  fn [] do\n"
        "    set n (n + 1)\n"
        "    return n\n"
        "  end\n"
        "end");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Fn);
    CHECK(node->stringValue == "make_counter");
}

TEST_CASE("Parser dot in set", "[parser]") {
    auto ast = parse("set self.health (self.health - amount)");
    auto& node = ast->children[0];
    CHECK(node->kind == AstNodeKind::Set);
    REQUIRE(node->nameParts.size() == 2);
    CHECK(node->nameParts[0] == "self");
    CHECK(node->nameParts[1] == "health");
    CHECK(node->children[0]->kind == AstNodeKind::Infix);
}

TEST_CASE("Parser method call in expression", "[parser]") {
    auto ast = parseExpr("(player.health * 2)");
    CHECK(ast->kind == AstNodeKind::Infix);
    CHECK(ast->op == "*");
    CHECK(ast->children[0]->kind == AstNodeKind::DottedName);
}

TEST_CASE("Parser parseExpression empty", "[parser]") {
    auto ast = parseExpr("");
    CHECK(ast->kind == AstNodeKind::NilLit);
}

// ---- Tilde reference ----

TEST_CASE("Parser tilde reference", "[parser]") {
    auto ast = parseExpr("~myFunc");
    // Tilde wraps in Ref, which is NOT auto-call wrapped
    CHECK(ast->kind == AstNodeKind::Ref);
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::Name);
    CHECK(ast->children[0]->stringValue == "myFunc");
}

TEST_CASE("Parser tilde dotted reference", "[parser]") {
    auto ast = parseExpr("~obj.method");
    CHECK(ast->kind == AstNodeKind::Ref);
    REQUIRE(ast->children.size() == 1);
    CHECK(ast->children[0]->kind == AstNodeKind::DottedName);
}

TEST_CASE("Parser tilde as argument", "[parser]") {
    auto ast = parse("apply ~myFunc 5");
    auto& call = ast->children[0];
    CHECK(call->kind == AstNodeKind::Call);
    REQUIRE(call->children.size() == 3);
    CHECK(call->children[1]->kind == AstNodeKind::Ref);
    CHECK(call->children[2]->intValue == 5);
}

// ---- Error cases ----

TEST_CASE("Parser error on unexpected token", "[parser]") {
    CHECK_THROWS(parse("+"));
}

TEST_CASE("Parser error unclosed paren", "[parser]") {
    CHECK_THROWS(parse("(x + 5"));
}

TEST_CASE("Parser error unclosed brace", "[parser]") {
    CHECK_THROWS(parse("{add 3 4"));
}

TEST_CASE("Parser source locations preserved", "[parser]") {
    auto ast = parse("set x 5");
    auto& node = ast->children[0];
    CHECK(node->loc.line == 1);
    CHECK(node->loc.column == 1);
}
