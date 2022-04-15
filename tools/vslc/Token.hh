#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>

enum class TokenKind {
    Eof,
    Ident,
    IntLit,
    KeywordFn,
    LeftBrace,
    LeftParen,
    RightBrace,
    RightParen,
};

class Token {
    size_t m_int_data{0};
    const void *m_ptr_data{nullptr};
    TokenKind m_kind;

public:
    static vull::StringView kind_string(TokenKind kind);

    Token(TokenKind kind) : m_kind(kind) {}
    Token(size_t number) : m_int_data(number), m_kind(TokenKind::IntLit) {}
    Token(TokenKind kind, vull::StringView string)
        : m_int_data(string.length()), m_ptr_data(string.data()), m_kind(kind) {}

    TokenKind kind() const { return m_kind; }
    size_t number() const;
    vull::StringView string() const;
    vull::String to_string() const;
};
