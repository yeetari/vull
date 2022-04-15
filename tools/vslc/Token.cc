#include "Token.hh"

#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>

vull::StringView Token::kind_string(TokenKind kind) {
    switch (kind) {
    case TokenKind::Eof:
        return "eof";
    case TokenKind::Ident:
        return "identifier";
    case TokenKind::IntLit:
        return "integer literal";
    case TokenKind::KeywordFn:
        return "'fn'";
    case TokenKind::LeftBrace:
        return "'{'";
    case TokenKind::LeftParen:
        return "'('";
    case TokenKind::RightBrace:
        return "'}'";
    case TokenKind::RightParen:
        return "')'";
    }
}

size_t Token::number() const {
    VULL_ASSERT(m_kind == TokenKind::IntLit);
    return m_int_data;
}

vull::StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::Ident);
    return {static_cast<const char *>(m_ptr_data), m_int_data};
}

vull::String Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Ident:
        return vull::format("'{}'", string());
    case TokenKind::IntLit:
        return vull::format("'{}'", number());
    default:
        return kind_string(m_kind);
    }
}
