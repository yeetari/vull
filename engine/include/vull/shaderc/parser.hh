#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/error.hh>
#include <vull/shaderc/token.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::shaderc {

class Lexer;

template <typename T>
using ParseResult = Result<ast::NodeHandle<T>, Error>;

class Parser {
    using Operand = Variant<ast::NodeHandle<ast::Node>, StringView, Vector<ast::NodeHandle<ast::Node>>>;

public:
    enum class Operator : uint32_t;

private:
    Lexer &m_lexer;
    ast::Root m_root;
    HashMap<StringView, Type> m_builtin_type_map;

    Optional<Token> consume(TokenKind kind);
    Result<Token, Error> expect(TokenKind kind);
    Result<Token, Error> expect(TokenKind kind, StringView reason);
    Result<void, Error> expect_semi(StringView entity_name);

    Result<Type, Error> parse_type();

    ParseResult<ast::Node> build_call_or_construct(Vector<Operand> &operands);
    ast::NodeHandle<ast::Node> build_node(Operand operand);
    void build_expr(Operator op, Vector<Operand> &operands);
    Optional<Operand> parse_operand();
    ParseResult<ast::Node> parse_expr();

    ParseResult<ast::Node> parse_stmt();
    ParseResult<ast::Aggregate> parse_block();

    ParseResult<ast::FunctionDecl> parse_function_decl();
    ParseResult<ast::IoDecl> parse_io_decl(ast::IoKind io_kind);
    ParseResult<ast::Node> parse_top_level();

public:
    explicit Parser(Lexer &lexer);

    Result<ast::Root, Error> parse();
};

} // namespace vull::shaderc
