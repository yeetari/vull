#pragma once

#include "Ast.hh"
#include "Token.hh"
#include "Type.hh"

#include <vull/container/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>

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
