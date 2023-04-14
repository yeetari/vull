#pragma once

#include <vull/container/Vector.hh>
#include <vull/script/Token.hh>
#include <vull/support/LexerBase.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>

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
    UniquePtr<Stream> m_stream;
    Vector<uint8_t> m_data;
    uint32_t m_head{0};

    static bool is_eof(const Token &token) { return token.kind() == TokenKind::Eof; }
    void skip_char() { m_head++; }
    void unskip_char() { m_head--; }
    uint8_t peek_char() { return m_data[m_head]; }
    uint8_t next_char() { return m_data[m_head++]; }
    Token next_token();

public:
    Lexer(String file_name, UniquePtr<Stream> &&stream);

    SourcePosition recover_position(const Token &token) const;
};

} // namespace vull::script
