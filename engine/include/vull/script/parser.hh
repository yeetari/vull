#pragma once

#include <vull/container/vector.hh>
#include <vull/script/token.hh>
#include <vull/script/value.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::script {

class Lexer;
class Vm;

class ParseMessage {
public:
    enum class Kind {
        Error,
        Note,
    };

private:
    Token m_token;
    String m_text;
    Kind m_kind;

public:
    ParseMessage(Kind kind, const Token &token, String &&text)
        : m_token(token), m_text(vull::move(text)), m_kind(kind) {}

    const Token &token() const { return m_token; }
    const String &text() const { return m_text; }
    Kind kind() const { return m_kind; }
};

class ParseError {
    // TODO(small-vector)
    Vector<ParseMessage> m_messages;

public:
    ParseError() = default;
    ParseError(const ParseError &other) { m_messages.extend(other.m_messages); }
    ParseError(ParseError &&) = default;
    ~ParseError() = default;

    ParseError &operator=(const ParseError &) = delete;
    ParseError &operator=(ParseError &&) = delete;

    void add_error(const Token &token, String &&message);
    void add_note(const Token &token, String &&message);

    const Vector<ParseMessage> &messages() const { return m_messages; }
};

class Parser {
    Vm &m_vm;
    Lexer &m_lexer;

    Optional<Token> consume(TokenKind kind);
    Result<Value, ParseError> parse_quote();
    Result<Value, ParseError> parse_list();
    Result<Value, ParseError> parse_form();

public:
    Parser(Vm &vm, Lexer &lexer) : m_vm(vm), m_lexer(lexer) {}

    Result<Value, ParseError> parse();
};

} // namespace vull::script
