#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/parser.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>
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
    String source_path;

    ArgsParser args_parser("vslc", "Vull Shader Compiler", "0.1.0");
    args_parser.add_flag(dump_ast, "Dump AST", "dump-ast");
    args_parser.add_argument(source_path, "input-vsl", true);
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    Vector<uint8_t> source_bytes;
    if (auto result = vull::read_entire_file(source_path, source_bytes); result.is_error()) {
        vull::println("vslc: '{}': {}", source_path, vull::file_error_string(result.error()));
        return EXIT_FAILURE;
    }

    // Interpret raw bytes directly as ASCII.
    auto source_bytes_span = source_bytes.take_all();
    auto source = String::move_raw(vull::bit_cast<char *>(source_bytes_span.data()), source_bytes_span.size());

    shaderc::Lexer lexer(source_path, source);
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