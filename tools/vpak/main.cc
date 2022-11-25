#include "GltfParser.hh"

#include <vull/core/Log.hh>
#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vpak/Writer.hh>

#include <stdlib.h>
#include <string.h>

namespace {

void print_usage(vull::StringView executable) {
    vull::String whitespace(executable.length());
    memset(whitespace.data(), ' ', whitespace.length());

    vull::StringBuilder sb;
    sb.append("usage:\n");
    sb.append("  {} <command> [<args>]\n", executable);
    sb.append("  {} convert-gltf [--dump-json] [--fast|--ultra] [--max-resolution]\n", executable);
    sb.append("  {}              [--reproducible] <input-gltf> [output-vpak]\n", whitespace);
    sb.append("  {} help\n", executable);
    sb.append("  {} ls <vpak>\n", executable);
    sb.append("  {} stat <vpak> <name>\n", executable);
    sb.append("\narguments:\n");
    sb.append("  [output-vpak]    The vpak file to be written (default: scene.vpak)\n");
    sb.append("  --dump-json      Dump the JSON scene data contained in the glTF\n");
    sb.append("  --fast           Use the lowest Zstd compression level (negative)\n");
    sb.append("  --max-resolution Don't discard the top mip for textures >1K\n");
    sb.append("  --reproducible   Limit the writer to one thread\n");
    sb.append("  --ultra          Use the highest Zstd compression level (warning: will increase memory usage)\n");
    sb.append("\nexamples:\n");
    sb.append("  {} convert-gltf --fast sponza.glb\n", executable);
    sb.append("  {} ls scene.vpak\n", executable);
    sb.append("  {} stat scene.vpak /default_albedo", executable);
    vull::log_raw(sb.build());
}

int convert_gltf(const vull::Vector<vull::StringView> &args) {
    bool dump_json = false;
    bool fast = false;
    bool max_resolution = false;
    bool reproducible = false;
    bool ultra = false;
    vull::StringView input_path;
    vull::StringView output_path;
    for (const auto arg : vull::slice(args, 2u)) {
        if (arg == "--dump-json") {
            dump_json = true;
        } else if (arg == "--fast") {
            fast = true;
        } else if (arg == "--max-resolution") {
            max_resolution = true;
        } else if (arg == "--reproducible") {
            reproducible = true;
        } else if (arg == "--ultra") {
            ultra = true;
        } else if (arg[0] == '-') {
            vull::log_raw(vull::format("fatal: unknown option {}", arg));
            return EXIT_FAILURE;
        } else if (input_path.empty()) {
            input_path = arg;
        } else if (output_path.empty()) {
            output_path = arg;
        } else {
            vull::log_raw(vull::format("fatal: unexpected argument {}", arg));
            return EXIT_FAILURE;
        }
    }

    if (fast && ultra) {
        vull::log_raw("fatal: cannot have --fast and --ultra");
        return EXIT_FAILURE;
    }

    if (input_path.empty()) {
        vull::log_raw("fatal: missing <input-gltf> argument");
        return EXIT_FAILURE;
    }
    output_path = !output_path.empty() ? output_path : "scene.vpak";

    auto compression_level = vull::vpak::CompressionLevel::Normal;
    if (fast) {
        compression_level = vull::vpak::CompressionLevel::Fast;
    }
    if (ultra) {
        compression_level = vull::vpak::CompressionLevel::Ultra;
    }

    GltfParser gltf_parser;
    if (!gltf_parser.parse_glb(input_path)) {
        return EXIT_FAILURE;
    }

    if (dump_json) {
        vull::log_raw(gltf_parser.json());
        return EXIT_SUCCESS;
    }

    auto file = VULL_EXPECT(
        vull::open_file(output_path, vull::OpenMode::Create | vull::OpenMode::Truncate | vull::OpenMode::Write));
    vull::vpak::Writer pack_writer(vull::make_unique<vull::FileStream>(file.create_stream()), compression_level);
    if (!gltf_parser.convert(pack_writer, max_resolution, reproducible)) {
        return EXIT_FAILURE;
    }

    const auto bytes_written = pack_writer.finish();
    vull::info("[main] Wrote {} bytes to {}", bytes_written, output_path);
    return EXIT_SUCCESS;
}

int ls(const vull::Vector<vull::StringView> &args) {
    if (args.size() != 3) {
        vull::log_raw("fatal: invalid usage");
        return EXIT_FAILURE;
    }
    vull::vpak::Reader pack_reader(VULL_EXPECT(vull::open_file(args[2], vull::OpenMode::Read)));
    for (const auto &entry : pack_reader.entries()) {
        vull::log_raw(vull::format("{}", entry.name));
    }
    return EXIT_SUCCESS;
}

vull::StringView type_string(vull::vpak::EntryType type) {
    switch (type) {
    case vull::vpak::EntryType::Blob:
        return "blob";
    case vull::vpak::EntryType::Image:
        return "image";
    case vull::vpak::EntryType::World:
        return "world";
    default:
        return "unknown";
    }
}

int stat(const vull::Vector<vull::StringView> &args) {
    if (args.size() != 4) {
        vull::log_raw("fatal: invalid usage");
        return EXIT_FAILURE;
    }
    vull::vpak::Reader pack_reader(VULL_EXPECT(vull::open_file(args[2], vull::OpenMode::Read)));
    auto entry = pack_reader.stat(args[3]);
    if (!entry) {
        vull::log_raw(vull::format("fatal: no entry named {}", args[3]));
        return EXIT_FAILURE;
    }
    vull::log_raw(vull::format("Size: {} bytes (uncompressed)", entry->size));
    vull::log_raw(vull::format("Type: {}", type_string(entry->type)));
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv) {
    vull::Vector<vull::StringView> args(argv, argv + argc);
    if (args.size() < 2 || args[1] == "help") {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const auto command = args[1];
    if (command == "convert-gltf") {
        return convert_gltf(args);
    }
    if (command == "ls") {
        return ls(args);
    }
    if (command == "stat") {
        return stat(args);
    }

    vull::log_raw(vull::format("fatal: unknown command '{}'", command));
    return EXIT_FAILURE;
}
