#include "Parser.hh"

#include <vull/support/Assert.hh>

#include "Lexer.hh"
#include "Token.hh"

namespace {

enum class Op {
    // Binary arithmetic operators.
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    // Unary operators.
    Negate,

    Assign,
    OpenParen,
};

ast::Node *create_expr(ast::Root &root, Op op, vull::Vector<ast::Node *> &operands) {
    auto *rhs = operands.take_last();
    switch (op) {
    case Op::Negate:
        return root.allocate<ast::UnaryExpr>(ast::UnaryOp::Negate, rhs);
    }
    auto *lhs = operands.take_last();
    switch (op) {
    case Op::Add:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Add, lhs, rhs);
    case Op::Sub:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Sub, lhs, rhs);
    case Op::Mul:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Mul, lhs, rhs);
    case Op::Div:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Div, lhs, rhs);
    case Op::Mod:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Mod, lhs, rhs);
    case Op::Assign:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::Assign, lhs, rhs);
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

unsigned precedence(Op op) {
    switch (op) {
    case Op::Assign:
    case Op::OpenParen:
        return 0;
    case Op::Add:
    case Op::Sub:
        return 1;
    case Op::Mul:
    case Op::Div:
    case Op::Mod:
        return 2;
    case Op::Negate:
        return 3;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

bool is_right_asc(Op op) {
    return op == Op::Assign;
}

bool higher_precedence(Op a, Op b) {
    if (is_right_asc(b)) {
        return precedence(a) > precedence(b);
    }
    return precedence(a) >= precedence(b);
}

vull::Optional<Op> to_binary_op(TokenKind kind) {
    switch (kind) {
    case TokenKind::Plus:
        return Op::Add;
    case TokenKind::Minus:
        return Op::Sub;
    case TokenKind::Asterisk:
        return Op::Mul;
    case TokenKind::Slash:
        return Op::Div;
    case TokenKind::Percent:
        return Op::Mod;
    case TokenKind::Equals:
        return Op::Assign;
    default:
        return {};
    }
}

Type parse_type(const Token &ident) {
    // TODO(hash-map): Hash map for builtin types.
    if (ident.string() == "float") {
        return {ScalarType::Float};
    }
    if (ident.string() == "vec2") {
        return {ScalarType::Float, 2};
    }
    if (ident.string() == "vec3") {
        return {ScalarType::Float, 3};
    }
    if (ident.string() == "vec4") {
        return {ScalarType::Float, 4};
    }
    if (ident.string() == "mat3") {
        return {ScalarType::Float, 3, 3};
    }
    if (ident.string() == "mat4") {
        return {ScalarType::Float, 4, 4};
    }
    VULL_ENSURE_NOT_REACHED();
}

} // namespace

vull::Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : vull::Optional<Token>();
}

Token Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    VULL_ENSURE(token.kind() == kind);
    return token;
}

ast::Node *Parser::parse_atom() {
    if (auto literal = consume(TokenKind::FloatLit)) {
        return m_root.allocate<ast::Constant>(literal->decimal());
    }
    if (auto literal = consume(TokenKind::IntLit)) {
        return m_root.allocate<ast::Constant>(literal->integer());
    }
    if (auto ident = consume(TokenKind::Ident)) {
        if (!consume(TokenKind::LeftParen)) {
            return m_root.allocate<ast::Symbol>(ident->string());
        }
        auto *construct_expr = m_root.allocate<ast::Aggregate>(ast::AggregateKind::ConstructExpr);
        construct_expr->set_type(parse_type(*ident));
        while (!consume(TokenKind::RightParen)) {
            construct_expr->append_node(parse_expr());
            consume(TokenKind::Comma);
        }
        return construct_expr;
    }
    return nullptr;
}

ast::Node *Parser::parse_expr() {
    // TODO(small-vector)
    vull::Vector<ast::Node *> operands;
    vull::Vector<Op> operators;
    unsigned paren_depth = 0;
    while (true) {
        if (auto *atom = parse_atom()) {
            operands.push(atom);
            continue;
        }

        // Unary negate.
        if (consume(TokenKind::Minus)) {
            operators.push(Op::Negate);
            continue;
        }

        if (auto binary_op = to_binary_op(m_lexer.peek().kind())) {
            m_lexer.next();
            while (!operators.empty() && higher_precedence(operators.last(), *binary_op)) {
                auto op = operators.take_last();
                operands.push(create_expr(m_root, op, operands));
            }
            operators.push(*binary_op);
            continue;
        }

        // Open parenthesis.
        if (consume(TokenKind::LeftParen)) {
            operators.push(Op::OpenParen);
            paren_depth++;
            continue;
        }

        // Close parenthesis.
        if (paren_depth > 0 && consume(TokenKind::RightParen)) {
            while (!operators.empty() && operators.last() != Op::OpenParen) {
                auto op = operators.take_last();
                operands.push(create_expr(m_root, op, operands));
            }
            // Pop parenthesis.
            VULL_ENSURE(operators.take_last() == Op::OpenParen);
            paren_depth--;
            continue;
        }
        break;
    }

    while (!operators.empty()) {
        auto op = operators.take_last();
        operands.push(create_expr(m_root, op, operands));
    }

    VULL_ENSURE(operands.size() == 1);
    return operands.last();
}

ast::Node *Parser::parse_stmt() {
    if (consume(TokenKind::KeywordLet)) {
        auto name = expect(TokenKind::Ident);
        expect(TokenKind::Equals);
        auto *value = parse_expr();
        expect(TokenKind::Semi);
        return m_root.allocate<ast::DeclStmt>(name.string(), value);
    }

    // Freestanding expression.
    auto *expr = parse_expr();
    if (consume(TokenKind::Semi)) {
        return expr;
    }
    // Otherwise, implicit return.
    return m_root.allocate<ast::ReturnStmt>(expr);
}

ast::Aggregate *Parser::parse_block() {
    expect(TokenKind::LeftBrace);
    auto *block = m_root.allocate<ast::Aggregate>(ast::AggregateKind::Block);
    while (!consume(TokenKind::RightBrace)) {
        block->append_node(parse_stmt());
    }
    return block;
}

ast::Function *Parser::parse_function() {
    auto name = expect(TokenKind::Ident);
    expect(TokenKind::LeftParen);

    vull::Vector<ast::Parameter> parameters;
    while (!consume(TokenKind::RightParen)) {
        expect(TokenKind::KeywordLet);
        auto param_name = expect(TokenKind::Ident);
        expect(TokenKind::Colon);
        auto type = parse_type(expect(TokenKind::Ident));
        parameters.emplace(param_name.string(), type);
        consume(TokenKind::Comma);
    }

    expect(TokenKind::Colon);
    auto return_type = parse_type(expect(TokenKind::Ident));
    auto *block = parse_block();
    return m_root.allocate<ast::Function>(name.string(), block, return_type, vull::move(parameters));
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
