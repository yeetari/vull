#include "CharStream.hh"
#include "Lexer.hh"

int main(int, char **argv) {
    CharStream char_stream(argv[1]);
    Lexer lexer(vull::move(char_stream));
    for (auto token = lexer.next(); token.kind() != TokenKind::Eof; token = lexer.next()) {
        printf("%s (%s)\n", token.to_string().data(), Token::kind_string(token.kind()).data());
    }
}
