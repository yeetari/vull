#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/error.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/parser.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>
#include <stdlib.h>

using namespace vull;

static void print_message(shaderc::Lexer &lexer, const shaderc::ErrorMessage &message) {
    StringView kind_string =
        message.kind() == shaderc::ErrorMessage::Kind::Error ? "\x1b[1;91merror" : "\x1b[1;35mnote";
    const auto [file_name, line_source, line, column] = lexer.recover_info(message.source_location());
    vull::println("\x1b[1;97m{}:{}:{}: {}: \x1b[1;97m{}\x1b[0m", file_name, line, column, kind_string, message.text());
    if (message.kind() == shaderc::ErrorMessage::Kind::NoteNoLine) {
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
    String source_path;

    ArgsParser args_parser("vslc", "Vull Shader Compiler", "0.1.0");
    args_parser.add_flag(dump_ast, "Dump AST", "dump-ast");
    args_parser.add_argument(source_path, "input-vsl", true);
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    auto source_or_error = platform::read_entire_file_ascii(source_path);
    if (source_or_error.is_error()) {
        vull::println("vslc: '{}': {}", source_path, platform::file_error_string(source_or_error.error()));
        return EXIT_FAILURE;
    }

    shaderc::Lexer lexer(source_path, source_or_error.disown_value());
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
}
