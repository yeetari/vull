#include <vull/shaderc/lexer.hh>

#include <vull/shaderc/token.hh>
#include <vull/support/enum.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/message.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;
using vull::shaderc::operator""_tk;

namespace {

class OfTokenKind {
    shaderc::TokenKind m_expected;

public:
    constexpr explicit OfTokenKind(shaderc::TokenKind expected) : m_expected(expected) {}

    void describe(test::Message &message) const {
        message.append_text("a token of kind ");
        message.append_text(vull::enum_name<1>(m_expected));
    }

    // NOLINTNEXTLINE
    void describe_mismatch(test::Message &message, const shaderc::Token &actual) const {
        message.append_text("was ");
        message.append_text(vull::enum_name<1>(actual.kind()));
    }

    bool matches(const shaderc::Token &actual) const { return m_expected == actual.kind(); }
};

constexpr auto of_token_kind(shaderc::TokenKind token_kind) {
    return OfTokenKind(token_kind);
}

} // namespace

TEST_CASE(ShaderLexer, Empty) {
    shaderc::Lexer lexer("", "");
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Whitespace) {
    shaderc::Lexer lexer("", "        \r\n\t");
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, EmptyComment) {
    shaderc::Lexer lexer("", "//");
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Comment) {
    shaderc::Lexer lexer("", "// Hello world");
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Punctuation) {
    shaderc::Lexer lexer("", "(); += -= *= /=");
    EXPECT_THAT(lexer.next(), is(of_token_kind('('_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(')'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(';'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::PlusEqual)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::MinusEqual)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::AsteriskEqual)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::SlashEqual)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Identifier) {
    shaderc::Lexer lexer("", "abcd ABCD a123 A123 1abc");
    ASSERT_THAT(lexer.peek(), is(of_token_kind(shaderc::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("abcd"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(shaderc::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("ABCD"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(shaderc::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("a123"))));
    ASSERT_THAT(lexer.peek(), is(of_token_kind(shaderc::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next().string(), is(equal_to(StringView("A123"))));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::IntLit)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Identifier)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Decimal) {
    shaderc::Lexer lexer("", "1234.56 1234.56f");
    auto first = lexer.next();
    ASSERT_THAT(first, is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(first.decimal(), is(close_to(1234.56f)));
    auto second = lexer.next();
    ASSERT_THAT(second, is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(second.decimal(), is(close_to(1234.56f)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Integer) {
    shaderc::Lexer lexer("", "1234");
    auto token = lexer.next();
    ASSERT_THAT(token, is(of_token_kind(shaderc::TokenKind::IntLit)));
    EXPECT_THAT(token.integer(), is(equal_to(1234)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Exponent) {
    shaderc::Lexer lexer("", "1234e5 1234.56E5");
    auto first = lexer.next();
    ASSERT_THAT(first, is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(first.decimal(), is(close_to(1234e5f)));
    auto second = lexer.next();
    ASSERT_THAT(second, is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(second.decimal(), is(close_to(1234.56e5f)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Negative) {
    shaderc::Lexer lexer("", "-1234 -1234.56 -1234e-5 -1234.56E-5");
    EXPECT_THAT(lexer.next(), is(of_token_kind('-'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::IntLit)));
    EXPECT_THAT(lexer.next(), is(of_token_kind('-'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(lexer.next(), is(of_token_kind('-'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(lexer.next(), is(of_token_kind('-'_tk)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::FloatLit)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}

TEST_CASE(ShaderLexer, Keywords) {
    shaderc::Lexer lexer("", "fn let pipeline uniform var");
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::KW_fn)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::KW_let)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::KW_pipeline)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::KW_uniform)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::KW_var)));
    EXPECT_THAT(lexer.next(), is(of_token_kind(shaderc::TokenKind::Eof)));
}
