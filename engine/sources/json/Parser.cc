#include <vull/json/Parser.hh>

#include <vull/json/Lexer.hh>
#include <vull/json/Token.hh>
#include <vull/json/Tree.hh>
#include <vull/support/Result.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

namespace vull::json {
namespace {

Result<Token, ParseError> expect(Lexer &lexer, TokenKind tk) {
    auto token = lexer.next();
    if (token.kind() != tk) [[unlikely]] {
        return ParseError{};
    }
    return token;
}

Result<Value, ParseError> parse_value(Lexer &lexer);

Result<Value, ParseError> parse_array(Lexer &lexer) {
    json::Array array;
    while (true) {
        if (lexer.peek().kind() == TokenKind::ArrayEnd) {
            lexer.next();
            break;
        }

        Value value = VULL_TRY(parse_value(lexer));
        array.push(vull::move(value));

        if (lexer.peek().kind() == TokenKind::ArrayEnd) {
            lexer.next();
            break;
        }

        VULL_TRY(expect(lexer, TokenKind::Comma));
    }
    return Value(vull::move(array));
}

Result<Value, ParseError> parse_object(Lexer &lexer) {
    json::Object object;
    while (true) {
        if (lexer.peek().kind() == TokenKind::ObjectEnd) {
            lexer.next();
            break;
        }

        auto key_token = VULL_TRY(expect(lexer, TokenKind::String));
        VULL_TRY(expect(lexer, TokenKind::Colon));
        Value value = VULL_TRY(parse_value(lexer));

        object.add(key_token.string(), vull::move(value));

        if (lexer.peek().kind() == TokenKind::ObjectEnd) {
            lexer.next();
            break;
        }

        VULL_TRY(expect(lexer, TokenKind::Comma));
    }
    return Value(vull::move(object));
}

Result<Value, ParseError> parse_value(Lexer &lexer) {
    Token token = lexer.next();
    switch (token.kind()) {
    case TokenKind::Decimal:
        return Value(token.decimal());
    case TokenKind::Integer:
        return Value(token.integer());
    case TokenKind::String:
        return Value(String(token.string()));
    case TokenKind::Null:
        return Value(Null{});
    case TokenKind::True:
        return Value(true);
    case TokenKind::False:
        return Value(false);
    case TokenKind::ArrayBegin:
        return parse_array(lexer);
    case TokenKind::ObjectBegin:
        return parse_object(lexer);
    default:
        return ParseError{};
    }
}

} // namespace

Result<Value, ParseError> parse(StringView source) {
    Lexer lexer(source);
    return parse_value(lexer);
}

} // namespace vull::json
