#include <vull/io/FileSystem.hh>

#include <vull/io/PackFile.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/Texture.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

struct FileSystemData {
    std::unordered_map<std::string, std::FILE *> packs;
};

// NOLINTNEXTLINE
FileSystemData *s_data = nullptr;

} // namespace

void FileSystem::initialise(const char *program_name) {
    s_data = new FileSystemData;

    std::filesystem::path program_path(program_name);
    for (const auto &entry : std::filesystem::recursive_directory_iterator(program_path.parent_path())) {
        if (!entry.is_regular_file() || entry.path().extension() != ".vpak") {
            continue;
        }
        Log::info("io", "Found %s", entry.path().filename().c_str());
        auto *file = std::fopen(entry.path().c_str(), "r");
        s_data->packs.emplace(entry.path().stem(), file);
    }
}

void FileSystem::deinitialise() {
    ASSERT(s_data != nullptr);
    for (auto &[name, file] : s_data->packs) {
        std::fclose(file);
    }
    delete s_data;
}

Vector<std::uint8_t> FileSystem::load(PackEntryType type, const char *name) {
    std::filesystem::path path(name);
    auto pack_name = path.begin()->string();
    std::string file_name;
    for (auto it = ++path.begin(); it != path.end(); ++it) {
        file_name += '/' + it->string();
    }

    ASSERT(s_data != nullptr);
    if (!s_data->packs.contains(pack_name)) {
        Log::error("io", "No pack named %s", pack_name.c_str());
        std::exit(1);
    }
    PackFile pack(s_data->packs[pack_name]);

    std::uint16_t entry_count = pack.read_header();
    for (std::uint16_t i = 0; i < entry_count; i++) {
        auto entry = pack.read_entry();
        if (entry.type() != type) {
            pack.skip_data(entry);
            continue;
        }
        if (entry.name() == file_name) {
            return pack.read_data(entry);
        }
        pack.skip_data(entry);
    }
    Log::error("io", "Failed to find %s (%s)", name, PackFile::entry_type_str(type));
    std::exit(1);
}

Mesh FileSystem::load_mesh(const char *name) {
    auto data = load(PackEntryType::Mesh, name);
    PackMesh mesh(data);
    return {mesh.index_count(), mesh.index_offset()};
}

Texture FileSystem::load_texture(const char *name) {
    auto data = load(PackEntryType::Texture, name);
    PackTexture texture(data);
    Vector<std::uint8_t> pixels(data.size_bytes() - 8);
    std::memcpy(pixels.data(), data.data() + 8, pixels.capacity());
    return {texture.width(), texture.height(), std::move(pixels)};
}

Vector<std::uint8_t> FileSystem::load_shader(const char *name) {
    return load(PackEntryType::Shader, name);
}
