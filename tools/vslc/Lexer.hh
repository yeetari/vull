#pragma once

#include <vull/support/Utility.hh>

#include "CharStream.hh"
#include "Token.hh"

class Lexer {
    CharStream m_stream;
    Token m_peek_token{TokenKind::Eof};
    bool m_peek_ready{false};

    Token next_token();

public:
    explicit Lexer(CharStream &&stream) : m_stream(vull::move(stream)) {}
    Lexer(const Lexer &) = delete;
    Lexer(Lexer &&) = delete;
    ~Lexer() = default;

    Lexer &operator=(const Lexer &) = delete;
    Lexer &operator=(Lexer &&) = delete;

    const Token &peek();
    Token next();
};
