#include <vull/script/lexer.hh>

#include <vull/script/token.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::script {

Lexer::Lexer(String file_name, String source) : m_file_name(vull::move(file_name)), m_source(vull::move(source)) {
    if (m_source.empty()) {
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
    }
}

Token Lexer::next_token() {
    char ch;
    while (is_space(ch = m_source[m_head++])) {
        if (ch == '\n') {
            m_line++;
        }
    }

    const auto position = m_head - 1;
    if (is_digit(ch)) {
        auto number = parse_number(ch);
        if (const auto decimal = number.try_get<double>()) {
            return {*decimal, position, m_line};
        }
        return {static_cast<double>(number.get<uint64_t>()), position, m_line};
    }
    if (is_ident(ch)) {
        while (is_ident(m_source[m_head]) || is_digit(m_source[m_head])) {
            m_head++;
        }
        auto string = m_source.view().substr(position, m_head);
        // TODO: Perfect hashing (http://0x80.pl/notesen/2023-04-30-lookup-in-strings.html) or trie switch.
        if (string == "elif") {
            return {TokenKind::KW_elif, position, m_line};
        }
        if (string == "else") {
            return {TokenKind::KW_else, position, m_line};
        }
        if (string == "end") {
            return {TokenKind::KW_end, position, m_line};
        }
        if (string == "function") {
            return {TokenKind::KW_function, position, m_line};
        }
        if (string == "if") {
            return {TokenKind::KW_if, position, m_line};
        }
        if (string == "let") {
            return {TokenKind::KW_let, position, m_line};
        }
        if (string == "return") {
            return {TokenKind::KW_return, position, m_line};
        }
        return {TokenKind::Identifier, string, position, m_line};
    }

    switch (ch) {
    case 0:
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
        return {TokenKind::Eof, position, m_line};
    case '=':
        if (m_source[m_head] == '=') {
            m_head++;
            return {TokenKind::EqualEqual, position, m_line};
        }
        return {'='_tk, position, m_line};
    case '!':
        if (m_source[m_head] == '=') {
            m_head++;
            return {TokenKind::NotEqual, position, m_line};
        }
        return {'!'_tk, position, m_line};
    case '<':
        if (m_source[m_head] == '=') {
            m_head++;
            return {TokenKind::LessEqual, position, m_line};
        }
        return {'<'_tk, position, m_line};
    case '>':
        if (m_source[m_head] == '=') {
            m_head++;
            return {TokenKind::GreaterEqual, position, m_line};
        }
        return {'>'_tk, position, m_line};
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
            return {TokenKind::Invalid, position, m_line};
        }
        return {static_cast<TokenKind>(ch), position, m_line};
    }
}

SourcePosition Lexer::recover_position(const Token &token) const {
    // Backtrack to find line start.
    uint32_t line_head = token.position();
    while (line_head > 1 && m_source[line_head - 1] != '\n') {
        line_head--;
    }

    // Advance to find line end.
    uint32_t line_end = token.position();
    while (line_end < m_source.length() && m_source[line_end] != '\n') {
        line_end++;
    }

    const auto line_view = m_source.view().substr(line_head, line_end);
    return {m_file_name, line_view, token.line(), token.position() - line_head + 1};
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
    case TokenKind::EqualEqual:
        return "'=='";
    case TokenKind::NotEqual:
        return "'!='";
    case TokenKind::LessEqual:
        return "'<='";
    case TokenKind::GreaterEqual:
        return "'>='";
    case TokenKind::KW_elif:
        return "'elif'";
    case TokenKind::KW_else:
        return "'else'";
    case TokenKind::KW_end:
        return "'end'";
    case TokenKind::KW_function:
        return "'function'";
    case TokenKind::KW_if:
        return "'if'";
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
