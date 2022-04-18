#pragma once

#include <stdint.h>

namespace spv {

using Word = uint32_t;
using Id = Word;

constexpr Word k_magic_number = 0x07230203;

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
};

enum class FunctionControl {
    None = 0,
};

enum class MemoryModel {
    Glsl450 = 1,
};

enum class Op {
    MemoryModel = 14,
    EntryPoint = 15,
    Capability = 17,
    TypeVoid = 19,
    TypeFloat = 22,
    TypeVector = 23,
    TypePointer = 32,
    TypeFunction = 33,
    Constant = 43,
    ConstantComposite = 44,
    Function = 54,
    FunctionEnd = 56,
    Variable = 59,
    Store = 62,
    Decorate = 71,
    Label = 248,
    Return = 253,
    ReturnValue = 254,
};

enum class StorageClass {
    Input = 1,
    Output = 3,
};

} // namespace spv
