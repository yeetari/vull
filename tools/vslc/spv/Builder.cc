#include "Builder.hh"

#include <vull/support/Array.hh>

#define ENUM_WORD(value) static_cast<Word>((value))
#define INST_WORD(opcode, word_count) ((ENUM_WORD(opcode) & 0xffffu) | ((word_count) << 16u))

namespace spv {

void Instruction::append_operand(Word word) {
    m_operands.push(word);
}

void Instruction::append_string_operand(vull::StringView string) {
    Word shift_amount = 0;
    Word word = 0;
    for (auto ch : string) {
        word |= static_cast<Word>(ch) << shift_amount;
        if ((shift_amount += 8) == 32) {
            append_operand(vull::exchange(word, 0u));
            shift_amount = 0;
        }
    }
    append_operand(word);
}

void Instruction::extend_operands(const vull::Vector<Word> &operands) {
    m_operands.extend(operands);
}

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

void Block::write(const vull::Function<void(Word)> &write_word) const {
    m_label.write(write_word);
    for (const auto &instruction : m_instructions) {
        instruction.write(write_word);
    }
}

Function::Function(vull::String name, Id id, Id return_type, Id function_type)
    : m_name(vull::move(name)), m_def_inst(Op::Function, id, return_type) {
    m_def_inst.append_operand(FunctionControl::None);
    m_def_inst.append_operand(function_type);
}

void Function::write(const vull::Function<void(Word)> &write_word) const {
    m_def_inst.write(write_word);
    for (const auto &block : m_blocks) {
        block.write(write_word);
    }
    write_word(INST_WORD(Op::FunctionEnd, 1));
}

Id Builder::float_type(Word width) {
    for (const auto &type : m_types) {
        if (type.op() != Op::TypeFloat) {
            continue;
        }
        if (type.operand(0) == width) {
            return type.id();
        }
    }
    auto &type = m_types.emplace(Op::TypeFloat, m_next_id++);
    type.append_operand(width);
    return type.id();
}

Id Builder::function_type(Id return_type, const vull::Vector<Id> &parameter_types) {
    for (const auto &type : m_types) {
        if (type.op() != Op::TypeFunction) {
            continue;
        }
        if (type.operand_count() == parameter_types.size() + 1 && type.operand(0) == return_type) {
            bool matching = true;
            for (uint32_t i = 0; i < parameter_types.size(); i++) {
                if (type.operand(i + 1) != parameter_types[i]) {
                    matching = false;
                    break;
                }
            }
            if (!matching) {
                continue;
            }
            return type.id();
        }
    }
    auto &type = m_types.emplace(Op::TypeFunction, m_next_id++);
    type.append_operand(return_type);
    for (Id parameter_type : parameter_types) {
        type.append_operand(parameter_type);
    }
    return type.id();
}

Id Builder::pointer_type(StorageClass storage_class, Id pointee_type) {
    for (const auto &type : m_types) {
        if (type.op() != Op::TypePointer) {
            continue;
        }
        if (type.operand(0) == ENUM_WORD(storage_class) && type.operand(1) == pointee_type) {
            return type.id();
        }
    }
    auto &type = m_types.emplace(Op::TypePointer, m_next_id++);
    type.append_operand(storage_class);
    type.append_operand(pointee_type);
    return type.id();
}

Id Builder::vector_type(Id component_type, Word component_count) {
    for (const auto &type : m_types) {
        if (type.op() != Op::TypeVector) {
            continue;
        }
        if (type.operand(0) == component_type && type.operand(1) == component_count) {
            return type.id();
        }
    }
    auto &type = m_types.emplace(Op::TypeVector, m_next_id++);
    type.append_operand(component_type);
    type.append_operand(component_count);
    return type.id();
}

Id Builder::void_type() {
    if (m_void_type == 0) {
        m_types.emplace(Op::TypeVoid, m_next_id);
        m_void_type = m_next_id++;
    }
    return m_void_type;
}

Instruction &Builder::scalar_constant(Id type, Word value) {
    for (auto &constant : m_constants) {
        if (constant.type() == type && constant.operand(0) == value) {
            return constant;
        }
    }
    auto &constant = m_constants.emplace(Op::Constant, m_next_id++, type);
    constant.append_operand(value);
    return constant;
}

Instruction &Builder::composite_constant(Id type, vull::Vector<Id> &&elements) {
    // TODO: Deduplication.
    auto &constant = m_constants.emplace(Op::ConstantComposite, m_next_id++, type);
    constant.extend_operands(elements);
    return constant;
}

void Builder::append_entry_point(Function &function, ExecutionModel model) {
    m_entry_points.push(EntryPoint{function, model});
}

Function &Builder::append_function(vull::StringView name, Id return_type, Id function_type) {
    return m_functions.emplace(name, m_next_id++, return_type, function_type);
}

Instruction &Builder::append_variable(Id type, StorageClass storage_class, Id initialiser) {
    auto &variable = m_global_variables.emplace(Op::Variable, m_next_id++, type);
    variable.append_operand(storage_class);
    if (initialiser != 0) {
        variable.append_operand(initialiser);
    }
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

    // Emit single required OpMemoryModel.
    write_word(INST_WORD(Op::MemoryModel, 3));
    write_word(ENUM_WORD(AddressingModel::Logical));
    write_word(ENUM_WORD(MemoryModel::Glsl450));

    for (const auto &entry_point : m_entry_points) {
        Instruction inst(Op::EntryPoint);
        inst.append_operand(entry_point.model);
        inst.append_operand(entry_point.function.def_inst().id());
        inst.append_string_operand(entry_point.function.name());
        for (const auto &variable : m_global_variables) {
            inst.append_operand(variable.id());
        }
        inst.write(write_word);
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
    for (const auto &function : m_functions) {
        function.write(write_word);
    }
}

} // namespace spv
