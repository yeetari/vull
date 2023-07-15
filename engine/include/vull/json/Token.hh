#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>

#include <stddef.h>

namespace vull::json {

enum class TokenKind {
    Invalid,
    Eof,

    Decimal,
    Integer,
    String,

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
        int64_t si;
        uint64_t ui;
    } m_data_numeric{};
    TokenKind m_kind;

public:
    explicit Token(TokenKind kind) : m_kind(kind) {}
    explicit Token(double d) : m_data_numeric{.d = d}, m_kind(TokenKind::Decimal) {}
    explicit Token(int64_t si) : m_data_numeric{.si = si}, m_kind(TokenKind::Integer) {}
    explicit Token(StringView sv)
        : m_data_ptr(sv.data()), m_data_numeric{.ui = sv.length()}, m_kind(TokenKind::String) {}

    TokenKind kind() const { return m_kind; }
    double decimal() const;
    int64_t integer() const;
    StringView string() const;
};

inline double Token::decimal() const {
    VULL_ASSERT(m_kind == TokenKind::Decimal, "token not a decimal");
    return m_data_numeric.d;
}

inline int64_t Token::integer() const {
    VULL_ASSERT(m_kind == TokenKind::Integer, "token not an integer");
    return m_data_numeric.si;
}

inline StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::String, "token not a string");
    return {static_cast<const char *>(m_data_ptr), m_data_numeric.ui};
}

} // namespace vull::json
