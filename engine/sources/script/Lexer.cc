#include <vull/script/Lexer.hh>

#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Format.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stdint.h>

namespace vull::script {

Token Lexer::next_token() {
    while (is_space(m_source[m_head])) {
        m_head++;
    }

    const auto position = m_head;
    char ch = m_source[m_head++];
    if (is_digit(ch)) {
        return {parse_double(ch), position};
    }
    if (is_ident(ch)) {
        while (is_ident(m_source[m_head]) || is_digit(m_source[m_head])) {
            m_head++;
        }
        auto string = m_source.view().substr(position, m_head);
        if (string == "function") {
            return {TokenKind::KW_function, position};
        }
        if (string == "let") {
            return {TokenKind::KW_let, position};
        }
        if (string == "return") {
            return {TokenKind::KW_return, position};
        }
        return {TokenKind::Identifier, string, position};
    }

    switch (ch) {
    case 0:
        return {TokenKind::Eof, position};
    case '/':
        // Handle comments.
        if (m_source[m_head] == '/') {
            while (m_source[m_head] != '\n' && m_source[m_head] != '\0') {
                m_head++;
            }
            return next_token();
        }
        [[fallthrough]];
    default:
        if (ch <= 31) {
            return {TokenKind::Invalid, position};
        }
        return {static_cast<TokenKind>(ch), position};
    }
}

SourcePosition Lexer::recover_position(const Token &token) const {
    uint32_t line = 1;
    uint32_t column = 1;
    uint32_t line_head = 0;
    for (uint32_t head = 0; head < token.position(); head++) {
        const char ch = m_source[head];
        column++;
        if (ch == '\n') {
            line++;
            column = 1;
            line_head = head + 1;
        }
    }
    uint32_t line_end = line_head;
    for (; m_source[line_end] != '\0' && m_source[line_end] != '\n'; line_end++) {
    }
    StringView line_view(reinterpret_cast<const char *>(&m_source[line_head]), line_end - line_head);
    return {m_file_name, line_view, line, column};
}

double Token::number() const {
    VULL_ASSERT(m_kind == TokenKind::Number);
    return m_number_data.decimal_data;
}

StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::Identifier);
    return {static_cast<const char *>(m_ptr_data), m_number_data.integer_data};
}

String Token::kind_string(TokenKind kind) {
    if (auto value = vull::to_underlying(kind); value < 256) {
        String string("'x'");
        string.data()[1] = static_cast<char>(value);
        return string;
    }
    switch (kind) {
    case TokenKind::Invalid:
        return "<invalid>";
    case TokenKind::Eof:
        return "<eof>";
    case TokenKind::Identifier:
        return "identifier";
    case TokenKind::Number:
        return "number literal";
    case TokenKind::KW_function:
        return "'function'";
    case TokenKind::KW_let:
        return "'let'";
    case TokenKind::KW_return:
        return "'return'";
#if defined(__GNUC__) && !defined(__clang__)
    default:
        vull::unreachable();
#endif
    }
}

String Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Identifier:
        return vull::format("'{}'", string());
    case TokenKind::Number:
        return vull::format("{}", number());
    default:
        return kind_string(m_kind);
    }
}

} // namespace vull::script
