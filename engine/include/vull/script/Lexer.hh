#pragma once

#include <vull/script/Token.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>

namespace vull::script {

struct SourcePosition {
    StringView file_name;
    StringView line_source;
    uint32_t line;
    uint32_t column;
};

class Lexer {
    String m_file_name;
    UniquePtr<Stream> m_stream;
    Optional<Token> m_peek_token;
    Vector<uint8_t> m_data;
    uint32_t m_head{0};

    double parse_number(uint8_t);
    Token next_token();

public:
    Lexer(String file_name, UniquePtr<Stream> &&stream);

    const Token &peek();
    Token next();
    SourcePosition recover_position(const Token &token) const;
};

} // namespace vull::script
