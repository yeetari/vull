#pragma once

#include <vull/support/optional.hh>
#include <vull/support/variant.hh>

namespace vull {

template <typename Derived, typename TokenType>
class LexerBase {
    Derived &derived() { return *static_cast<Derived *>(this); }

protected:
    Optional<TokenType> m_peek_token;

    bool is_digit(auto);
    bool is_ident(auto);
    bool is_space(auto);
    Variant<double, uint64_t> parse_number(auto);

public:
    const TokenType &peek();
    TokenType next();
};

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_digit(auto ch) {
    return ch >= '0' && ch <= '9';
}

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_ident(auto ch) {
    return ((static_cast<unsigned char>(ch) | 32u) - 'a' < 26) || ch == '_';
}

template <typename Derived, typename TokenType>
bool LexerBase<Derived, TokenType>::is_space(auto ch) {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

template <typename Derived, typename TokenType>
Variant<double, uint64_t> LexerBase<Derived, TokenType>::parse_number(auto ch) {
    auto whole_part = static_cast<uint64_t>(ch - '0');
    while (is_digit(ch = derived().next_char())) {
        whole_part = whole_part * 10 + static_cast<uint64_t>(ch - '0');
    }

    if (ch != '.' && ch != 'e' && ch != 'E') {
        derived().unskip_char();
        return whole_part;
    }

    auto value = static_cast<double>(whole_part);
    if (ch == '.') {
        double addend = 0;
        double power = 1;
        while (is_digit(ch = derived().next_char())) {
            addend = addend * 10 + (ch - '0');
            power *= 10;
        }
        value += addend / power;
    }

    if (ch != 'e' && ch != 'E') [[likely]] {
        derived().unskip_char();
        return value;
    }

    bool negative_exponent = false;
    switch (derived().peek_char()) {
    case '-':
        negative_exponent = true;
        [[fallthrough]];
    case '+':
        derived().skip_char();
        break;
    }

    unsigned exponent = 0;
    while (is_digit(ch = derived().peek_char())) {
        exponent = exponent * 10 + static_cast<unsigned>(ch - '0');
        derived().skip_char();
    }
    value *= __builtin_pow(10, negative_exponent ? -exponent : exponent);
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
