#include <vull/script/Lexer.hh>

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Format.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull::script {

Lexer::Lexer(String file_name, UniquePtr<Stream> &&stream)
    : m_file_name(vull::move(file_name)), m_stream(vull::move(stream)) {
    while (true) {
        Array<uint8_t, 16384> data;
        auto bytes_read = static_cast<uint32_t>(VULL_EXPECT(m_stream->read(data.span())));
        if (bytes_read == 0) {
            break;
        }
        m_data.extend(Span<uint8_t>{data.data(), bytes_read});
    }
    m_data.push(0);
}

Token Lexer::next_token() {
    while (is_space(m_data[m_head])) {
        m_head++;
    }

    const auto position = m_head;
    uint8_t ch = m_data[m_head++];
    if (is_digit(ch)) {
        return {parse_double(ch), position};
    }
    if (is_ident(ch)) {
        const auto cur_head = m_head;
        while (is_ident(m_data[m_head]) || is_digit(m_data[m_head])) {
            m_head++;
        }
        const auto length = m_head - cur_head + 1;
        StringView string(reinterpret_cast<const char *>(&m_data[m_head] - length), length);
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
        if (m_data[m_head] == '/') {
            while (m_data[m_head] != '\n') {
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
        const uint8_t ch = m_data[head];
        column++;
        if (ch == '\n') {
            line++;
            column = 1;
            line_head = head + 1;
        }
    }
    uint32_t line_end = line_head;
    for (; m_data[line_end] != '\0' && m_data[line_end] != '\n'; line_end++) {
    }
    StringView line_view(reinterpret_cast<const char *>(&m_data[line_head]), line_end - line_head);
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
