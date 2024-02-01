#pragma once

#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stddef.h>

enum class TokenKind {
    Eof = 256,
    FloatLit,
    Ident,
    IntLit,

    PlusEqual,
    MinusEqual,
    AsteriskEqual,
    SlashEqual,

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
    TokenKind m_kind;

public:
    static vull::String kind_string(TokenKind kind);

    Token(TokenKind kind) : m_kind(kind) {}
    Token(float number) : m_number_data{.float_data = number}, m_kind(TokenKind::FloatLit) {}
    Token(size_t number) : m_number_data{.int_data = number}, m_kind(TokenKind::IntLit) {}
    Token(TokenKind kind, vull::StringView string)
        : m_ptr_data(string.data()), m_number_data{.int_data = string.length()}, m_kind(kind) {}

    TokenKind kind() const { return m_kind; }
    float decimal() const;
    size_t integer() const;
    vull::StringView string() const;
    vull::String to_string() const;
};

constexpr TokenKind operator""_tk(char ch) {
    return static_cast<TokenKind>(ch);
}
