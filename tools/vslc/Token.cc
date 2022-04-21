#include "Token.hh"

#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>

vull::StringView Token::kind_string(TokenKind kind) {
    switch (kind) {
    case TokenKind::Asterisk:
        return "'*'";
    case TokenKind::Colon:
        return "':'";
    case TokenKind::Comma:
        return "','";
    case TokenKind::Eof:
        return "eof";
    case TokenKind::Equals:
        return "'='";
    case TokenKind::FloatLit:
        return "float literal";
    case TokenKind::Ident:
        return "identifier";
    case TokenKind::IntLit:
        return "integer literal";
    case TokenKind::KeywordFn:
        return "'fn'";
    case TokenKind::KeywordLet:
        return "'let'";
    case TokenKind::LeftBrace:
        return "'{'";
    case TokenKind::LeftParen:
        return "'('";
    case TokenKind::Minus:
        return "'-'";
    case TokenKind::Percent:
        return "'%'";
    case TokenKind::Plus:
        return "'+'";
    case TokenKind::RightBrace:
        return "'}'";
    case TokenKind::RightParen:
        return "')'";
    case TokenKind::Semi:
        return "';'";
    case TokenKind::Slash:
        return "'/'";
    }
}

float Token::decimal() const {
    VULL_ASSERT(m_kind == TokenKind::FloatLit);
    return m_number_data.float_data;
}

size_t Token::integer() const {
    VULL_ASSERT(m_kind == TokenKind::IntLit);
    return m_number_data.int_data;
}

vull::StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::Ident);
    return {static_cast<const char *>(m_ptr_data), m_number_data.int_data};
}

vull::String Token::to_string() const {
    switch (m_kind) {
    case TokenKind::FloatLit:
        return vull::format("'{}f'", decimal());
    case TokenKind::Ident:
        return vull::format("'{}'", string());
    case TokenKind::IntLit:
        return vull::format("'{}u'", integer());
    default:
        return kind_string(m_kind);
    }
}
