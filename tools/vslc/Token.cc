#include "Token.hh"

#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>

vull::String Token::kind_string(TokenKind kind) {
    if (auto value = vull::to_underlying(kind); value < 256) {
        vull::String string("'x'");
        string.data()[1] = static_cast<char>(value);
        return string;
    }
    switch (kind) {
    case TokenKind::Eof:
        return "eof";
    case TokenKind::FloatLit:
        return "float literal";
    case TokenKind::Ident:
        return "identifier";
    case TokenKind::IntLit:
        return "integer literal";
    default:
        VULL_ENSURE_NOT_REACHED();
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
