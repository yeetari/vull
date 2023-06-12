#include "Lexer.hh"

#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>

#include <stdio.h>

Token Lexer::next_token() {
    while (m_stream.has_next() && is_space(m_stream.peek())) {
        m_stream.next();
    }
    if (!m_stream.has_next()) {
        return TokenKind::Eof;
    }

    char ch = m_stream.next();
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
            return TokenKind::KW_fn;
        }
        if (ident == "let") {
            return TokenKind::KW_let;
        }
        if (ident == "uniform") {
            return TokenKind::KW_uniform;
        }
        return {TokenKind::Ident, ident};
    }

    if (ch == '/') {
        // Handle comments.
        if (m_stream.peek() == '/') {
            while (m_stream.peek() != '\n') {
                m_stream.next();
            }
            return next_token();
        }
        return '/'_tk;
    }

    if (ch > 31) {
        return static_cast<TokenKind>(ch);
    }

    printf("unexpected %c\n", ch);
    VULL_ENSURE_NOT_REACHED();
}
