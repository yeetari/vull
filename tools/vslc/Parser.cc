#include "Parser.hh"

#include "Lexer.hh"
#include "Token.hh"
#include "Type.hh"

#include <vull/container/HashMap.hh>
#include <vull/container/Vector.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

namespace {

enum class Op {
    // Binary arithmetic operators.
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,

    // Unary operators.
    Negate,

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
    case Op::AddAssign:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::AddAssign, lhs, rhs);
    case Op::SubAssign:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::SubAssign, lhs, rhs);
    case Op::MulAssign:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::MulAssign, lhs, rhs);
    case Op::DivAssign:
        return root.allocate<ast::BinaryExpr>(ast::BinaryOp::DivAssign, lhs, rhs);
    default:
        vull::unreachable();
    }
}

unsigned precedence(Op op) {
    switch (op) {
    case Op::Assign:
    case Op::AddAssign:
    case Op::SubAssign:
    case Op::MulAssign:
    case Op::DivAssign:
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
        vull::unreachable();
    }
}

bool is_right_asc(Op op) {
    switch (op) {
    case Op::Assign:
    case Op::AddAssign:
    case Op::SubAssign:
    case Op::MulAssign:
    case Op::DivAssign:
        return true;
    default:
        return false;
    }
}

bool higher_precedence(Op a, Op b) {
    if (is_right_asc(b)) {
        return precedence(a) > precedence(b);
    }
    return precedence(a) >= precedence(b);
}

vull::Optional<Op> to_binary_op(TokenKind kind) {
    switch (kind) {
    case '+'_tk:
        return Op::Add;
    case '_'_tk:
        return Op::Sub;
    case '*'_tk:
        return Op::Mul;
    case '/'_tk:
        return Op::Div;
    case '%'_tk:
        return Op::Mod;
    case '='_tk:
        return Op::Assign;
    case TokenKind::PlusEqual:
        return Op::AddAssign;
    case TokenKind::MinusEqual:
        return Op::SubAssign;
    case TokenKind::AsteriskEqual:
        return Op::MulAssign;
    case TokenKind::SlashEqual:
        return Op::DivAssign;
    default:
        return {};
    }
}

} // namespace

Parser::Parser(Lexer &lexer) : m_lexer(lexer) {
    m_builtin_type_map.set("float", ScalarType::Float);
    m_builtin_type_map.set("vec2", {ScalarType::Float, 2});
    m_builtin_type_map.set("vec3", {ScalarType::Float, 3});
    m_builtin_type_map.set("vec4", {ScalarType::Float, 4});
    m_builtin_type_map.set("mat3", {ScalarType::Float, 3, 3});
    m_builtin_type_map.set("mat4", {ScalarType::Float, 4, 4});
}

vull::Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : vull::Optional<Token>();
}

Token Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    VULL_ENSURE(token.kind() == kind);
    return token;
}

Type Parser::parse_type(const Token &token) {
    VULL_ASSERT(token.kind() == TokenKind::Ident);
    return *m_builtin_type_map.get(token.string());
}

ast::Node *Parser::parse_atom() {
    if (auto literal = consume(TokenKind::FloatLit)) {
        return m_root.allocate<ast::Constant>(literal->decimal());
    }
    if (auto literal = consume(TokenKind::IntLit)) {
        return m_root.allocate<ast::Constant>(literal->integer());
    }
    if (auto ident = consume(TokenKind::Ident)) {
        if (!consume('('_tk)) {
            return m_root.allocate<ast::Symbol>(ident->string());
        }

        if (auto type = m_builtin_type_map.get(ident->string())) {
            auto *construct_expr = m_root.allocate<ast::Aggregate>(ast::AggregateKind::ConstructExpr);
            construct_expr->set_type(*type);
            while (!consume(')'_tk)) {
                construct_expr->append_node(parse_expr());
                consume(','_tk);
            }
            return construct_expr;
        }

        auto *call_expr = m_root.allocate<ast::CallExpr>(ident->string());
        while (!consume(')'_tk)) {
            call_expr->append_argument(parse_expr());
            consume(','_tk);
        }
        return call_expr;
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
        if (consume('-'_tk)) {
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
        if (consume('('_tk)) {
            operators.push(Op::OpenParen);
            paren_depth++;
            continue;
        }

        // Close parenthesis.
        if (paren_depth > 0 && consume(')'_tk)) {
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
    if (consume(TokenKind::KW_let) || consume(TokenKind::KW_var)) {
        auto name = expect(TokenKind::Ident);
        expect('='_tk);
        auto *value = parse_expr();
        expect(';'_tk);
        return m_root.allocate<ast::DeclStmt>(name.string(), value);
    }

    // Freestanding expression.
    auto *expr = parse_expr();
    if (consume(';'_tk)) {
        return expr;
    }
    // Otherwise, implicit return.
    return m_root.allocate<ast::ReturnStmt>(expr);
}

ast::Aggregate *Parser::parse_block() {
    expect('{'_tk);
    auto *block = m_root.allocate<ast::Aggregate>(ast::AggregateKind::Block);
    while (!consume('}'_tk)) {
        block->append_node(parse_stmt());
    }
    return block;
}

ast::Function *Parser::parse_function() {
    auto name = expect(TokenKind::Ident);
    expect('('_tk);

    vull::Vector<ast::Parameter> parameters;
    while (!consume(')'_tk)) {
        expect(TokenKind::KW_let);
        auto param_name = expect(TokenKind::Ident);
        expect(':'_tk);
        auto type = parse_type(expect(TokenKind::Ident));
        parameters.emplace(param_name.string(), type);
        consume(','_tk);
    }

    Type return_type(ScalarType::Void);
    if (consume(':'_tk)) {
        return_type = parse_type(expect(TokenKind::Ident));
    }
    auto *block = parse_block();
    return m_root.allocate<ast::Function>(name.string(), block, return_type, vull::move(parameters));
}

ast::PipelineDecl *Parser::parse_pipeline_decl() {
    auto type = parse_type(expect(TokenKind::Ident));
    auto name = expect(TokenKind::Ident);
    expect(';'_tk);
    return m_root.allocate<ast::PipelineDecl>(name.string(), type);
}

ast::Aggregate *Parser::parse_uniform_block() {
    expect('{'_tk);
    auto *block = m_root.allocate<ast::Aggregate>(ast::AggregateKind::UniformBlock);
    while (!consume('}'_tk)) {
        auto name = expect(TokenKind::Ident);
        expect(':'_tk);
        auto type = parse_type(expect(TokenKind::Ident));
        auto *symbol = m_root.allocate<ast::Symbol>(name.string());
        symbol->set_type(type);
        block->append_node(symbol);
        expect(','_tk);
    }
    expect(';'_tk);
    return block;
}

ast::Node *Parser::parse_top_level() {
    switch (m_lexer.next().kind()) {
    case TokenKind::KW_fn:
        return parse_function();
    case TokenKind::KW_pipeline:
        return parse_pipeline_decl();
    case TokenKind::KW_uniform:
        return parse_uniform_block();
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
