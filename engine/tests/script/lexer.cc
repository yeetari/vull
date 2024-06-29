#include <vull/script/lexer.hh>

#include <vull/script/token.hh>
#include <vull/support/enum.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/message.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

namespace {

class OfTokenKind {
    script::TokenKind m_expected;

public:
    constexpr explicit OfTokenKind(script::TokenKind expected) : m_expected(expected) {}

    void describe(test::Message &message) const {
        message.append_text("a token of kind ");
        message.append_text(vull::enum_name<1>(m_expected));
    }

    // NOLINTNEXTLINE
    void describe_mismatch(test::Message &message, const script::Token &actual) const {
        message.append_text("was ");
        message.append_text(vull::enum_name<1>(actual.kind()));
    }

    bool matches(const script::Token &actual) const { return m_expected == actual.kind(); }
};

constexpr auto of_token_kind(script::TokenKind token_kind) {
    return OfTokenKind(token_kind);
}

} // namespace

TEST_CASE(ScriptLexer, Empty) {
    script::Lexer lexer("", "");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Whitespace) {
    script::Lexer lexer("", "        \r\n\t");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, EmptyComment) {
    script::Lexer lexer("", ";");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Comment) {
    script::Lexer lexer("", "; Hello world");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, DoubleComment) {
    script::Lexer lexer("", ";; Hello world ; Test");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Punctuation) {
    script::Lexer lexer("", "() [] '");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::ListBegin)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::ListEnd)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::ListBegin)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::ListEnd)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Quote)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Identifier) {
    script::Lexer lexer("", "abcd ABCD a123 A_12? !#$%&*+-./:<=>?@^_ 1abc");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("abcd"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("ABCD"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("a123"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("A_12?"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("!#$%&*+-./:<=>?@^_"))));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Quote) {
    script::Lexer lexer("", "'foo '5");
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Quote)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("foo"))));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Quote)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next().integer(), is(equal_to(5)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Integer) {
    script::Lexer lexer("", "1234 -1234 1 -1");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next().integer(), is(equal_to(1234)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next().integer(), is(equal_to(-1234)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next().integer(), is(equal_to(1)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Integer)));
    EXPECT_THAT(lexer.next().integer(), is(equal_to(-1)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Decimal) {
    script::Lexer lexer("", "1234.56 -1234.56");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(1234.56)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(-1234.56)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, Exponent) {
    script::Lexer lexer("", "1234e5 1234.56E5 1234e-5 1234.56E-5");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(1234e5)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(1234.56e5)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(1234e-5)));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::Decimal)));
    EXPECT_THAT(lexer.next().decimal(), is(close_to(1234.56e-5)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}

TEST_CASE(ScriptLexer, String) {
    script::Lexer lexer("", "\"hello\"");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(script::TokenKind::String)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("hello"))));
    EXPECT_THAT(lexer.next(), is(of_token_kind(script::TokenKind::Eof)));
}
