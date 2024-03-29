#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/platform/timer.hh>
#include <vull/script/bytecode.hh>
#include <vull/script/constant_pool.hh>
#include <vull/script/lexer.hh>
#include <vull/script/parser.hh>
#include <vull/script/value.hh>
#include <vull/script/vm.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <stdint.h>
#include <stdlib.h>

using namespace vull;

static void print_message(script::Lexer &lexer, const script::ParseMessage &message) {
    StringView kind_string = message.kind() == script::ParseMessage::Kind::Error ? "\x1b[1;91merror" : "\x1b[1;35mnote";
    const auto [file_name, line_source, line, column] = lexer.recover_position(message.token());
    vull::println("\x1b[1;37m{}:{}:{}: {}: \x1b[1;37m{}\x1b[0m", file_name, line, column, kind_string,
                  message.message());
    vull::print(" { 4 } | {}\n      |", line, line_source);
    for (uint32_t i = 0; i < column; i++) {
        vull::print(" ");
    }
    vull::println("\x1b[1;92m^\x1b[0m");
}

int main(int argc, char **argv) {
    Vector<StringView> args(argv, argv + argc);
    if (args.size() < 2) {
        vull::println("usage: {} [--dump-bc] <script>", args[0]);
        return EXIT_SUCCESS;
    }

    StringView script_path;
    bool dump_bytecode = false;
    for (const auto &arg : vull::slice(args, 1u)) {
        if (arg == "--dump-bc") {
            dump_bytecode = true;
        } else if (arg[0] == '-') {
            vull::println("fatal: unknown option {}", arg);
            return EXIT_FAILURE;
        } else if (script_path.empty()) {
            script_path = arg;
        } else {
            vull::println("fatal: unexpected argument {}", arg);
            return EXIT_FAILURE;
        }
    }

    Timer timer;
    auto file = VULL_EXPECT(vull::open_file(script_path, OpenMode::Read));
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

    script::ConstantPool constant_pool;
    script::Lexer lexer(script_path, sb.build());
    script::Parser parser(lexer, constant_pool);
    auto frame_or_error = parser.parse();
    if (frame_or_error.is_error()) {
        const auto &error = frame_or_error.error();
        for (const auto &message : error.messages()) {
            print_message(lexer, message);
        }
        return EXIT_FAILURE;
    }

    auto frame = frame_or_error.disown_value();
    script::Vm vm(vull::move(constant_pool));
    if (dump_bytecode) {
        vm.dump_frame(*frame);
    }
    script::Value ret = vm.exec_frame(*frame);
    vull::println("Returned {} in {} ms", ret.number, timer.elapsed() * 1000.0f);
}
