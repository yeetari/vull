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

static bool has_message(const shaderc::Error &error, shaderc::ErrorMessage::Kind kind, StringView text) {
    for (const auto &message : error.messages()) {
        if (message.kind() == kind && message.text().view() == text) {
            return true;
        }
    }
    return false;
}

static bool has_error(const shaderc::Error &error, StringView text) {
    return has_message(error, shaderc::ErrorMessage::Kind::Error, text);
}

static bool has_note(const shaderc::Error &error, StringView text) {
    return has_message(error, shaderc::ErrorMessage::Kind::Note, text) ||
           has_message(error, shaderc::ErrorMessage::Kind::NoteNoLine, text);
}

TEST_CASE(ShaderParseErrors, BadTopLevel) {
    auto parse_error = try_parse("foo");
    EXPECT_TRUE(has_error(parse_error, "unexpected token 'foo'"));
    EXPECT_TRUE(has_note(parse_error, "expected top level declaration or <eof>"));
}

TEST_CASE(ShaderParseErrors, FunctionDeclBadName) {
    auto parse_error = try_parse("fn 123() {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for function name"));
    EXPECT_TRUE(has_note(parse_error, "got '123u' instead"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingName) {
    auto parse_error = try_parse("fn () {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for function name"));
    EXPECT_TRUE(has_note(parse_error, "got '(' instead"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingOpenParen) {
    auto parse_error = try_parse("fn foo) {}");
    EXPECT_TRUE(has_error(parse_error, "expected '(' to open the parameter list"));
    EXPECT_TRUE(has_note(parse_error, "got ')' instead"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclBadParameter) {
    auto parse_error = try_parse("fn foo(bar) {}");
    EXPECT_TRUE(has_error(parse_error, "unexpected token 'bar'"));
    EXPECT_TRUE(has_note(parse_error, "expected a parameter (let) or ')'"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingParameterName) {
    auto parse_error = try_parse("fn foo(let) {}");
    EXPECT_TRUE(has_error(parse_error, "expected identifier for parameter name"));
    EXPECT_TRUE(has_note(parse_error, "got ')' instead"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingReturnType) {
    auto parse_error = try_parse("fn foo(): {}");
    EXPECT_TRUE(has_error(parse_error, "expected type name but got '{'"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclUnknownReturnType) {
    auto parse_error = try_parse("fn foo(): footype {}");
    EXPECT_TRUE(has_error(parse_error, "unknown type name 'footype'"));
}
TEST_CASE(ShaderParseErrors, FunctionDeclMissingBlock) {
    auto parse_error = try_parse("fn foo()");
    EXPECT_TRUE(has_error(parse_error, "expected '{' to open a block"));
    EXPECT_TRUE(has_note(parse_error, "got <eof> instead"));
}

TEST_CASE(ShaderParseErrors, PipelineDeclBadType) {
    auto parse_error = try_parse("pipeline 123 g_foo;");
    EXPECT_TRUE(has_error(parse_error, "expected type name but got '123u'"));
}
TEST_CASE(ShaderParseErrors, PipelineDeclUnknownType) {
    auto parse_error = try_parse("pipeline footype g_foo;");
    EXPECT_TRUE(has_error(parse_error, "unknown type name 'footype'"));
}
TEST_CASE(ShaderParseErrors, PipelineDeclBadName) {
    auto parse_error = try_parse("pipeline vec2 123;");
    EXPECT_TRUE(has_error(parse_error, "expected identifier but got '123u'"));
}
TEST_CASE(ShaderParseErrors, PipelineDeclMissingSemicolon) {
    auto parse_error = try_parse("pipeline vec3 g_foo");
    EXPECT_TRUE(has_error(parse_error, "missing ';' after IO declaration"));
    EXPECT_TRUE(has_note(parse_error, "expected ';' before <eof>"));
}

TEST_CASE(ShaderParseErrors, UniformBlockDeclMissingSemicolon) {
    auto parse_error = try_parse(R"(
uniform {
    g_transform: mat4;
}
)");
    EXPECT_TRUE(has_error(parse_error, "missing ';' after IO declaration"));
    EXPECT_TRUE(has_note(parse_error, "expected ';' before <eof>"));
}
