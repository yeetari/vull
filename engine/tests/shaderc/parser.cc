#include <vull/shaderc/parser.hh>

#include <vull/shaderc/ast.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

static Result<shaderc::ast::Root, shaderc::ParseError> try_parse(StringView source) {
    shaderc::Lexer lexer("", source);
    shaderc::Parser parser(lexer);
    return parser.parse();
}

TEST_CASE(ShaderParser, Empty) {
    auto result = try_parse("");
    EXPECT_FALSE(result.is_error());
}
