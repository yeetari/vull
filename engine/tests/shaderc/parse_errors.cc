#include <vull/shaderc/parser.hh>

#include <vull/container/vector.hh>
#include <vull/shaderc/error.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/token.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;
using vull::shaderc::operator""_tk;

// TODO: Use matchers.

static shaderc::Error try_parse(StringView source) {
    shaderc::Lexer lexer("", source);
    shaderc::Parser parser(lexer);
    auto result = parser.parse();
    EXPECT_TRUE(result.is_error());
    return result.error();
}

static bool has_message(const shaderc::Error &error, shaderc::ErrorMessage::Kind kind, StringView text,
                        shaderc::TokenKind token_kind) {
    for (const auto &message : error.messages()) {
        if (message.kind() == kind && message.text().view() == text && message.token().kind() == token_kind) {
            return true;
        }
    }
    return false;
}

static bool has_error(const shaderc::Error &error, StringView text, shaderc::TokenKind token_kind) {
    return has_message(error, shaderc::ErrorMessage::Kind::Error, text, token_kind);
}

static bool has_note(const shaderc::Error &error, StringView text, shaderc::TokenKind token_kind) {
    return has_message(error, shaderc::ErrorMessage::Kind::Note, text, token_kind) ||
           has_message(error, shaderc::ErrorMessage::Kind::NoteNoLine, text, token_kind);
}

TEST_CASE(ShaderParseErrors, BadTopLevel) {
    auto parse_error = try_parse("foo");
    EXPECT_TRUE(has_error(parse_error, "unexpected token 'foo'", shaderc::TokenKind::Identifier));
    EXPECT_TRUE(has_note(parse_error, "expected top level declaration or <eof>", shaderc::TokenKind::Identifier));
}

TEST_CASE(ShaderParseErrors, FunctionDeclBadName) {
    auto parse_error = try_parse("fn 123() {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for function name", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "got '123u' instead", shaderc::TokenKind::IntLit));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingName) {
    auto parse_error = try_parse("fn () {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for function name", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "got '(' instead", '('_tk));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingOpenParen) {
    auto parse_error = try_parse("fn foo) {}");
    EXPECT_TRUE(has_error(parse_error, "expected '(' to open the parameter list", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "got ')' instead", ')'_tk));
}
TEST_CASE(ShaderParseErrors, FunctionDeclBadParameter) {
    auto parse_error = try_parse("fn foo(bar) {}");
    EXPECT_TRUE(has_error(parse_error, "unexpected token 'bar'", shaderc::TokenKind::Identifier));
    EXPECT_TRUE(has_note(parse_error, "expected a parameter (let) or ')'", shaderc::TokenKind::Identifier));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingParameterName) {
    auto parse_error = try_parse("fn foo(let) {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for parameter name", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "got ')' instead", ')'_tk));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingReturnType) {
    auto parse_error = try_parse("fn foo(): {}");
    EXPECT_TRUE(has_error(parse_error, "expected type name but got '{'", '{'_tk));
}
TEST_CASE(ShaderParseErrors, FunctionDeclUnknownReturnType) {
    auto parse_error = try_parse("fn foo(): footype {}");
    EXPECT_TRUE(has_error(parse_error, "unknown type name 'footype'", shaderc::TokenKind::Identifier));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingBlock) {
    auto parse_error = try_parse("fn foo()");
    EXPECT_TRUE(has_error(parse_error, "expected '{' to open a block", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "got <eof> instead", shaderc::TokenKind::Eof));
}

TEST_CASE(ShaderParseErrors, PipelineDeclBadType) {
    auto parse_error = try_parse("pipeline 123 g_foo;");
    EXPECT_TRUE(has_error(parse_error, "expected type name but got '123u'", shaderc::TokenKind::IntLit));
}
TEST_CASE(ShaderParseErrors, PipelineDeclUnknownType) {
    auto parse_error = try_parse("pipeline footype g_foo;");
    EXPECT_TRUE(has_error(parse_error, "unknown type name 'footype'", shaderc::TokenKind::Identifier));
}
TEST_CASE(ShaderParseErrors, PipelineDeclBadName) {
    auto parse_error = try_parse("pipeline vec2 123;");
    EXPECT_TRUE(has_error(parse_error, "expected identifier but got '123u'", shaderc::TokenKind::IntLit));
}
TEST_CASE(ShaderParseErrors, PipelineDeclMissingSemicolon) {
    auto parse_error = try_parse("pipeline vec3 g_foo");
    EXPECT_TRUE(has_error(parse_error, "missing ';' after IO declaration", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "expected ';' before <eof>", shaderc::TokenKind::Eof));
}

TEST_CASE(ShaderParseErrors, UniformBlockDeclMissingSemicolon) {
    auto parse_error = try_parse(R"(
uniform {
    g_transform: mat4;
}
)");
    EXPECT_TRUE(has_error(parse_error, "missing ';' after IO declaration", shaderc::TokenKind::Cursor));
    EXPECT_TRUE(has_note(parse_error, "expected ';' before <eof>", shaderc::TokenKind::Eof));
}
