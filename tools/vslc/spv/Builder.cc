#include "Builder.hh"

#include <vull/support/Array.hh>

#define ENUM_WORD(value) static_cast<Word>((value))
#define INST_WORD(opcode, word_count) ((ENUM_WORD(opcode) & 0xffffu) | ((word_count) << 16u))

namespace spv {

void Builder::write(vull::Function<void(Word)> write_word) const {
    // Note that SPIR-V can be written in any endian as it is up to the reader to determine the endian and flip it
    // whilst reading if necessary.
    write_word(k_magic_number);
    write_word(0x00010600); // SPIR-V 1.6
    write_word(0);
    write_word(m_next_id + 1);
    write_word(0);

    // Emit shader capability.
    write_word(INST_WORD(Op::Capability, 2));
    write_word(ENUM_WORD(Capability::Shader));

    // Emit single required OpMemoryModel.
    write_word(INST_WORD(Op::MemoryModel, 3));
    write_word(ENUM_WORD(AddressingModel::Logical));
    write_word(ENUM_WORD(MemoryModel::Glsl450));

    write_word(INST_WORD(Op::EntryPoint, 5));
    write_word(ENUM_WORD(ExecutionModel::Vertex));
    write_word(4);
    write_word(0x6e69616d);
    write_word(0x00000000);
}

} // namespace spv
