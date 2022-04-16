#include "CharStream.hh"
#include "Lexer.hh"
#include "Parser.hh"
#include "spv/Backend.hh"
#include "spv/Builder.hh"

static void print_usage(const char *executable) {
    fprintf(stderr, "usage: %s [--format] <input>\n", executable);
}

int main(int argc, char **argv) {
    bool format = false;
    const char *input_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--format") == 0) {
            format = true;
        } else if (input_path == nullptr) {
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

    if (format) {
        ast::Formatter formatter;
        ast.traverse(formatter);
        return 0;
    }

    spv::Backend backend;
    ast.traverse(backend);
    backend.builder().write([](spv::Word word) {
        fwrite(&word, sizeof(spv::Word), 1, stdout);
    });
}
