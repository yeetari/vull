#pragma once

#include <vull/script/Token.hh>
#include <vull/support/LexerBase.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

#include <stdint.h>

namespace vull::script {

struct SourcePosition {
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

    static bool is_eof(const Token &token) { return token.kind() == TokenKind::Eof; }
    void skip_char() { m_head++; }
    void unskip_char() { m_head--; }
    char peek_char() { return m_source[m_head]; }
    char next_char() { return m_source[m_head++]; }
    Token next_token();

public:
    Lexer(String file_name, String source);

    SourcePosition recover_position(const Token &token) const;
};

} // namespace vull::script
