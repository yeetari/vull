#pragma once

#include <vull/support/lexer_base.hh>
#include <vull/support/utility.hh>

#include "char_stream.hh"
#include "token.hh"

class Lexer : public vull::LexerBase<Lexer, Token> {
    friend LexerBase<Lexer, Token>;

private:
    CharStream m_stream;

    static bool is_eof(const Token &token) { return token.kind() == TokenKind::Eof; }
    Token next_token();

public:
    explicit Lexer(CharStream &&stream) : m_stream(vull::move(stream)) {}
    Lexer(const Lexer &) = delete;
    Lexer(Lexer &&) = delete;
    ~Lexer() = default;

    Lexer &operator=(const Lexer &) = delete;
    Lexer &operator=(Lexer &&) = delete;
};
