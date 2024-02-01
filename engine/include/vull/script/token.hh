#pragma once

#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stddef.h>

namespace vull::script {

enum class TokenKind : uint16_t {
    Invalid = 256,
    Eof,
    Identifier,
    Number,

    EqualEqual,
    NotEqual,
    LessEqual,
    GreaterEqual,

    KW_elif,
    KW_else,
    KW_end,
    KW_function,
    KW_if,
    KW_let,
    KW_return,
};

class Token {
    const void *m_ptr_data{nullptr};
    union {
        double decimal_data;
        size_t integer_data;
    } m_number_data{};
    uint32_t m_position;
    uint16_t m_line;
    TokenKind m_kind;

public:
    static String kind_string(TokenKind kind);

    Token() = default;
    Token(TokenKind kind, uint32_t position, uint16_t line) : m_position(position), m_line(line), m_kind(kind) {}
    Token(double decimal, uint32_t position, uint16_t line)
        : m_number_data{.decimal_data = decimal}, m_position(position), m_line(line), m_kind(TokenKind::Number) {}
    Token(TokenKind kind, StringView string, uint32_t position, uint16_t line)
        : m_ptr_data(string.data()), m_number_data{.integer_data = string.length()}, m_position(position), m_line(line),
          m_kind(kind) {}

    bool is_one_of(auto... kinds) const;
    double number() const;
    StringView string() const;
    String to_string() const;

    TokenKind kind() const { return m_kind; }
    uint32_t position() const { return m_position; }
    uint16_t line() const { return m_line; }
};

bool Token::is_one_of(auto... kinds) const {
    return ((m_kind == kinds) || ...);
}

constexpr TokenKind operator""_tk(char ch) {
    return static_cast<TokenKind>(ch);
}

} // namespace vull::script
