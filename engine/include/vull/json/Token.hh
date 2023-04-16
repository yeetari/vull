#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>

namespace vull::json {

enum class TokenKind {
    Invalid,
    Eof,

    String,
    Number,

    ArrayBegin,
    ArrayEnd,
    ObjectBegin,
    ObjectEnd,
    Colon,
    Comma,

    Null,
    True,
    False,
};

class Token {
    const void *m_data_ptr{nullptr};
    union {
        double d;
        size_t i;
    } m_data_numeric{};
    TokenKind m_kind;

public:
    explicit Token(TokenKind kind) : m_kind(kind) {}
    explicit Token(double d) : m_data_numeric{.d = d}, m_kind(TokenKind::Number) {}
    explicit Token(StringView sv)
        : m_data_ptr(sv.data()), m_data_numeric{.i = sv.length()}, m_kind(TokenKind::String) {}

    TokenKind kind() const { return m_kind; }
    double number() const;
    StringView string() const;
};

inline double Token::number() const {
    VULL_ASSERT(m_kind == TokenKind::Number, "token not a number");
    return m_data_numeric.d;
}

inline StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::String, "token not a string");
    return {static_cast<const char *>(m_data_ptr), m_data_numeric.i};
}

} // namespace vull::json
