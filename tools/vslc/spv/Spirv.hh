#pragma once

#include <stdint.h>

namespace spv {

using Word = uint32_t;
using Id = Word;

constexpr Word k_magic_number = 0x07230203;

enum class AddressingModel {
    Logical = 0,
};

enum class Capability {
    Shader = 1,
};

enum class ExecutionModel {
    Vertex = 0,
};

enum class MemoryModel {
    Glsl450 = 1,
};

enum class Op {
    MemoryModel = 14,
    EntryPoint = 15,
    Capability = 17,
};

} // namespace spv
