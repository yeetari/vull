#include <vull/shaderc/lexer.hh>

#include <vull/maths/epsilon.hh>
#include <vull/shaderc/token.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/test.hh>

using namespace vull;
using vull::shaderc::operator""_tk; // NOLINT

TEST_CASE(ShaderLexer, Empty) {
    shaderc::Lexer lexer("", "");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Whitespace) {
    shaderc::Lexer lexer("", "        \r\n\t");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, EmptyComment) {
    shaderc::Lexer lexer("", "//");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Comment) {
    shaderc::Lexer lexer("", "// Hello world");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Punctuation) {
    shaderc::Lexer lexer("", "(); += -= *= /=");
    EXPECT(lexer.next().kind() == '('_tk);
    EXPECT(lexer.next().kind() == ')'_tk);
    EXPECT(lexer.next().kind() == ';'_tk);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::PlusEqual);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::MinusEqual);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::AsteriskEqual);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::SlashEqual);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Identifier) {
    shaderc::Lexer lexer("", "abcd ABCD a123 A123 1abc");
    EXPECT(lexer.peek().kind() == shaderc::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "abcd");
    EXPECT(lexer.peek().kind() == shaderc::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "ABCD");
    EXPECT(lexer.peek().kind() == shaderc::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "a123");
    EXPECT(lexer.peek().kind() == shaderc::TokenKind::Identifier);
    EXPECT(lexer.next().string() == "A123");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::IntLit);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Identifier);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Decimal) {
    shaderc::Lexer lexer("", "1234.56");
    auto token = lexer.next();
    EXPECT(token.kind() == shaderc::TokenKind::FloatLit);
    EXPECT(vull::fuzzy_equal(token.decimal(), 1234.56f));
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Integer) {
    shaderc::Lexer lexer("", "1234");
    auto token = lexer.next();
    EXPECT(token.kind() == shaderc::TokenKind::IntLit);
    EXPECT(token.integer() == 1234);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Exponent) {
    shaderc::Lexer lexer("", "1234e5 1234.56E5");
    auto first = lexer.next();
    EXPECT(first.kind() == shaderc::TokenKind::FloatLit);
    EXPECT(vull::fuzzy_equal(first.decimal(), 1234e5f));
    auto second = lexer.next();
    EXPECT(second.kind() == shaderc::TokenKind::FloatLit);
    EXPECT(vull::fuzzy_equal(second.decimal(), 1234.56e5f));
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Negative) {
    shaderc::Lexer lexer("", "-1234 -1234.56 -1234e-5 -1234.56E-5");
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::IntLit);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::FloatLit);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::FloatLit);
    EXPECT(lexer.next().kind() == '-'_tk);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::FloatLit);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}

TEST_CASE(ShaderLexer, Keywords) {
    shaderc::Lexer lexer("", "fn let pipeline uniform var");
    EXPECT(lexer.next().kind() == shaderc::TokenKind::KW_fn);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::KW_let);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::KW_pipeline);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::KW_uniform);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::KW_var);
    EXPECT(lexer.next().kind() == shaderc::TokenKind::Eof);
}
