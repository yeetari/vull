#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>

enum class TokenKind {
    Asterisk,
    Colon,
    Comma,
    Eof,
    Equals,
    FloatLit,
    Ident,
    IntLit,
    KeywordFn,
    KeywordLet,
    LeftBrace,
    LeftParen,
    Minus,
    Percent,
    Plus,
    RightBrace,
    RightParen,
    Semi,
    Slash,
};

class Token {
    const void *m_ptr_data{nullptr};
    union {
        size_t int_data;
        float float_data;
    } m_number_data{};
    TokenKind m_kind;

public:
    static vull::StringView kind_string(TokenKind kind);

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
