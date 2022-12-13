#pragma once

#include <vull/script/Builder.hh>
#include <vull/script/Token.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>

#include <stddef.h>

namespace vull::script {

class ConstantPool;
class Frame;
class Lexer;
class Scope;

class Parser {
    enum class MessageKind {
        Error,
        Note,
    };

private:
    Lexer &m_lexer;
    Builder m_builder;
    Scope *m_scope{nullptr};
    size_t m_error_count{0};

    Optional<Token> consume(TokenKind kind);
    Token expect(TokenKind kind);
    void message(MessageKind kind, const Token &token, StringView message);

    Op parse_subexpr(Expr &expr, unsigned prec);
    void parse_expr(Expr &expr);
    void parse_stmt();
    void parse_block();

public:
    Parser(Lexer &lexer, ConstantPool &constant_pool) : m_lexer(lexer), m_builder(constant_pool) {}

    UniquePtr<Frame> parse();
    size_t error_count() const { return m_error_count; }
};

} // namespace vull::script
