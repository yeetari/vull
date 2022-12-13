#include <vull/core/Log.hh>
#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/platform/Timer.hh>
#include <vull/script/ConstantPool.hh>
#include <vull/script/Lexer.hh>
#include <vull/script/Parser.hh>
#include <vull/script/Value.hh>
#include <vull/script/Vm.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include <stdlib.h>

using namespace vull;

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
    script::ConstantPool constant_pool;
    script::Lexer lexer(script_path, vull::adopt_unique(file.create_stream()));
    script::Parser parser(lexer, constant_pool);
    auto frame = parser.parse();
    if (auto count = parser.error_count()) {
        vull::println("{} error{} generated", count, count != 1 ? "s" : "");
        return EXIT_FAILURE;
    }

    script::Vm vm(vull::move(constant_pool));
    if (dump_bytecode) {
        vm.dump_frame(*frame);
    }
    script::Value ret = vm.exec_frame(*frame);
    vull::println("Returned {} in {} ms", ret.number, timer.elapsed() * 1000.0f);
}
