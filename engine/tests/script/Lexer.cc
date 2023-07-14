#include <vull/script/Lexer.hh>

#include <vull/maths/Epsilon.hh>
#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Test.hh>

using namespace vull;
using vull::script::operator""_tk; // NOLINT

TEST_CASE(ScriptLexer, Empty) {
    script::Lexer lexer("", "");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Whitespace) {
    script::Lexer lexer("", "        \r\n\t");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, EmptyComment) {
    script::Lexer lexer("", "//");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Comment) {
    script::Lexer lexer("", "// Hello world");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Punctuation) {
    script::Lexer lexer("", "(); == != <= >=");
    EXPECT(lexer.next().kind() == '('_tk);
    EXPECT(lexer.next().kind() == ')'_tk);
    EXPECT(lexer.next().kind() == ';'_tk);
    EXPECT(lexer.next().kind() == script::TokenKind::EqualEqual);
    EXPECT(lexer.next().kind() == script::TokenKind::NotEqual);
    EXPECT(lexer.next().kind() == script::TokenKind::LessEqual);
    EXPECT(lexer.next().kind() == script::TokenKind::GreaterEqual);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Identifier) {
    script::Lexer lexer("", "abcd ABCD a123 A123 1abc");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "abcd");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "ABCD");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "a123");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "A123");
    EXPECT(lexer.next().kind() == script::TokenKind::Number);
    EXPECT(lexer.next().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Integer) {
    script::Lexer lexer("", "1234");
    auto token = lexer.next();
    EXPECT(token.kind() == script::TokenKind::Number);
    EXPECT(vull::fuzzy_equal(token.number(), 1234.0));
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Decimal) {
    script::Lexer lexer("", "1234.56");
    auto token = lexer.next();
    EXPECT(token.kind() == script::TokenKind::Number);
    EXPECT(vull::fuzzy_equal(token.number(), 1234.56));
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Exponent) {
    script::Lexer lexer("", "1234e5 1234.56E5");
    auto first = lexer.next();
    EXPECT(first.kind() == script::TokenKind::Number);
    EXPECT(vull::fuzzy_equal(first.number(), 1234e5));
    auto second = lexer.next();
    EXPECT(second.kind() == script::TokenKind::Number);
    EXPECT(vull::fuzzy_equal(second.number(), 1234.56e5));
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Negative) {
    script::Lexer lexer("", "-1234 -1234.56 -1234e-5 -1234.56E-5");
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == script::TokenKind::Number);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == script::TokenKind::Number);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == script::TokenKind::Number);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == script::TokenKind::Number);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Keywords) {
    script::Lexer lexer("", "elif else end function if let return");
    EXPECT(lexer.next().kind() == script::TokenKind::KW_elif);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_else);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_end);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_function);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_if);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_let);
    EXPECT(lexer.next().kind() == script::TokenKind::KW_return);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}
