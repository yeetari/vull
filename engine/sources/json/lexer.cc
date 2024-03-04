#include <vull/json/lexer.hh>

#include <vull/json/token.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/variant.hh>

namespace vull::json {

Token Lexer::next_token() {
    while (m_head < m_source.length() && is_space(m_source[m_head])) {
        m_head++;
    }

    if (m_head >= m_source.length()) {
        return Token(TokenKind::Eof);
    }

    const uint32_t begin = m_head;
    char ch = m_source[m_head++];

    if (ch == '"') {
        while (m_source[m_head] != '"' && m_head < m_source.length()) {
            m_head++;
        }
        m_head++;
        if (m_head > m_source.length()) {
            return Token(TokenKind::Invalid);
        }
        return Token(m_source.substr(begin + 1, m_head - begin - 2));
    }

    if (ch == '-') {
        if (!is_digit(ch = m_source[m_head++])) {
            return Token(TokenKind::Invalid);
        }
        auto number = parse_number(ch);
        if (const auto decimal = number.try_get<double>()) {
            return Token(-*decimal);
        }
        return Token(-static_cast<int64_t>(number.get<uint64_t>()));
    }
    if (is_digit(ch)) {
        auto number = parse_number(ch);
        if (const auto decimal = number.try_get<double>()) {
            return Token(*decimal);
        }
        return Token(static_cast<int64_t>(number.get<uint64_t>()));
    }

    if (ch == 'n' && m_head + 3 <= m_source.length() && m_source.substr(m_head, 3) == "ull") {
        m_head += 3;
        return Token(TokenKind::Null);
    }
    if (ch == 't' && m_head + 3 <= m_source.length() && m_source.substr(m_head, 3) == "rue") {
        m_head += 3;
        return Token(TokenKind::True);
    }
    if (ch == 'f' && m_head + 4 <= m_source.length() && m_source.substr(m_head, 4) == "alse") {
        m_head += 4;
        return Token(TokenKind::False);
    }

    switch (ch) {
    case '{':
        return Token(TokenKind::ObjectBegin);
    case '}':
        return Token(TokenKind::ObjectEnd);
    case '[':
        return Token(TokenKind::ArrayBegin);
    case ']':
        return Token(TokenKind::ArrayEnd);
    case ',':
        return Token(TokenKind::Comma);
    case ':':
        return Token(TokenKind::Colon);
    default:
        return Token(TokenKind::Invalid);
    }
}

} // namespace vull::json
