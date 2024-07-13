#include <vull/script/parser.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/script/lexer.hh>
#include <vull/script/token.hh>
#include <vull/script/value.hh>
#include <vull/script/vm.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/utility.hh>

namespace vull::script {

void ParseError::add_error(const Token &token, String &&message) {
    m_messages.emplace(ParseMessage::Kind::Error, token, vull::move(message));
}

void ParseError::add_note(const Token &token, String &&message) {
    m_messages.emplace(ParseMessage::Kind::Note, token, vull::move(message));
}

Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : Optional<Token>();
}

Result<Value, ParseError> Parser::parse_quote() {
    Array list{
        m_vm.make_symbol("quote"),
        VULL_TRY(parse_form()),
    };
    return m_vm.make_list(list.span());
}

Result<Value, ParseError> Parser::parse_list() {
    if (consume(TokenKind::ListEnd)) {
        return Value::null();
    }

    Vector<Value> list;
    while (!consume(TokenKind::ListEnd)) {
        list.push(VULL_TRY(parse_form()));
    }
    return m_vm.make_list(list.span());
}

Result<Value, ParseError> Parser::parse_form() {
    if (consume(TokenKind::Quote)) {
        return VULL_TRY(parse_quote());
    }
    if (consume(TokenKind::ListBegin)) {
        return VULL_TRY(parse_list());
    }
    if (auto token = consume(TokenKind::Identifier)) {
        return m_vm.make_symbol(token->string());
    }
    if (auto token = consume(TokenKind::Integer)) {
        return Value::integer(token->integer());
    }
    if (auto token = consume(TokenKind::Decimal)) {
        return Value::real(token->decimal());
    }
    if (auto token = consume(TokenKind::String)) {
        return m_vm.make_string(token->string());
    }

    auto token = m_lexer.next();
    ParseError error;
    error.add_error(token, vull::format("unexpected token {}", token.to_string()));
    error.add_note(token, "expected atom or end of list");
    return error;
}

Result<Value, ParseError> Parser::parse() {
    Vector<Value> list;
    list.push(m_vm.make_symbol("seq"));
    while (!consume(TokenKind::Eof)) {
        list.push(VULL_TRY(parse_form()));
    }
    return m_vm.make_list(list.span());
}

} // namespace vull::script
