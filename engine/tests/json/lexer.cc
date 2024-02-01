#include <vull/json/lexer.hh>

#include <vull/json/token.hh>
#include <vull/maths/epsilon.hh>
#include <vull/support/assert.hh>
#include <vull/support/string_view.hh>
#include <vull/support/test.hh>

using namespace vull;

TEST_CASE(JsonLexer, Empty) {
    json::Lexer lexer("");
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Whitespace) {
    json::Lexer lexer("        ");
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Null) {
    json::Lexer lexer("null");
    EXPECT(lexer.next().kind() == json::TokenKind::Null);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, True) {
    json::Lexer lexer("true");
    EXPECT(lexer.next().kind() == json::TokenKind::True);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, False) {
    json::Lexer lexer("false");
    EXPECT(lexer.next().kind() == json::TokenKind::False);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Punctuation) {
    json::Lexer lexer("{}[]:,");
    EXPECT(lexer.next().kind() == json::TokenKind::ObjectBegin);
    EXPECT(lexer.next().kind() == json::TokenKind::ObjectEnd);
    EXPECT(lexer.next().kind() == json::TokenKind::ArrayBegin);
    EXPECT(lexer.next().kind() == json::TokenKind::ArrayEnd);
    EXPECT(lexer.next().kind() == json::TokenKind::Colon);
    EXPECT(lexer.next().kind() == json::TokenKind::Comma);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Integer) {
    json::Lexer lexer("1234");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::Integer);
    EXPECT(token.integer() == 1234);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, NegativeInteger) {
    json::Lexer lexer("-1234");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::Integer);
    EXPECT(token.integer() == -1234);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Decimal) {
    json::Lexer lexer("1234.56");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(token.decimal(), 1234.56));
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, NegativeDecimal) {
    json::Lexer lexer("-1234.56");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(token.decimal(), -1234.56));
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, Exponent) {
    json::Lexer lexer("1234e5 -1234.56E5");
    auto first = lexer.next();
    EXPECT(first.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(first.decimal(), 1234e5));
    auto second = lexer.next();
    EXPECT(second.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(second.decimal(), -1234.56e5));
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, NegativeExponent) {
    json::Lexer lexer("1234e-5 -1234.56E-5");
    auto first = lexer.next();
    EXPECT(first.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(first.decimal(), 1234e-5));
    auto second = lexer.next();
    EXPECT(second.kind() == json::TokenKind::Decimal);
    EXPECT(vull::fuzzy_equal(second.decimal(), -1234.56e-5));
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, EmptyString) {
    json::Lexer lexer("\"\"");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::String);
    EXPECT(token.string().empty());
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, String) {
    json::Lexer lexer("\"foo\"");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::String);
    EXPECT(token.string() == "foo");
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}

TEST_CASE(JsonLexer, MalformedString) {
    json::Lexer lexer("\"foo");
    auto token = lexer.next();
    EXPECT(token.kind() == json::TokenKind::Invalid);
    EXPECT(lexer.next().kind() == json::TokenKind::Eof);
}
