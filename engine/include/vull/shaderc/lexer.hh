#pragma once

#include <vull/shaderc/token.hh>
#include <vull/support/lexer_base.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>

namespace vull::shaderc {

class SourceLocation;

struct SourceInfo {
    StringView file_name;
    StringView line_source;
    uint32_t line;
    uint32_t column;
};

class Lexer : public LexerBase<Lexer, Token> {
    friend LexerBase<Lexer, Token>;

private:
    String m_file_name;
    String m_source;
    uint32_t m_head{0};
    uint16_t m_line{1};

    uint32_t m_last_head{0};
    uint16_t m_last_line{0};

    static bool is_eof(const Token &token) { return token.kind() == TokenKind::Eof; }
    void skip_char() { m_head++; }
    void unskip_char() { m_head--; }
    char peek_char() { return m_source[m_head]; }
    char next_char() { return m_source[m_head++]; }
    Token next_token(bool in_comment);
    Token next_token();

public:
    Lexer(String file_name, String source);

    Token cursor_token() const;
    SourceInfo recover_info(SourceLocation location) const;
};

} // namespace vull::shaderc
