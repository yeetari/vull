#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/shaderc/hir.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/parser.hh>
#include <vull/shaderc/spv_builder.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>

#include <stdlib.h>

using namespace vull;

static void print_message(shaderc::Lexer &lexer, const shaderc::ParseMessage &message) {
    StringView kind_string =
        message.kind() == shaderc::ParseMessage::Kind::Error ? "\x1b[1;91merror" : "\x1b[1;35mnote";
    const auto [file_name, line_source, line, column] = lexer.recover_position(message.token());
    vull::println("\x1b[1;37m{}:{}:{}: {}: \x1b[1;37m{}\x1b[0m", file_name, line, column, kind_string, message.text());
    if (message.kind() == shaderc::ParseMessage::Kind::NoteNoLine) {
        return;
    }
    vull::print(" { 4 } | {}\n      |", line, line_source);
    for (uint32_t i = 0; i < column; i++) {
        vull::print(" ");
    }
    vull::println("\x1b[1;92m^\x1b[0m");
}

int main(int argc, char **argv) {
    bool dump_ast = false;
    String input_path;
    String output_path;

    ArgsParser args_parser("vslc", "Vull Shader Compiler", "0.1.0");
    args_parser.add_flag(dump_ast, "Dump AST", "dump-ast");
    args_parser.add_argument(input_path, "input-vsl", true);
    args_parser.add_option(output_path, "Output SPIR-V path", "output", 'o');
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    auto file = VULL_EXPECT(vull::open_file(input_path, OpenMode::Read));
    auto stream = file.create_stream();
    StringBuilder sb;
    while (true) {
        Array<char, 16384> data;
        auto bytes_read = static_cast<uint32_t>(VULL_EXPECT(stream.read(data.span())));
        if (bytes_read == 0) {
            break;
        }
        sb.extend(data.span().subspan(0, bytes_read));
    }

    shaderc::Lexer lexer(input_path, sb.build());
    shaderc::Parser parser(lexer);
    auto ast_or_error = parser.parse();
    if (ast_or_error.is_error()) {
        const auto &error = ast_or_error.error();
        for (const auto &message : error.messages()) {
            print_message(lexer, message);
        }
        return EXIT_FAILURE;
    }

    auto ast = ast_or_error.disown_value();
    if (dump_ast) {
        shaderc::ast::Dumper dumper;
        ast.traverse(dumper);
        return EXIT_SUCCESS;
    }

    shaderc::spv::Builder builder;
    vull::println("a={} b={} c={} d={}", builder.float_type(32), builder.float_type(33), builder.float_type(32),
                  builder.float_type(64));

    return EXIT_SUCCESS;
}
