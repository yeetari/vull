#include "builder.hh"

#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#define ENUM_WORD(value) static_cast<Word>((value))
#define INST_WORD(opcode, word_count) ((ENUM_WORD(opcode) & 0xffffu) | ((word_count) << 16u))

namespace spv {

void Instruction::write(const vull::Function<void(Word)> &write_word) const {
    write_word(INST_WORD(m_op, m_operands.size() + 1 + (m_type != 0) + (m_id != 0)));
    if (m_type != 0) {
        write_word(m_type);
    }
    if (m_id != 0) {
        write_word(m_id);
    }
    for (Word operand : m_operands) {
        write_word(operand);
    }
}

void Block::write_label(const vull::Function<void(Word)> &write_word) const {
    m_label.write(write_word);
}

void Block::write_insts(const vull::Function<void(Word)> &write_word) const {
    for (const auto &instruction : m_instructions) {
        instruction->write(write_word);
    }
}

void Block::write(const vull::Function<void(Word)> &write_word) const {
    write_label(write_word);
    write_insts(write_word);
}

void Function::write(const vull::Function<void(Word)> &write_word) const {
    m_def_inst.write(write_word);
    m_blocks[0].write_label(write_word);
    for (const auto &variable : m_variables) {
        variable.write(write_word);
    }
    m_blocks[0].write_insts(write_word);
    for (uint32_t i = 1; i < m_blocks.size(); i++) {
        m_blocks[i].write(write_word);
    }
    write_word(INST_WORD(Op::FunctionEnd, 1));
}

Instruction &EntryPoint::append_variable(Id type, StorageClass storage_class) {
    const auto pointer_type = m_function.builder().pointer_type(storage_class, type);
    auto &variable = m_global_variables.emplace(Op::Variable, m_function.builder().make_id(), pointer_type);
    variable.append_operand(storage_class);
    return variable;
}

EntryPoint &Builder::append_entry_point(Function &function, ExecutionModel model) {
    return *m_entry_points.emplace(new EntryPoint(function, model));
}

Function &Builder::append_function(vull::StringView name, Id return_type, Id function_type) {
    return *m_functions.emplace(new Function(*this, name, return_type, function_type));
}

Instruction &Builder::append_variable(Id type, StorageClass storage_class) {
    auto &variable = m_global_variables.emplace(Op::Variable, m_next_id++, pointer_type(storage_class, type));
    variable.append_operand(storage_class);
    return variable;
}

void Builder::write(vull::Function<void(Word)> write_word) const {
    // Note that SPIR-V can be written in any endian as it is up to the reader to determine the endian and flip it
    // whilst reading if necessary.
    write_word(k_magic_number);
    write_word(0x00010600); // SPIR-V 1.6
    write_word(0);
    write_word(m_next_id);
    write_word(0);

    // Emit shader capability.
    write_word(INST_WORD(Op::Capability, 2));
    write_word(ENUM_WORD(Capability::Shader));

    for (const auto &instruction : m_ext_inst_imports) {
        instruction.write(write_word);
    }

    // Emit single required OpMemoryModel.
    write_word(INST_WORD(Op::MemoryModel, 3));
    write_word(ENUM_WORD(AddressingModel::Logical));
    write_word(ENUM_WORD(MemoryModel::Glsl450));

    for (const auto &entry_point : m_entry_points) {
        const auto function_id = entry_point->function().def_inst().id();
        Instruction inst(Op::EntryPoint);
        inst.append_operand(entry_point->execution_model());
        inst.append_operand(function_id);
        inst.append_string_operand(entry_point->function().name());
        for (const auto &variable : m_global_variables) {
            inst.append_operand(variable.id());
        }
        for (const auto &variable : entry_point->global_variables()) {
            inst.append_operand(variable.id());
        }
        inst.write(write_word);

        if (entry_point->execution_model() == ExecutionModel::Fragment) {
            Instruction origin_inst(Op::ExecutionMode);
            origin_inst.append_operand(function_id);
            origin_inst.append_operand(ExecutionMode::OriginUpperLeft);
            origin_inst.write(write_word);
        }
    }
    for (const auto &decoration : m_decorations) {
        decoration.write(write_word);
    }
    for (const auto &type : m_types) {
        type.write(write_word);
    }
    for (const auto &constant : m_constants) {
        constant.write(write_word);
    }
    for (const auto &variable : m_global_variables) {
        variable.write(write_word);
    }
    for (vull::HashSet<Id> seen_variables; const auto &entry_point : m_entry_points) {
        for (const auto &variable : entry_point->global_variables()) {
            if (seen_variables.add(variable.id())) {
                continue;
            }
            variable.write(write_word);
        }
    }
    for (const auto &function : m_functions) {
        function->write(write_word);
    }
}

} // namespace spv
