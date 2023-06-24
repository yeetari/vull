#include <vull/script/Parser.hh>

#include <vull/container/Array.hh>
#include <vull/container/HashMap.hh>
#include <vull/script/Builder.hh>
#include <vull/script/Bytecode.hh>
#include <vull/script/Lexer.hh>
#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull::script {
namespace {

struct Local {
    Token token;
    uint8_t reg;
};

unsigned precedence_of(Op op) {
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
        vull::unreachable();
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
    // TODO(small-map)
    HashMap<StringView, Local> m_local_map;

public:
    explicit Scope(Scope *&current);
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope();

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    Optional<uint8_t> lookup_local(StringView name) const;
    Optional<Token> put_local(const Token &token, uint8_t reg);
};

Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Scope::~Scope() {
    m_current = m_parent;
}

Optional<uint8_t> Scope::lookup_local(StringView name) const {
    auto opt = m_local_map.get(name);
    return opt ? opt->reg : Optional<uint8_t>();
}

Optional<Token> Scope::put_local(const Token &token, uint8_t reg) {
    if (auto previous = m_local_map.get(token.string())) {
        return previous->token;
    }
    m_local_map.set(token.string(), Local{token, reg});
    return {};
}

Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : Optional<Token>();
}

Result<Token, ParseError> Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    if (token.kind() != kind) {
        ParseError error;
        error.add_error(token, vull::format("expected {} but got {}", Token::kind_string(kind), token.to_string()));
        return error;
    }
    return token;
}

Result<Op, ParseError> Parser::parse_subexpr(Expr &expr, unsigned precedence) {
    if (consume('-'_tk)) {
        // Unary negate.
        VULL_TRY(parse_subexpr(expr, precedence_of(Op::Negate)));
        // TODO: Emit an OP_negate.
    } else if (auto name = consume(TokenKind::Identifier)) {
        // TODO: VLOCAL that gets materialised later?
        auto local = m_scope->lookup_local(name->string());
        if (!local) {
            ParseError error;
            error.add_error(*name, vull::format("no symbol named '{}' in the current scope", name->string()));
            return error;
        }
        expr.kind = ExprKind::Allocated;
        expr.index = *local;
    } else if (consume('('_tk)) {
        VULL_TRY(parse_expr(expr));
        VULL_TRY(expect(')'_tk));
    } else if (auto token = consume(TokenKind::Number)) {
        expr.kind = ExprKind::Number;
        expr.number_value = token->number();
    } else {
        ParseError error;
        error.add_error(m_lexer.peek(), "expected expression part");
        return error;
    }

    auto binary_op = to_binary_op(m_lexer.peek().kind());
    while (binary_op != Op::None && precedence_of(binary_op) > precedence) {
        m_lexer.next();
        Expr rhs;
        Op next_op = VULL_TRY(parse_subexpr(rhs, precedence_of(binary_op)));
        m_builder.emit_binary(vull::exchange(binary_op, next_op), expr, rhs);
    }
    return binary_op;
}

Result<void, ParseError> Parser::parse_expr(Expr &expr) {
    [[maybe_unused]] Op op = VULL_TRY(parse_subexpr(expr, 0));
    VULL_ASSERT(op == Op::None);
    return {};
}

Result<void, ParseError> Parser::parse_stmt() {
    if (consume(TokenKind::KW_let)) {
        auto name = VULL_TRY(expect(TokenKind::Identifier));
        VULL_TRY(expect('='_tk));
        Expr expr;
        VULL_TRY(parse_expr(expr));
        if (auto previous_name = m_scope->put_local(name, m_builder.materialise(expr))) {
            ParseError error;
            error.add_error(name, vull::format("redefinition of '{}'", name.string()));
            error.add_note(*previous_name, "previously defined here");
            return error;
        }
        return {};
    }

    if (consume(TokenKind::KW_return)) {
        Expr expr;
        VULL_TRY(parse_expr(expr));
        m_builder.emit_return(expr);
        return {};
    }

    ParseError error;
    error.add_error(m_lexer.next(), "expected statement");
    return error;
}

Result<void, ParseError> Parser::parse_block() {
    Scope scope(m_scope);
    while (!consume(TokenKind::Eof)) {
        VULL_TRY(parse_stmt());
    }
    return {};
}

Result<UniquePtr<Frame>, ParseError> Parser::parse() {
    VULL_TRY(parse_block());
    VULL_TRY(expect(TokenKind::Eof));
    return m_builder.build_frame();
}

} // namespace vull::script
