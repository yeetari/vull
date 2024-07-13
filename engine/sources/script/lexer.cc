#include <vull/script/lexer.hh>

#include <vull/script/token.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

#define MAKE_TOKEN(...)                                                                                                \
    { __VA_ARGS__, position, m_line }

namespace vull::script {

Lexer::Lexer(String file_name, String source) : m_file_name(vull::move(file_name)), m_source(vull::move(source)) {
    if (m_source.empty()) {
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
    }
}

static bool is_identifier(char ch) {
    return (ch >= '!' && ch <= '&' && ch != '"') || (ch >= '*' && ch <= 'Z' && ch != ';') ||
           (ch >= '^' && ch <= 'z' && ch != '`');
}

Token Lexer::next_token() {
    char ch;
    while (is_space(ch = m_source[m_head++])) {
        if (ch == '\n') {
            m_line++;
        }
    }

    const auto position = m_head - 1;

    // Handle special case of negative numbers first since - is a valid identifier character.
    if (ch == '-' && is_digit(m_source[m_head])) {
        auto number = parse_number(m_source[m_head++]);
        if (const auto decimal = number.try_get<double>()) {
            return MAKE_TOKEN(-*decimal);
        }
        return MAKE_TOKEN(-static_cast<int64_t>(number.get<uint64_t>()));
    }

    if (is_digit(ch)) {
        auto number = parse_number(ch);
        if (const auto decimal = number.try_get<double>()) {
            return MAKE_TOKEN(*decimal);
        }
        return MAKE_TOKEN(static_cast<int64_t>(number.get<uint64_t>()));
    }

    if (is_identifier(ch)) {
        while (is_identifier(m_source[m_head])) {
            m_head++;
        }
        auto string = m_source.view().substr(position, m_head - position);
        return MAKE_TOKEN(TokenKind::Identifier, string);
    }

    switch (ch) {
    case '\0':
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
        return MAKE_TOKEN(TokenKind::Eof);
    case ';':
        while (m_source[m_head] != '\n' && m_source[m_head] != '\0') {
            m_head++;
        }
        return next_token();
    case '"': {
        while (m_source[m_head] != '"' && m_source[m_head] != '\0') {
            m_head++;
        }
        if (m_source[m_head] == '\0') {
            return MAKE_TOKEN(TokenKind::Invalid);
        }
        m_head++;
        auto string = m_source.view().substr(position + 1, m_head - position - 2);
        return MAKE_TOKEN(TokenKind::String, string);
    }
    case '(':
    case '[':
        return MAKE_TOKEN(TokenKind::ListBegin);
    case ')':
    case ']':
        return MAKE_TOKEN(TokenKind::ListEnd);
    case '\'':
        return MAKE_TOKEN(TokenKind::Quote);
    default:
        return MAKE_TOKEN(TokenKind::Invalid);
    }
}

SourcePosition Lexer::recover_position(const Token &token) const {
    // Backtrack to find line start.
    uint32_t line_head = token.position();
    while (line_head > 0 && m_source[line_head - 1] != '\n') {
        line_head--;
    }

    // Advance to find line end.
    uint32_t line_end = token.position();
    while (line_end < m_source.length() && m_source[line_end] != '\n') {
        line_end++;
    }

    const auto line_view = m_source.view().substr(line_head, line_end - line_head);
    return {m_file_name, line_view, token.line(), token.position() - line_head + 1};
}

double Token::decimal() const {
    VULL_ASSERT(m_kind == TokenKind::Decimal);
    return m_number_data.decimal_data;
}

int64_t Token::integer() const {
    VULL_ASSERT(m_kind == TokenKind::Integer);
    return m_number_data.integer_data;
}

StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::Identifier || m_kind == TokenKind::String);
    return {static_cast<const char *>(m_ptr_data), m_number_data.unsigned_data};
}

String Token::kind_string(TokenKind kind) {
    switch (kind) {
    case TokenKind::Invalid:
        return "<invalid>";
    case TokenKind::Eof:
        return "<eof>";
    case TokenKind::Identifier:
        return "identifier";
    case TokenKind::Decimal:
        return "float literal";
    case TokenKind::Integer:
        return "integer literal";
    case TokenKind::String:
        return "string literal";
    case TokenKind::ListBegin:
        return "'('";
    case TokenKind::ListEnd:
        return "')'";
    case TokenKind::Quote:
        return "'";
    default:
        vull::unreachable();
    }
}

String Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Identifier:
    case TokenKind::String:
        return vull::format("'{}'", string());
    case TokenKind::Decimal:
        return vull::format("'{}'", decimal());
    case TokenKind::Integer:
        return vull::format("'{}'", integer());
    default:
        return kind_string(m_kind);
    }
}

} // namespace vull::script
