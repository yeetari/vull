#pragma once

#include <vull/io/PackFile.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/support/Vector.hh>

#include <cstdint>

struct FileSystem {
    static void initialise(const char *program_name);
    static void deinitialise();
    static Vector<std::uint8_t> load(PackEntryType type, const char *name);
    static Mesh load_mesh(const char *name);
    static Vector<std::uint8_t> load_shader(const char *name);
};
