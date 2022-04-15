#include "Lexer.hh"

#include <vull/support/Assert.hh>

namespace {

bool is_ident(char ch) {
    return ((static_cast<unsigned char>(ch) | 32u) - 'a' < 26) || ch == '_';
}

bool is_space(char ch) {
    return ch == ' ' || ch - '\t' < 5;
}

} // namespace

Token Lexer::next_token() {
    while (m_stream.has_next() && is_space(m_stream.peek())) {
        m_stream.next();
    }
    if (!m_stream.has_next()) {
        return TokenKind::Eof;
    }

    char ch = m_stream.next();
    switch (ch) {
    case '/':
        // Handle comments.
        if (m_stream.peek() == '/') {
            while (m_stream.peek() != '\n') {
                m_stream.next();
            }
            return next_token();
        }
        break;
    case '{':
        return TokenKind::LeftBrace;
    case '}':
        return TokenKind::RightBrace;
    case '(':
        return TokenKind::LeftParen;
    case ')':
        return TokenKind::RightParen;
    default:
        break;
    }

    if (is_ident(ch)) {
        size_t length = 1;
        while (is_ident(m_stream.peek())) {
            length++;
            m_stream.next();
        }
        vull::StringView ident(m_stream.pointer() - length, length);
        if (ident == "fn") {
            return TokenKind::KeywordFn;
        }
        return {TokenKind::Ident, ident};
    }
    printf("unexpected %c\n", ch);
    VULL_ENSURE_NOT_REACHED();
}

const Token &Lexer::peek() {
    if (!vull::exchange(m_peek_ready, true)) {
        m_peek_token = next_token();
    }
    return m_peek_token;
}

Token Lexer::next() {
    if (vull::exchange(m_peek_ready, false)) {
        return vull::move(m_peek_token);
    }
    return next_token();
}
