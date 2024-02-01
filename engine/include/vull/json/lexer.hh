#pragma once

#include <vull/json/token.hh>
#include <vull/support/lexer_base.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>

namespace vull::json {

class Lexer : public LexerBase<Lexer, Token> {
    friend LexerBase<Lexer, Token>;

private:
    StringView m_source;
    uint32_t m_head{0};

    static bool is_eof(const Token &token) { return token.kind() == TokenKind::Eof; }
    void skip_char() { m_head++; }
    void unskip_char() { m_head--; }
    char peek_char() { return m_source[m_head]; }
    char next_char() { return m_source[m_head++]; }
    Token next_token();

public:
    explicit Lexer(StringView source) : m_source(source) {}
};

} // namespace vull::json
