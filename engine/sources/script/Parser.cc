#include <vull/script/Parser.hh>

#include <vull/core/Log.hh>
#include <vull/script/Builder.hh>
#include <vull/script/Lexer.hh>
#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull::script {
namespace {

unsigned precedence(Op op) {
    switch (op) {
    case Op::Add:
    case Op::Sub:
        return 1;
    case Op::Mul:
    case Op::Div:
        return 2;
    case Op::Negate:
        return 3;
    default:
        __builtin_unreachable();
    }
}

Op to_binary_op(TokenKind kind) {
    switch (kind) {
    case '+'_tk:
        return Op::Add;
    case '-'_tk:
        return Op::Sub;
    case '*'_tk:
        return Op::Mul;
    case '/'_tk:
        return Op::Div;
    default:
        return Op::None;
    }
}

} // namespace

class Scope {
    Scope *&m_current;
    Scope *m_parent;
    HashMap<StringView, uint8_t> m_local_map;

public:
    explicit Scope(Scope *&current);
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope();

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    Optional<uint8_t> lookup_local(StringView name) const;
    void put_local(StringView name, uint8_t reg);
};

Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Scope::~Scope() {
    m_current = m_parent;
}

Optional<uint8_t> Scope::lookup_local(StringView name) const {
    // TODO: Optional(Optional<U> &&) constructor
    auto opt = m_local_map.get(name);
    return opt ? *opt : Optional<uint8_t>();
}

void Scope::put_local(StringView name, uint8_t reg) {
    VULL_ENSURE(m_local_map.set(name, reg));
}

Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : Optional<Token>();
}

Token Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    if (token.kind() != kind) {
        message(MessageKind::Error, token,
                vull::format("expected {} but got {}", Token::kind_string(kind), token.to_string()));
    }
    return token;
}

void Parser::message(MessageKind kind, const Token &token, StringView message) {
    if (kind == MessageKind::Error) {
        m_error_count++;
    }
    StringView kind_string = kind == MessageKind::Error ? "\x1b[1;91merror" : "\x1b[1;35mnote";
    const auto [file_name, line_source, line, column] = m_lexer.recover_position(token);
    vull::println("\x1b[1;37m{}:{}:{}: {}: \x1b[1;37m{}\x1b[0m", file_name, line, column, kind_string, message);
    vull::print(" { 4 } | {}\n      |", line, line_source);
    for (uint32_t i = 0; i < column; i++) {
        vull::print(" ");
    }
    vull::println("\x1b[1;92m^\x1b[0m");
}

Op Parser::parse_subexpr(Expr &expr, unsigned prec) {
    if (consume('-'_tk)) {
        // Unary negate.
        parse_subexpr(expr, precedence(Op::Negate));
        // TODO: Emit an OP_negate.
    } else if (auto name = consume(TokenKind::Identifier)) {
        // TODO: VLOCAL that gets materialised later?
        expr.kind = ExprKind::Allocated;
        expr.index = *m_scope->lookup_local(name->string());
    } else if (consume('('_tk)) {
        parse_expr(expr);
        expect(')'_tk);
    } else if (auto token = consume(TokenKind::Number)) {
        expr.kind = ExprKind::Number;
        expr.number_value = token->number();
    } else {
        message(MessageKind::Error, m_lexer.peek(), "error");
    }

    auto binary_op = to_binary_op(m_lexer.peek().kind());
    while (binary_op != Op::None && precedence(binary_op) > prec) {
        const auto op_token = m_lexer.next();

        Expr rhs;
        Op next_op = parse_subexpr(rhs, precedence(binary_op));
        if (rhs.kind == ExprKind::Invalid) {
            const auto erroneous_token = m_lexer.peek();
            message(MessageKind::Error, op_token, "malformed expression");
            message(MessageKind::Note, erroneous_token,
                    vull::format("expected expression before {}", erroneous_token.to_string()));
            message(MessageKind::Note, erroneous_token,
                    vull::format("assuming {} is erroneous", erroneous_token.to_string()));
            continue;
        }
        m_builder.emit_binary(vull::exchange(binary_op, next_op), expr, rhs);
    }
    return binary_op;
}

void Parser::parse_expr(Expr &expr) {
    [[maybe_unused]] Op op = parse_subexpr(expr, 0);
    VULL_ASSERT(op == Op::None);
}

void Parser::parse_stmt() {
    if (consume(TokenKind::KW_let)) {
        auto name = expect(TokenKind::Identifier);
        expect('='_tk);
        Expr expr;
        parse_expr(expr);
        m_scope->put_local(name.string(), m_builder.materialise(expr));
        return;
    }
    if (consume(TokenKind::KW_return)) {
        Expr expr;
        parse_expr(expr);
        m_builder.emit_return(expr);
        return;
    }
    message(MessageKind::Error, m_lexer.next(), "expected statement");
}

void Parser::parse_block() {
    Scope scope(m_scope);
    while (!consume(TokenKind::Eof)) {
        parse_stmt();
    }
}

UniquePtr<Frame> Parser::parse() {
    parse_block();
    expect(TokenKind::Eof);
    return m_builder.build_frame();
}

} // namespace vull::script
