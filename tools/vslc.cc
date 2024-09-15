#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/legaliser.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/parser.hh>
#include <vull/shaderc/spv_builder.hh>
#include <vull/shaderc/spv_backend.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

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

    auto source_or_error = vull::read_entire_file_ascii(source_path);
    if (source_or_error.is_error()) {
        vull::println("vslc: '{}': {}", source_path, vull::file_error_string(source_or_error.error()));
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

    shaderc::hir::Root root;
    shaderc::Legaliser legaliser(root);
    VULL_EXPECT(legaliser.legalise(ast));

    auto output_file = VULL_EXPECT(
        vull::open_file("/dev/stdout", vull::OpenMode::Create | vull::OpenMode::Truncate | vull::OpenMode::Write));
    auto output_stream = output_file.create_stream();
    shaderc::spv::Builder builder;

    shaderc::spv::build_spv(builder, root);

    // builder.ensure_capability(vk::spv::Capability::Shader);

    // auto return_type = builder.void_type();
    // auto &function = builder.append_function(return_type, builder.function_type(return_type, {}));
    // auto &entry_point = builder.append_entry_point("vertex_main", function, vk::spv::ExecutionModel::Vertex);

    // const auto position_type = builder.vector_type(builder.float_type(32), 4);
    // auto &position_variable = entry_point.append_variable(position_type, vk::spv::StorageClass::Output);
    // builder.decorate(position_variable.id(), vk::spv::Decoration::BuiltIn, vk::spv::BuiltIn::Position);

    // const auto zero = builder.scalar_constant(builder.float_type(32), vull::bit_cast<uint32_t>(0.0f));
    // Vector<vk::spv::Id> constant_inits(4, zero);
    // const auto constant = builder.composite_constant(position_type, vull::move(constant_inits));

    // auto &block = function.append_block();
    // auto &store = block.append(vk::spv::Op::Store);
    // store.append_operand(position_variable.id());
    // store.append_operand(constant);

    // block.append(vk::spv::Op::Return);

    Vector<vk::spv::Word> words;
    builder.build(words);

    for (auto word : words) {
        VULL_EXPECT(output_stream.write_le(word));
    }
}
