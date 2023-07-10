#pragma once

#include <vull/container/Array.hh>
#include <vull/script/Builder.hh>
#include <vull/script/Token.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull::script {

class ConstantPool;
class Frame;
class Lexer;

class ParseMessage {
public:
    enum class Kind {
        Error,
        Note,
    };

private:
    Token m_token;
    String m_message;
    Kind m_kind;

public:
    ParseMessage() = default;
    ParseMessage(Kind kind, const Token &token, String &&message)
        : m_token(token), m_message(vull::move(message)), m_kind(kind) {}

    const Token &token() const { return m_token; }
    const String &message() const { return m_message; }
    Kind kind() const { return m_kind; }
};

class ParseError {
    Array<ParseMessage, 3> m_messages;
    uint8_t m_message_count{0};

public:
    void add_error(const Token &token, String &&message) {
        m_messages[m_message_count++] = {ParseMessage::Kind::Error, token, vull::move(message)};
    }
    void add_note(const Token &token, String &&message) {
        m_messages[m_message_count++] = {ParseMessage::Kind::Note, token, vull::move(message)};
    }

    Span<const ParseMessage> messages() const { return m_messages.span().subspan(0, m_message_count); }
};

class Parser {
    class Scope;

private:
    Lexer &m_lexer;
    Builder m_builder;
    Scope *m_scope{nullptr};

    Optional<Token> consume(TokenKind kind);
    Result<Token, ParseError> expect(TokenKind kind);

    Result<Op, ParseError> parse_subexpr(Expr &expr, unsigned precedence);
    Result<void, ParseError> parse_expr(Expr &expr);
    Result<void, ParseError> parse_stmt();
    Result<void, ParseError> parse_block();

public:
    Parser(Lexer &lexer, ConstantPool &constant_pool) : m_lexer(lexer), m_builder(constant_pool) {}

    Result<UniquePtr<Frame>, ParseError> parse();
};

} // namespace vull::script
