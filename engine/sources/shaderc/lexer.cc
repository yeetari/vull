#include <vull/shaderc/lexer.hh>

#include <vull/shaderc/source_location.hh>
#include <vull/shaderc/token.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stddef.h>

// TODO: Deduplicate functionality between this lexer and the script lexer.

#define MAKE_TOKEN(...) {__VA_ARGS__, position, m_line}

namespace vull::shaderc {

Lexer::Lexer(String file_name, String source) : m_file_name(vull::move(file_name)), m_source(vull::move(source)) {
    if (m_source.empty()) {
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
    }
}

Token Lexer::next_token(bool in_comment) {
    if (!in_comment) {
        m_last_head = m_head;
        m_last_line = m_line;
    }

    char ch;
    while (is_space(ch = m_source[m_head++])) {
        if (ch == '\n') {
            m_line++;
        }
    }

    auto consume = [this](char ch) {
        if (m_source[m_head] == ch) {
            m_head++;
            return true;
        }
        return false;
    };

    const auto position = m_head - 1;
    if (is_digit(ch)) {
        auto number = parse_number(ch);
        if (const auto decimal = number.try_get<double>()) {
            consume('f');
            return MAKE_TOKEN(static_cast<float>(*decimal));
        }
        return MAKE_TOKEN(number.get<uint64_t>());
    }

    if (is_ident(ch)) {
        while (is_ident(m_source[m_head]) || is_digit(m_source[m_head])) {
            m_head++;
        }
        auto string = m_source.view().substr(position, m_head - position);
        if (string == "fn") {
            return MAKE_TOKEN(TokenKind::KW_fn);
        }
        if (string == "let") {
            return MAKE_TOKEN(TokenKind::KW_let);
        }
        if (string == "pipeline") {
            return MAKE_TOKEN(TokenKind::KW_pipeline);
        }
        if (string == "return") {
            return MAKE_TOKEN(TokenKind::KW_return);
        }
        if (string == "uniform") {
            return MAKE_TOKEN(TokenKind::KW_uniform);
        }
        if (string == "var") {
            return MAKE_TOKEN(TokenKind::KW_var);
        }
        return MAKE_TOKEN(TokenKind::Identifier, string);
    }

    if (ch == '\0') {
        m_peek_token.emplace(TokenKind::Eof, 0u, uint16_t(0));
        return MAKE_TOKEN(TokenKind::Eof);
    }

    if (ch == '/' && consume('/')) {
        while (m_source[m_head] != '\n' && m_source[m_head] != '\0') {
            m_head++;
        }
        return next_token(true);
    }

    if (ch == '"') {
        while (m_source[m_head] != '"' && m_source[m_head] != '\0') {
            m_head++;
        }
        if (m_source[m_head] == '\0') {
            return MAKE_TOKEN(TokenKind::Invalid);
        }
        m_head++;
        auto string = m_source.view().substr(position + 1, m_head - position - 2);
        return MAKE_TOKEN(TokenKind::StringLit, string);
    }

    if (ch == '+' && consume('=')) {
        return MAKE_TOKEN(TokenKind::PlusEqual);
    }
    if (ch == '-' && consume('=')) {
        return MAKE_TOKEN(TokenKind::MinusEqual);
    }
    if (ch == '*' && consume('=')) {
        return MAKE_TOKEN(TokenKind::AsteriskEqual);
    }
    if (ch == '/' && consume('=')) {
        return MAKE_TOKEN(TokenKind::SlashEqual);
    }
    if (ch == '[' && consume('[')) {
        return MAKE_TOKEN(TokenKind::DoubleOpenSquareBrackets);
    }
    if (ch == ']' && consume(']')) {
        return MAKE_TOKEN(TokenKind::DoubleCloseSquareBrackets);
    }

    if (ch <= 31) {
        return MAKE_TOKEN(TokenKind::Invalid);
    }
    return MAKE_TOKEN(static_cast<TokenKind>(ch));
}

Token Lexer::next_token() {
    return next_token(false);
}

Token Lexer::cursor_token() const {
    return {TokenKind::Cursor, m_last_head, m_last_line};
}

SourceInfo Lexer::recover_info(SourceLocation location) const {
    // Backtrack to find line start.
    uint32_t line_head = location.byte_offset();
    while (line_head > 0 && m_source[line_head - 1] != '\n') {
        line_head--;
    }

    // Advance to find line end.
    uint32_t line_end = location.byte_offset();
    while (line_end < m_source.length() && m_source[line_end] != '\n') {
        line_end++;
    }

    const auto line_view = m_source.view().substr(line_head, line_end - line_head);
    return {m_file_name, line_view, location.line(), location.byte_offset() - line_head + 1};
}

float Token::decimal() const {
    VULL_ASSERT(m_kind == TokenKind::FloatLit);
    return m_number_data.float_data;
}

size_t Token::integer() const {
    VULL_ASSERT(m_kind == TokenKind::IntLit);
    return m_number_data.int_data;
}

StringView Token::string() const {
    VULL_ASSERT(m_kind == TokenKind::Identifier || m_kind == TokenKind::StringLit);
    return {static_cast<const char *>(m_ptr_data), m_number_data.int_data};
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
    case TokenKind::FloatLit:
        return "float literal";
    case TokenKind::IntLit:
        return "integer literal";
    case TokenKind::StringLit:
        return "string literal";
    case TokenKind::PlusEqual:
        return "'+='";
    case TokenKind::MinusEqual:
        return "'-='";
    case TokenKind::AsteriskEqual:
        return "'*='";
    case TokenKind::SlashEqual:
        return "'/='";
    case TokenKind::DoubleOpenSquareBrackets:
        return "[[";
    case TokenKind::DoubleCloseSquareBrackets:
        return "]]";
    case TokenKind::KW_fn:
        return "'fn'";
    case TokenKind::KW_let:
        return "'let'";
    case TokenKind::KW_pipeline:
        return "'pipeline'";
    case TokenKind::KW_return:
        return "'return'";
    case TokenKind::KW_uniform:
        return "'uniform'";
    case TokenKind::KW_var:
        return "'var'";
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

String Token::to_string() const {
    switch (m_kind) {
    case TokenKind::Identifier:
        return vull::format("'{}'", string());
    case TokenKind::FloatLit:
        return vull::format("'{}f'", decimal());
    case TokenKind::IntLit:
        return vull::format("'{}u'", integer());
    case TokenKind::StringLit:
        return vull::format("\"{}\"", string());
    default:
        return kind_string(m_kind);
    }
}

} // namespace vull::shaderc
