#include "Parser.hh"

#include <vull/support/Assert.hh>

#include "Lexer.hh"
#include "Token.hh"

vull::Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : vull::Optional<Token>();
}

Token Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    VULL_ENSURE(token.kind() == kind);
    return token;
}

ast::Node *Parser::parse_expr() {
    auto *constant_list = m_root.allocate<ast::ConstantList>(ast::Type(ast::ScalarType::Float, 1));
    if (auto ident = consume(TokenKind::Ident)) {
        VULL_ENSURE(ident->string() == "vec4");
        constant_list->set_type(ast::Type(ast::ScalarType::Float, 4));
        expect(TokenKind::LeftParen);
        auto *sub_list = static_cast<ast::ConstantList *>(parse_expr());
        for (auto constant : *sub_list) {
            constant_list->push(constant);
        }
        sub_list->ast::ConstantList::~ConstantList();
        expect(TokenKind::RightParen);
        return constant_list;
    }
    auto literal = expect(TokenKind::FloatLit);
    constant_list->push(ast::Constant{
        .literal{.decimal = literal.decimal()},
        .scalar_type = ast::ScalarType::Float,
    });
    return constant_list;
}

ast::Node *Parser::parse_stmt() {
    auto *expr = parse_expr();
    // Implicit return.
    return m_root.allocate<ast::ReturnStmt>(expr);
}

ast::Block *Parser::parse_block() {
    expect(TokenKind::LeftBrace);
    auto *block = m_root.allocate<ast::Block>();
    while (!consume(TokenKind::RightBrace)) {
        block->append_node(parse_stmt());
    }
    return block;
}

ast::Function *Parser::parse_function() {
    auto name = expect(TokenKind::Ident);
    expect(TokenKind::LeftParen);
    expect(TokenKind::RightParen);
    auto *block = parse_block();
    return m_root.allocate<ast::Function>(name.string(), block);
}

ast::Node *Parser::parse_top_level() {
    switch (m_lexer.next().kind()) {
    case TokenKind::KeywordFn:
        return parse_function();
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

ast::Root Parser::parse() {
    while (!consume(TokenKind::Eof)) {
        m_root.append_top_level(parse_top_level());
    }
    return vull::move(m_root);
}
