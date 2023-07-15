#include <vull/json/Parser.hh>

#include <vull/json/Lexer.hh>
#include <vull/json/Token.hh>
#include <vull/json/Tree.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

// TODO: Error handling.

namespace vull::json {
namespace {

Value parse_value(Lexer &lexer);

Value parse_array(Lexer &lexer) {
    json::Array array;
    while (true) {
        if (lexer.peek().kind() == TokenKind::ArrayEnd) {
            lexer.next();
            break;
        }

        Value value = parse_value(lexer);
        array.push(vull::move(value));

        if (lexer.peek().kind() == TokenKind::ArrayEnd) {
            lexer.next();
            break;
        }

        if (lexer.next().kind() != TokenKind::Comma) {
            VULL_ENSURE_NOT_REACHED();
        }
    }
    return array;
}

Value parse_object(Lexer &lexer) {
    json::Object object;
    while (true) {
        if (lexer.peek().kind() == TokenKind::ObjectEnd) {
            lexer.next();
            break;
        }

        auto key_token = lexer.next();
        VULL_ENSURE(key_token.kind() == TokenKind::String);
        VULL_ENSURE(lexer.next().kind() == TokenKind::Colon);
        Value value = parse_value(lexer);

        object.add(key_token.string(), vull::move(value));

        if (lexer.peek().kind() == TokenKind::ObjectEnd) {
            lexer.next();
            break;
        }

        VULL_ENSURE(lexer.next().kind() == TokenKind::Comma);
    }
    return object;
}

Value parse_value(Lexer &lexer) {
    Token token = lexer.next();
    switch (token.kind()) {
    case TokenKind::Decimal:
        return token.decimal();
    case TokenKind::Integer:
        return token.integer();
    case TokenKind::String:
        return String(token.string());
    case TokenKind::Null:
        return Null{};
    case TokenKind::True:
        return true;
    case TokenKind::False:
        return false;
    case TokenKind::ArrayBegin:
        return parse_array(lexer);
    case TokenKind::ObjectBegin:
        return parse_object(lexer);
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

Result<Value, JsonError> parse(StringView source) {
    Lexer lexer(source);
    return parse_value(lexer);
}

} // namespace vull::json
