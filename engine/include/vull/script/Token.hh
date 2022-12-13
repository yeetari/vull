#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>

namespace vull::script {

enum class TokenKind {
    Invalid = 256,
    Eof,
    Identifier,
    Number,

    KW_function,
    KW_let,
    KW_return,
};

class Token {
    const void *m_ptr_data{nullptr};
    union {
        double decimal_data;
        size_t integer_data;
    } m_number_data{};
    TokenKind m_kind;
    uint32_t m_position;

public:
    static String kind_string(TokenKind kind);

    Token() = default;
    Token(TokenKind kind, uint32_t position) : m_kind(kind), m_position(position) {}
    Token(double decimal, uint32_t position)
        : m_number_data{.decimal_data = decimal}, m_kind(TokenKind::Number), m_position(position) {}
    Token(TokenKind kind, StringView string, uint32_t position)
        : m_ptr_data(string.data()), m_number_data{.integer_data = string.length()}, m_kind(kind),
          m_position(position) {}

    TokenKind kind() const { return m_kind; }
    uint32_t position() const { return m_position; }
    double number() const;
    StringView string() const;
    String to_string() const;
};

constexpr TokenKind operator""_tk(char ch) {
    return static_cast<TokenKind>(ch);
}

} // namespace vull::script
