#include <vull/json/Lexer.hh>

#include <vull/json/Token.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>

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
        return Token(m_source.substr(begin + 1, m_head - 1));
    }

    if (ch == '-') {
        if (!is_digit(ch = m_source[m_head++])) {
            return Token(TokenKind::Invalid);
        }
        return Token(-parse_double(ch));
    }
    if (is_digit(ch)) {
        return Token(parse_double(ch));
    }

    if (ch == 'n' && m_head + 3 <= m_source.length() && m_source.substr(m_head, m_head + 3) == "ull") {
        m_head += 3;
        return Token(TokenKind::Null);
    }
    if (ch == 't' && m_head + 3 <= m_source.length() && m_source.substr(m_head, m_head + 3) == "rue") {
        m_head += 3;
        return Token(TokenKind::True);
    }
    if (ch == 'f' && m_head + 4 <= m_source.length() && m_source.substr(m_head, m_head + 4) == "alse") {
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
