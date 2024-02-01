#pragma once

#include "ast.hh"
#include "token.hh"
#include "type.hh"

#include <vull/container/hash_map.hh>
#include <vull/support/optional.hh>
#include <vull/support/string_view.hh>

class Lexer;

class Parser {
    Lexer &m_lexer;
    ast::Root m_root;
    vull::HashMap<vull::StringView, Type> m_builtin_type_map;

    vull::Optional<Token> consume(TokenKind kind);
    Token expect(TokenKind kind);

    Type parse_type(const Token &token);
    ast::Node *parse_atom();
    ast::Node *parse_expr();
    ast::Node *parse_stmt();
    ast::Aggregate *parse_block();
    ast::Function *parse_function();
    ast::PipelineDecl *parse_pipeline_decl();
    ast::Aggregate *parse_uniform_block();
    ast::Node *parse_top_level();

public:
    explicit Parser(Lexer &lexer);

    ast::Root parse();
};
