#include "ast.hh"
#include "char_stream.hh"
#include "legaliser.hh"
#include "lexer.hh"
#include "parser.hh"
#include "spv/backend.hh"
#include "spv/builder.hh"
#include "spv/spirv.hh"

#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/support/function.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

#include <stdio.h>

static void print_usage(const char *executable) {
    fprintf(stderr, "usage: %s <input> <output>\n", executable);
}

int main(int argc, char **argv) {
    const char *input_path = nullptr;
    const char *output_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (input_path == nullptr) {
            input_path = argv[i];
        } else if (output_path == nullptr) {
            output_path = argv[i];
        } else {
            fprintf(stderr, "Invalid argument %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_path == nullptr || output_path == nullptr) {
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

    auto output_file = VULL_EXPECT(vull::open_file(
        output_path, vull::OpenModes(vull::OpenMode::Create, vull::OpenMode::Truncate, vull::OpenMode::Write)));
    auto output_stream = output_file.create_stream();
    backend.builder().write([&](spv::Word word) {
        VULL_EXPECT(output_stream.write_le(word));
    });
}
