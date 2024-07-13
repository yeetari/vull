#include <vull/script/lexer.hh>

#include <vull/maths/epsilon.hh>
#include <vull/script/token.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/test.hh>

using namespace vull;

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
    script::Lexer lexer("", ";");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Comment) {
    script::Lexer lexer("", "; Hello world");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, DoubleComment) {
    script::Lexer lexer("", ";; Hello world ; Test");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Punctuation) {
    script::Lexer lexer("", "() [] '");
    EXPECT(lexer.next().kind() == script::TokenKind::ListBegin);
    EXPECT(lexer.next().kind() == script::TokenKind::ListEnd);
    EXPECT(lexer.next().kind() == script::TokenKind::ListBegin);
    EXPECT(lexer.next().kind() == script::TokenKind::ListEnd);
    EXPECT(lexer.next().kind() == script::TokenKind::Quote);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Identifier) {
    script::Lexer lexer("", "abcd ABCD a123 A_12? !#$%&*+-./:<=>?@^_ 1abc");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "abcd");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "ABCD");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "a123");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "A_12?");
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "!#$%&*+-./:<=>?@^_");
    EXPECT(lexer.next().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Quote) {
    script::Lexer lexer("", "'foo '5");
    EXPECT(lexer.next().kind() == script::TokenKind::Quote);
    EXPECT(lexer.peek().kind() == script::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "foo");
    EXPECT(lexer.next().kind() == script::TokenKind::Quote);
    EXPECT(lexer.peek().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().integer() == 5);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Integer) {
    script::Lexer lexer("", "1234 -1234 1 -1");
    EXPECT(lexer.peek().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().integer() == 1234);
    EXPECT(lexer.peek().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().integer() == -1234);
    EXPECT(lexer.peek().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().integer() == 1);
    EXPECT(lexer.peek().kind() == script::TokenKind::Integer);
    EXPECT(lexer.next().integer() == -1);
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Decimal) {
    script::Lexer lexer("", "1234.56 -1234.56");
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), 1234.56));
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), -1234.56));
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, Exponent) {
    script::Lexer lexer("", "1234e5 1234.56E5 1234e-5 1234.56E-5");
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), 1234e5));
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), 1234.56e5));
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), 1234e-5));
    EXPECT(lexer.peek().kind() == script::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(lexer.next().decimal(), 1234.56e-5));
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}

TEST_CASE(ScriptLexer, String) {
    script::Lexer lexer("", "\"hello\"");
    EXPECT(lexer.peek().kind() == script::TokenKind::String);
    EXPECT(lexer.next().string() == "hello");
    EXPECT(lexer.next().kind() == script::TokenKind::Eof);
}
