#include "Lexer.hh"

#include <vull/support/Assert.hh>

namespace {

bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

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
    case ',':
        return TokenKind::Comma;
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

    if (is_digit(ch)) {
        size_t length = 1;
        bool is_decimal = false;
        while (is_digit(m_stream.peek()) || m_stream.peek() == '.') {
            length++;
            if (m_stream.next() == '.') {
                is_decimal = true;
            }
        }
        // TODO: Don't use scanf.
        vull::StringView view(m_stream.pointer() - length, length);
        if (is_decimal) {
            float decimal = 0.0f;
            sscanf(view.data(), "%f", &decimal);
            if (m_stream.peek() == 'f') {
                m_stream.next();
            }
            return decimal;
        }
        size_t integer = 0;
        sscanf(view.data(), "%lu", &integer);
        return integer;
    }

    if (is_ident(ch)) {
        size_t length = 1;
        while (is_ident(m_stream.peek()) || is_digit(m_stream.peek())) {
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
