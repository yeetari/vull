#include "Ast.hh"
#include "CharStream.hh"
#include "Legaliser.hh"
#include "Lexer.hh"
#include "Parser.hh"
#include "spv/Backend.hh"
#include "spv/Builder.hh"
#include "spv/Spirv.hh"

#include <vull/support/Utility.hh>

#include <stdio.h>

static void print_usage(const char *executable) {
    fprintf(stderr, "usage: %s [--format] <input>\n", executable);
}

int main(int argc, char **argv) {
    const char *input_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (input_path == nullptr) {
            input_path = argv[i];
        } else {
            fprintf(stderr, "Invalid argument %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_path == nullptr) {
        print_usage(argv[0]);
        return 1;
    }

    CharStream char_stream(input_path);
    Lexer lexer(vull::move(char_stream));
    Parser parser(lexer);
    auto ast = parser.parse();

    Legaliser legaliser;
    ast.traverse(legaliser);

    spv::Backend backend;
    ast.traverse(backend);
    backend.builder().write([](spv::Word word) {
        fwrite(&word, sizeof(spv::Word), 1, stdout);
    });
}
