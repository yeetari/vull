#include "lexer.hh"

#include <vull/support/assert.hh>
#include <vull/support/string_view.hh>

#include <stdio.h>

Token Lexer::next_token() {
    while (m_stream.has_next() && is_space(m_stream.peek())) {
        m_stream.next();
    }
    if (!m_stream.has_next()) {
        return TokenKind::Eof;
    }

    auto consume = [this](char ch) {
        if (m_stream.peek() == ch) {
            m_stream.next();
            return true;
        }
        return false;
    };

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
        if (ident == "pipeline") {
            return TokenKind::KW_pipeline;
        }
        if (ident == "uniform") {
            return TokenKind::KW_uniform;
        }
        if (ident == "var") {
            return TokenKind::KW_var;
        }
        return {TokenKind::Ident, ident};
    }

    if (ch == '/' && consume('/')) {
        while (m_stream.peek() != '\n') {
            m_stream.next();
        }
        return next_token();
    }

    if (ch == '+' && consume('=')) {
        return TokenKind::PlusEqual;
    }
    if (ch == '-' && consume('=')) {
        return TokenKind::MinusEqual;
    }
    if (ch == '*' && consume('=')) {
        return TokenKind::AsteriskEqual;
    }
    if (ch == '/' && consume('=')) {
        return TokenKind::SlashEqual;
    }

    if (ch > 31) {
        return static_cast<TokenKind>(ch);
    }

    printf("unexpected %c\n", ch);
    VULL_ENSURE_NOT_REACHED();
}
