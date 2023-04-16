#pragma once

#include <vull/json/Token.hh>
#include <vull/support/LexerBase.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>

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
