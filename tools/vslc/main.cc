#include "CharStream.hh"
#include "Lexer.hh"
#include "spv/Builder.hh"

int main(int, char **argv) {
    CharStream char_stream(argv[1]);
    Lexer lexer(vull::move(char_stream));
    for (auto token = lexer.next(); token.kind() != TokenKind::Eof; token = lexer.next()) {
        printf("%s (%s)\n", token.to_string().data(), Token::kind_string(token.kind()).data());
    }

    auto *file = fopen("out.spv", "wb");
    spv::Builder builder;
    builder.write([file](spv::Word word) {
        fwrite(&word, sizeof(spv::Word), 1, file);
    });
    fclose(file);
}
