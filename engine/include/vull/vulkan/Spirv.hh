#pragma once

#include <vull/support/Array.hh>

#include <stdint.h>

namespace vull::vk::spv {

using Word = uint32_t;
using Id = Word;

constexpr Word k_magic_number = 0x07230203;
constexpr Array<uint8_t, 4> k_magic_bytes{0x07, 0x23, 0x02, 0x03};

enum class AddressingModel {
    Logical = 0,
};

enum class BuiltIn {
    Position = 0,
};

enum class Capability {
    Shader = 1,
};

enum class Decoration {
    BuiltIn = 11,
    Location = 30,
};

enum class ExecutionModel {
    Vertex = 0,
    Fragment = 4,
    GLCompute = 5,
};

enum class FunctionControl {
    None = 0,
};

enum class MemoryModel {
    Glsl450 = 1,
};

enum class Op {
    Nop = 0,
    MemoryModel = 14,
    EntryPoint = 15,
    Capability = 17,
    TypeVoid = 19,
    TypeFloat = 22,
    TypeVector = 23,
    TypeMatrix = 24,
    TypePointer = 32,
    TypeFunction = 33,
    Constant = 43,
    ConstantComposite = 44,
    Function = 54,
    FunctionEnd = 56,
    Variable = 59,
    Load = 61,
    Store = 62,
    Decorate = 71,
    CompositeConstruct = 80,
    CompositeExtract = 81,
    FNegate = 127,
    FAdd = 129,
    FSub = 131,
    FMul = 133,
    FDiv = 136,
    VectorTimesScalar = 142,
    MatrixTimesScalar = 143,
    VectorTimesMatrix = 144,
    MatrixTimesVector = 145,
    MatrixTimesMatrix = 146,
    Label = 248,
    Return = 253,
    ReturnValue = 254,
};

enum class StorageClass {
    Input = 1,
    Output = 3,
    Function = 7,
};

} // namespace vull::vk::spv
