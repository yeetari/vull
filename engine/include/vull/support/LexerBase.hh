#pragma once

#include <vull/support/Optional.hh>

#include <cmath>

namespace vull {

template <typename Derived, typename TokenType>
class LexerBase {
    Optional<TokenType> m_peek_token;

    Derived &derived() { return *static_cast<Derived *>(this); }

protected:
    bool is_digit(uint8_t);
    bool is_ident(uint8_t);
    bool is_space(uint8_t);
    double parse_double(uint8_t);

public:
    const TokenType &peek();
    TokenType next();
};

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_digit(uint8_t ch) {
    return ch >= '0' && ch <= '9';
}

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_ident(uint8_t ch) {
    return ((static_cast<unsigned char>(ch) | 32u) - 'a' < 26) || ch == '_';
}

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_space(uint8_t ch) {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

template <typename Derived, typename TokenType>
double LexerBase<Derived, TokenType>::parse_double(uint8_t ch) {
    double value = ch - '0';
    while (is_digit(ch = derived().next_char())) {
        value = value * 10 + (ch - '0');
    }

    if (ch != '.') {
        derived().unskip_char();
        return value;
    }

    double addend = 0;
    double power = 1;
    while (is_digit(ch = derived().next_char())) {
        addend = addend * 10 + (ch - '0');
        power *= 10;
    }
    value += addend / power;

    if (ch != 'e') [[likely]] {
        derived().unskip_char();
        return value;
    }

    unsigned exponent = 0;
    while (is_digit(ch = derived().peek_char())) {
        exponent = exponent * 10 + (ch - '0');
        derived().skip_char();
    }
    value *= ::pow(10, exponent);
    return value;
}

template <typename Derived, typename TokenType>
const TokenType &LexerBase<Derived, TokenType>::peek() {
    if (!m_peek_token) {
        m_peek_token = derived().next_token();
    }
    return *m_peek_token;
}

template <typename Derived, typename TokenType>
TokenType LexerBase<Derived, TokenType>::next() {
    if (m_peek_token) {
        auto token = *m_peek_token;
        if (!Derived::is_eof(token)) [[likely]] {
            m_peek_token.clear();
        }
        return token;
    }
    return derived().next_token();
}

} // namespace vull
