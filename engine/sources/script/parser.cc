#include <vull/script/parser.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/script/builder.hh>
#include <vull/script/bytecode.hh>
#include <vull/script/lexer.hh>
#include <vull/script/token.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::script {
namespace {

struct Local {
    Token token;
    uint8_t reg;
};

Array<unsigned, vull::to_underlying(Op::_count)> s_precedence_table{
    0,          // None
    3, 3,       // Add, Sub
    4, 4,       // Mul, Div
    1, 1,       // Equal, NotEqual
    2, 2, 2, 2, // LessThan, LessEqual, GreaterThan, GreaterEqual
    5,          // Negate
};

unsigned precedence_of(Op op) {
    return s_precedence_table[vull::to_underlying(op)];
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
    case TokenKind::EqualEqual:
        return Op::Equal;
    case TokenKind::NotEqual:
        return Op::NotEqual;
    case '<'_tk:
        return Op::LessThan;
    case TokenKind::LessEqual:
        return Op::LessEqual;
    case '>'_tk:
        return Op::GreaterThan;
    case TokenKind::GreaterEqual:
        return Op::GreaterEqual;
    default:
        return Op::None;
    }
}

} // namespace

class Parser::Scope {
    Scope *&m_current;
    Scope *m_parent;
    // TODO(small-map)
    HashMap<StringView, Local> m_local_map;

public:
    explicit Scope(Scope *&current) : m_current(current), m_parent(current) { current = this; }
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope() { m_current = m_parent; }

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    Optional<uint8_t> lookup_local(StringView name) const;
    Optional<Token> put_local(const Token &token, uint8_t reg);
};

Optional<uint8_t> Parser::Scope::lookup_local(StringView name) const {
    if (auto local = m_local_map.get(name)) {
        return local->reg;
    }
    if (m_parent == nullptr) {
        return {};
    }
    return m_parent->lookup_local(name);
}

Optional<Token> Parser::Scope::put_local(const Token &token, uint8_t reg) {
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
        m_builder.emit_unary(Op::Negate, expr);
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
        // Skip operator token.
        m_lexer.next();

        // Parse right hand side recursively.
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

Result<void, ParseError> Parser::parse_if_stmt() {
    // TODO(small-vector)
    Vector<uint32_t> exit_jump_pcs;
    uint32_t else_jump_pc;
    do {
        Expr condition_expr;
        VULL_TRY(parse_expr(condition_expr));

        // Emit a jump to the next elif/else branch if the condition is false.
        else_jump_pc = m_builder.emit_jump();

        // Parse the then block and emit a jump to the end of the if for when the branch is done.
        VULL_TRY(parse_block());
        exit_jump_pcs.push(m_builder.emit_jump());

        // Next branch starts here.
        m_builder.patch_jump_to_here(else_jump_pc);
    } while (consume(TokenKind::KW_elif));

    // Parse any else branch.
    if (consume(TokenKind::KW_else)) {
        VULL_TRY(parse_block());
    }
    VULL_TRY(expect(TokenKind::KW_end));

    // Patch up exit jumps.
    for (uint32_t jump_pc : exit_jump_pcs) {
        m_builder.patch_jump_to_here(jump_pc);
    }
    return {};
}

Result<void, ParseError> Parser::parse_let_stmt() {
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

Result<void, ParseError> Parser::parse_return_stmt() {
    Expr expr;
    VULL_TRY(parse_expr(expr));
    m_builder.emit_return(expr);
    return {};
}

Result<void, ParseError> Parser::parse_stmt() {
    if (consume(TokenKind::KW_if)) {
        return parse_if_stmt();
    }
    if (consume(TokenKind::KW_let)) {
        return parse_let_stmt();
    }
    if (consume(TokenKind::KW_return)) {
        return parse_return_stmt();
    }

    ParseError error;
    error.add_error(m_lexer.next(), "expected statement");
    return error;
}

Result<void, ParseError> Parser::parse_block() {
    Scope scope(m_scope);
    while (!m_lexer.peek().is_one_of(TokenKind::KW_end, TokenKind::KW_else, TokenKind::KW_elif)) {
        VULL_TRY(parse_stmt());
    }
    return {};
}

Result<void, ParseError> Parser::parse_function() {
    VULL_TRY(expect(TokenKind::Identifier));
    VULL_TRY(expect('('_tk));
    VULL_TRY(expect(')'_tk));
    VULL_TRY(parse_block());
    VULL_TRY(expect(TokenKind::KW_end));
    m_builder.emit_return({});
    return {};
}

Result<void, ParseError> Parser::parse_top_level() {
    if (consume(TokenKind::KW_function)) {
        return parse_function();
    }

    ParseError error;
    error.add_error(m_lexer.next(), "expected top level declaration");
    return error;
}

Result<UniquePtr<Frame>, ParseError> Parser::parse() {
    while (!consume(TokenKind::Eof)) {
        VULL_TRY(parse_top_level());
    }
    return m_builder.build_frame();
}

} // namespace vull::script
