#pragma once

#include "Ast.hh"
#include "Token.hh"

#include <vull/support/Optional.hh>

class Lexer;

class Parser {
    Lexer &m_lexer;
    ast::Root m_root;

    vull::Optional<Token> consume(TokenKind kind);
    Token expect(TokenKind kind);

    ast::Constant *parse_constant();
    ast::Node *parse_expr();
    ast::Node *parse_stmt();
    ast::Aggregate *parse_block();
    ast::Function *parse_function();
    ast::Node *parse_top_level();

public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) {}

    ast::Root parse();
};
