#pragma once

#include <vull/shaderc/source_location.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stddef.h>

namespace vull::shaderc {

enum class TokenKind : uint16_t {
    Invalid = 256,
    Cursor,
    Eof,
    Identifier,
    FloatLit,
    IntLit,
    StringLit,

    PlusEqual,
    MinusEqual,
    AsteriskEqual,
    SlashEqual,
    DoubleOpenSquareBrackets,
    DoubleCloseSquareBrackets,

    KW_fn,
    KW_let,
    KW_pipeline,
    KW_uniform,
    KW_var,
};

class Token {
    const void *m_ptr_data{nullptr};
    union {
        size_t int_data;
        float float_data;
    } m_number_data{};
    uint32_t m_position;
    uint16_t m_line;
    TokenKind m_kind;

public:
    static String kind_string(TokenKind kind);

    Token(TokenKind kind, uint32_t position, uint16_t line) : m_position(position), m_line(line), m_kind(kind) {}
    Token(float decimal, uint32_t position, uint16_t line)
        : m_number_data{.float_data = decimal}, m_position(position), m_line(line), m_kind(TokenKind::FloatLit) {}
    Token(size_t integer, uint32_t position, uint16_t line)
        : m_number_data{.int_data = integer}, m_position(position), m_line(line), m_kind(TokenKind::IntLit) {}
    Token(TokenKind kind, StringView string, uint32_t position, uint16_t line)
        : m_ptr_data(string.data()), m_number_data{.int_data = string.length()}, m_position(position), m_line(line),
          m_kind(kind) {}

    float decimal() const;
    size_t integer() const;
    StringView string() const;
    String to_string() const;

    TokenKind kind() const { return m_kind; }
    SourceLocation location() const { return {m_position, m_line}; }
    uint32_t position() const { return m_position; }
    uint16_t line() const { return m_line; }
};

constexpr TokenKind operator""_tk(char ch) {
    return static_cast<TokenKind>(ch);
}

} // namespace vull::shaderc
