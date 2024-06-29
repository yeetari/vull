#include <vull/json/lexer.hh>

#include <vull/json/token.hh>
#include <vull/support/enum.hh>
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/message.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

namespace {

class OfTokenKind {
    json::TokenKind m_expected;

public:
    constexpr explicit OfTokenKind(json::TokenKind expected) : m_expected(expected) {}

    void describe(test::Message &message) const {
        message.append_text("a token of kind ");
        message.append_text(vull::enum_name<1>(m_expected));
    }

    // NOLINTNEXTLINE
    void describe_mismatch(test::Message &message, const json::Token &actual) const {
        message.append_text("was ");
        message.append_text(vull::enum_name<1>(actual.kind()));
    }

    bool matches(const json::Token &actual) const { return m_expected == actual.kind(); }
};

constexpr auto of_token_kind(json::TokenKind token_kind) {
    return OfTokenKind(token_kind);
}

} // namespace

TEST_CASE(JsonLexer, Empty) {
    json::Lexer lexer("");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Whitespace) {
    json::Lexer lexer("        ");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Null) {
    json::Lexer lexer("null");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Null)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, True) {
    json::Lexer lexer("true");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::True)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, False) {
    json::Lexer lexer("false");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::False)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Punctuation) {
    json::Lexer lexer("{}[]:,");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::ObjectBegin)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::ObjectEnd)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::ArrayBegin)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::ArrayEnd)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Colon)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Comma)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Integer) {
    json::Lexer lexer("1234");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::Integer)));
    EXPECT_THAT(token.integer(), is(equal_to(1234)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, NegativeInteger) {
    json::Lexer lexer("-1234");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::Integer)));
    EXPECT_THAT(token.integer(), is(equal_to(-1234)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Decimal) {
    json::Lexer lexer("1234.56");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(token.decimal(), is(close_to(1234.56)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, NegativeDecimal) {
    json::Lexer lexer("-1234.56");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(token.decimal(), is(close_to(-1234.56)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, Exponent) {
    json::Lexer lexer("1234e5 -1234.56E5");

    auto first = lexer.next();
    ASSERT_THAT(first, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(first.decimal(), is(close_to(1234e5)));

    auto second = lexer.next();
    ASSERT_THAT(second, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(second.decimal(), is(close_to(-1234.56e5)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, NegativeExponent) {
    json::Lexer lexer("1234e-5 -1234.56E-5");

    auto first = lexer.next();
    ASSERT_THAT(first, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(first.decimal(), is(close_to(1234e-5)));

    auto second = lexer.next();
    ASSERT_THAT(second, is(of_token_kind(json::TokenKind::Decimal)));
    EXPECT_THAT(second.decimal(), is(close_to(-1234.56e-5)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, EmptyString) {
    json::Lexer lexer("\"\"");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::String)));
    EXPECT_THAT(token.string(), is(empty()));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, String) {
    json::Lexer lexer("\"foo\"");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(json::TokenKind::String)));
    EXPECT_THAT(token.string(), is(equal_to(StringView("foo"))));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}

TEST_CASE(JsonLexer, MalformedString) {
    json::Lexer lexer("\"foo");
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Invalid)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(json::TokenKind::Eof)));
}
