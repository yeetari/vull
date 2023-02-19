#ifdef BUILD_GLTF
#include "GltfParser.hh"
#endif
#include "PngStream.hh"

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vpak/Writer.hh>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using namespace vull;

namespace {

void print_usage(StringView executable) {
    String whitespace(executable.length());
    memset(whitespace.data(), ' ', whitespace.length());

    StringBuilder sb;
    sb.append("usage:\n");
    sb.append("  {} <command> [<args>]\n", executable);
    sb.append("  {} add [--fast|--ultra] <vpak> <file> <name>\n", executable);
    sb.append("  {} add-gltf [--dump-json] [--fast|--ultra] [--max-resolution]\n", executable);
    sb.append("  {}          [--reproducible] <vpak> <gltf>\n", whitespace);
    sb.append("  {} add-skybox <vpak> <entry> <faces>\n");
    sb.append("  {} get <vpak> <name> <file>\n", executable);
    sb.append("  {} help\n", executable);
    sb.append("  {} ls <vpak>\n", executable);
    sb.append("  {} stat <vpak> <name>\n", executable);
    sb.append("\narguments:\n");
    sb.append("  <vpak>           The vpak file to be inspected/modified\n");
    sb.append("  --dump-json      Dump the JSON scene data contained in the glTF\n");
    sb.append("  --fast           Use the lowest Zstd compression level (negative)\n");
    sb.append("  --max-resolution Don't discard the top mip for textures >1K\n");
    sb.append("  --reproducible   Limit the writer to one thread\n");
    sb.append("                   (only relevant for add-gltf)\n");
    sb.append("  --ultra          Use the highest Zstd compression level\n");
    sb.append("                   (warning: will increase memory usage by a lot)\n");
    sb.append("\nexamples:\n");
    sb.append("  {} add shaders.vpak my_shader.spv /shaders/my_shader\n", executable);
    sb.append("  {} add-gltf --fast sponza.vpak sponza.glb\n", executable);
    sb.append("  {} add-gltf sponza.vpak player_model.glb\n", executable);
    sb.append("  {} ls sounds.vpak\n", executable);
    sb.append("  {} stat textures.vpak /default_albedo", executable);
    vull::println(sb.build());
}

int add(const Vector<StringView> &args) {
    bool fast = false;
    bool ultra = false;
    StringView vpak_path;
    StringView input_path;
    StringView entry_name;
    for (const auto arg : vull::slice(args, 2u)) {
        if (arg == "--fast") {
            fast = true;
        } else if (arg == "--ultra") {
            ultra = true;
        } else if (arg[0] == '-') {
            vull::println("fatal: unknown option {}", arg);
            return EXIT_FAILURE;
        } else if (vpak_path.empty()) {
            vpak_path = arg;
        } else if (input_path.empty()) {
            input_path = arg;
        } else if (entry_name.empty()) {
            entry_name = arg;
        } else {
            vull::println("fatal: unexpected argument {}", arg);
            return EXIT_FAILURE;
        }
    }

    if (fast && ultra) {
        vull::println("fatal: cannot have --fast and --ultra");
        return EXIT_FAILURE;
    }

    if (vpak_path.empty()) {
        vull::println("fatal: missing <vpak> argument");
        return EXIT_FAILURE;
    }
    if (input_path.empty()) {
        vull::println("fatal: missing <file> argument");
        return EXIT_FAILURE;
    }
    if (entry_name.empty()) {
        vull::println("fatal: missing <name> argument");
        return EXIT_FAILURE;
    }

    auto compression_level = vpak::CompressionLevel::Normal;
    if (fast) {
        compression_level = vpak::CompressionLevel::Fast;
    }
    if (ultra) {
        compression_level = vpak::CompressionLevel::Ultra;
    }

    auto input_file_or_error = vull::open_file(input_path, OpenMode::Read);
    if (input_file_or_error.is_error()) {
        vull::println("fatal: failed to open input file {}", input_path);
        return EXIT_FAILURE;
    }
    auto input_file = input_file_or_error.disown_value();
    auto input_stream = input_file.create_stream();

    auto vpak_file = VULL_EXPECT(vull::open_file(vpak_path, OpenMode::Create | OpenMode::Read | OpenMode::Write));
    vpak::Writer pack_writer(vull::make_unique<FileStream>(vpak_file.create_stream()), compression_level);

    auto entry_stream = pack_writer.start_entry(entry_name, vpak::EntryType::Blob);

    Array<uint8_t, 128 * 1024> buffer;
    size_t bytes_read = 0;
    while ((bytes_read = VULL_EXPECT(input_stream.read(buffer.span()))) > 0) {
        VULL_EXPECT(entry_stream.write({buffer.data(), static_cast<uint32_t>(bytes_read)}));
    }
    entry_stream.finish();
    pack_writer.finish();
    return EXIT_SUCCESS;
}

#ifdef BUILD_GLTF
int add_gltf(const Vector<StringView> &args) {
    bool dump_json = false;
    bool fast = false;
    bool max_resolution = false;
    bool reproducible = false;
    bool ultra = false;
    StringView vpak_path;
    StringView gltf_path;
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
            vull::println("fatal: unknown option {}", arg);
            return EXIT_FAILURE;
        } else if (vpak_path.empty()) {
            vpak_path = arg;
        } else if (gltf_path.empty()) {
            gltf_path = arg;
        } else {
            vull::println("fatal: unexpected argument {}", arg);
            return EXIT_FAILURE;
        }
    }

    if (fast && ultra) {
        vull::println("fatal: cannot have --fast and --ultra");
        return EXIT_FAILURE;
    }

    if (vpak_path.empty()) {
        vull::println("fatal: missing <vpak> argument");
        return EXIT_FAILURE;
    }
    if (gltf_path.empty()) {
        vull::println("fatal: missing <gltf> argument");
        return EXIT_FAILURE;
    }

    auto compression_level = vpak::CompressionLevel::Normal;
    if (fast) {
        compression_level = vpak::CompressionLevel::Fast;
    }
    if (ultra) {
        compression_level = vpak::CompressionLevel::Ultra;
    }

    GltfParser gltf_parser;
    if (!gltf_parser.parse_glb(gltf_path)) {
        return EXIT_FAILURE;
    }

    if (dump_json) {
        vull::println(gltf_parser.json());
        return EXIT_SUCCESS;
    }

    auto vpak_file = VULL_EXPECT(vull::open_file(vpak_path, OpenMode::Create | OpenMode::Read | OpenMode::Write));
    vpak::Writer pack_writer(vull::make_unique<FileStream>(vpak_file.create_stream()), compression_level);
    if (!gltf_parser.convert(pack_writer, max_resolution, reproducible)) {
        return EXIT_FAILURE;
    }

    const auto bytes_written = pack_writer.finish();
    vull::info("[main] Wrote {} bytes to {}", bytes_written, vpak_path);
    return EXIT_SUCCESS;
}
#else
int add_gltf(const Vector<StringView> &) {
    vull::println("fatal: not built with gltf support");
    return EXIT_FAILURE;
}
#endif

int add_skybox(const Vector<StringView> &args) {
    if (args.size() != 10) {
        vull::println("fatal: invalid usage");
        return EXIT_FAILURE;
    }

    Vector<FileStream> face_streams;
    for (const auto face_path : vull::slice(args, 4u)) {
        auto file_or_error = vull::open_file(face_path, OpenMode::Read);
        if (file_or_error.is_error()) {
            vull::println("fatal: failed to open file {}", face_path);
            return EXIT_FAILURE;
        }
        face_streams.push(file_or_error.disown_value().create_stream());
    }

    auto vpak_file = VULL_EXPECT(vull::open_file(args[2], OpenMode::Create | OpenMode::Read | OpenMode::Write));
    vpak::Writer pack_writer(vull::make_unique<FileStream>(vpak_file.create_stream()), vpak::CompressionLevel::Normal);
    auto entry_stream = pack_writer.start_entry(args[3], vpak::EntryType::Blob);
    for (auto &stream : face_streams) {
        auto png_stream = VULL_EXPECT(PngStream::create(stream.clone_unique()));
        for (uint32_t y = 0; y < png_stream.height(); y++) {
            Array<uint8_t, 32768> row_buffer;
            png_stream.read_row(row_buffer.span());
            VULL_EXPECT(entry_stream.write(row_buffer.span().subspan(0, png_stream.row_byte_count())));
        }
    }
    entry_stream.finish();
    pack_writer.finish();
    return EXIT_SUCCESS;
}

int get(const Vector<StringView> &args) {
    if (args.size() != 5) {
        vull::println("fatal: invalid usage");
        return EXIT_FAILURE;
    }
    vpak::Reader pack_reader(VULL_EXPECT(vull::open_file(args[2], OpenMode::Read)));
    auto entry_stream = pack_reader.open(args[3]);
    if (!entry_stream) {
        vull::println("fatal: no entry named {}", args[3]);
        return EXIT_FAILURE;
    }

    auto output_file_or_error = vull::open_file(args[4], OpenMode::Create | OpenMode::Truncate | OpenMode::Write);
    if (output_file_or_error.is_error()) {
        vull::println("fatal: failed to create output file {}", args[4]);
        return EXIT_FAILURE;
    }

    auto output_file = output_file_or_error.disown_value();
    auto output_stream = output_file.create_stream();

    // TODO: Do this in a nicer way.
    uint32_t size = pack_reader.stat(args[3])->size;
    while (size > 0) {
        Array<uint8_t, 64 * 1024> buffer;
        const auto to_read = vull::min(buffer.size(), size);
        VULL_EXPECT(entry_stream->read({buffer.data(), to_read}));
        VULL_EXPECT(output_stream.write({buffer.data(), to_read}));
        size -= to_read;
    }
    return EXIT_SUCCESS;
}

int ls(const Vector<StringView> &args) {
    if (args.size() != 3) {
        vull::println("fatal: invalid usage");
        return EXIT_FAILURE;
    }
    vpak::Reader pack_reader(VULL_EXPECT(vull::open_file(args[2], OpenMode::Read)));
    for (const auto &entry : pack_reader.entries()) {
        vull::println("{}", entry.name);
    }
    return EXIT_SUCCESS;
}

StringView type_string(vpak::EntryType type) {
    switch (type) {
    case vpak::EntryType::Blob:
        return "blob";
    case vpak::EntryType::Image:
        return "image";
    case vpak::EntryType::World:
        return "world";
    default:
        return "unknown";
    }
}

int stat(const Vector<StringView> &args) {
    if (args.size() != 4) {
        vull::println("fatal: invalid usage");
        return EXIT_FAILURE;
    }
    vpak::Reader pack_reader(VULL_EXPECT(vull::open_file(args[2], OpenMode::Read)));
    auto entry = pack_reader.stat(args[3]);
    if (!entry) {
        vull::println("fatal: no entry named {}", args[3]);
        return EXIT_FAILURE;
    }
    vull::println("Size: {} bytes (uncompressed)", entry->size);
    vull::println("Type: {}", type_string(entry->type));
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv) {
    Vector<StringView> args(argv, argv + argc);
    if (args.size() < 2 || args[1] == "help") {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    const auto command = args[1];
    if (command == "add") {
        return add(args);
    }
    if (command == "add-gltf") {
        return add_gltf(args);
    }
    if (command == "add-skybox") {
        return add_skybox(args);
    }
    if (command == "get") {
        return get(args);
    }
    if (command == "ls") {
        return ls(args);
    }
    if (command == "stat") {
        return stat(args);
    }

    vull::println("fatal: unknown command '{}'", command);
    return EXIT_FAILURE;
}
