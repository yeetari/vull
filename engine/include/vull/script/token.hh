#pragma once

#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stddef.h>

namespace vull::script {

enum class TokenKind : uint16_t {
    Invalid,
    Eof,

    Identifier,
    Decimal,
    Integer,
    String,

    ListBegin,
    ListEnd,
    Quote,
};

class Token {
    const void *m_ptr_data{nullptr};
    union {
        double decimal_data;
        int64_t integer_data;
        uint64_t unsigned_data;
    } m_number_data{};
    uint32_t m_position;
    uint16_t m_line;
    TokenKind m_kind;

public:
    static String kind_string(TokenKind kind);

    Token(TokenKind kind, uint32_t position, uint16_t line) : m_position(position), m_line(line), m_kind(kind) {}
    Token(double decimal, uint32_t position, uint16_t line)
        : m_number_data{.decimal_data = decimal}, m_position(position), m_line(line), m_kind(TokenKind::Decimal) {}
    Token(int64_t integer, uint32_t position, uint16_t line)
        : m_number_data{.integer_data = integer}, m_position(position), m_line(line), m_kind(TokenKind::Integer) {}
    Token(TokenKind kind, StringView string, uint32_t position, uint16_t line)
        : m_ptr_data(string.data()), m_number_data{.unsigned_data = string.length()}, m_position(position),
          m_line(line), m_kind(kind) {}

    double decimal() const;
    int64_t integer() const;
    StringView string() const;
    String to_string() const;

    TokenKind kind() const { return m_kind; }
    uint32_t position() const { return m_position; }
    uint16_t line() const { return m_line; }
};

} // namespace vull::script
