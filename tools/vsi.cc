#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/timer.hh>
#include <vull/script/lexer.hh>
#include <vull/script/parser.hh>
#include <vull/script/value.hh>
#include <vull/script/vm.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>
#include <stdlib.h>

using namespace vull;

static void print_message(script::Lexer &lexer, const script::ParseMessage &message) {
    StringView kind_string = message.kind() == script::ParseMessage::Kind::Error ? "\x1b[1;91merror" : "\x1b[1;35mnote";
    const auto [file_name, line_source, line, column] = lexer.recover_position(message.token());
    vull::println("\x1b[1;37m{}:{}:{}: {}: \x1b[1;37m{}\x1b[0m", file_name, line, column, kind_string, message.text());
    if (message.kind() == script::ParseMessage::Kind::Note) {
        return;
    }
    vull::print(" { 4 } | {}\n      |", line, line_source);
    for (uint32_t i = 0; i < column; i++) {
        vull::print(" ");
    }
    vull::println("\x1b[1;92m^\x1b[0m");
}

int main(int argc, char **argv) {
    vull::open_log();
    vull::set_log_colours_enabled(true);

    String script_path;

    ArgsParser args_parser("vsi", "Vull Script Interpreter", "0.1.0");
    args_parser.add_argument(script_path, "script", true);
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    platform::Timer timer;
    auto source_or_error = platform::read_entire_file_ascii(script_path);
    if (source_or_error.is_error()) {
        vull::println("vsi: '{}': {}", script_path, platform::file_error_string(source_or_error.error()));
        return EXIT_FAILURE;
    }

    script::Vm vm;
    script::Lexer lexer(script_path, source_or_error.disown_value());
    script::Parser parser(vm, lexer);
    auto parse_result = parser.parse();
    if (parse_result.is_error()) {
        const auto &error = parse_result.error();
        for (const auto &message : error.messages()) {
            print_message(lexer, message);
        }
        return EXIT_FAILURE;
    }

    auto program = parse_result.disown_value();
    auto result = vm.evaluate(program);

    StringBuilder result_sb;
    result.format_into(result_sb);
    vull::println("Returned {} in {} ms", result_sb.build(), timer.elapsed() * 1000.0f);
}
