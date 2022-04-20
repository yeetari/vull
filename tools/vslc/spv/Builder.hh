#pragma once

#include "Spirv.hh"

#include <vull/support/Function.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>

namespace spv {

class Instruction {
    const Op m_op;
    const Id m_id;
    const Id m_type;
    // TODO(small-vector): Use a small vector - an instruction likely only has a few operands.
    vull::Vector<Word> m_operands;

public:
    Instruction(Op op, Id id = 0, Id type = 0) : m_op(op), m_id(id), m_type(type) {}

    template <typename T>
    void append_operand(T operand) requires(sizeof(T) <= sizeof(Word)) {
        append_operand(static_cast<Word>(operand));
    }
    void append_operand(Word word);
    void append_string_operand(vull::StringView string);
    void extend_operands(const vull::Vector<Word> &operands);
    void write(const vull::Function<void(Word)> &write_word) const;

    Op op() const { return m_op; }
    Id id() const { return m_id; }
    Id type() const { return m_type; }
    Word operand(uint32_t index) const { return m_operands[index]; }
    uint32_t operand_count() const { return m_operands.size(); }
    const vull::Vector<Word> &operands() const { return m_operands; }
};

class Block {
    Instruction m_label;
    vull::Vector<Instruction> m_instructions;

public:
    explicit Block(Id id) : m_label(Op::Label, id) {}

    template <typename... Args>
    Instruction &append(Args &&...args) {
        return m_instructions.emplace(vull::forward<Args>(args)...);
    }
    void write(const vull::Function<void(Word)> &write_word) const;
};

class Function {
    vull::String m_name;
    Instruction m_def_inst;
    vull::Vector<Block> m_blocks;

public:
    Function(vull::String name, Id id, Id return_type, Id function_type);

    Block &append_block(Id id) { return m_blocks.emplace(id); }
    void write(const vull::Function<void(Word)> &write_word) const;

    const vull::String &name() const { return m_name; }
    const Instruction &def_inst() const { return m_def_inst; }
};

class Builder {
    struct EntryPoint {
        Function &function;
        ExecutionModel model;
    };

    vull::Vector<EntryPoint> m_entry_points;
    vull::Vector<Instruction> m_decorations;
    vull::Vector<Instruction> m_types;
    vull::Vector<Instruction> m_constants;
    vull::Vector<Instruction> m_global_variables;
    vull::Vector<Function> m_functions;
    Id m_next_id{1};
    Id m_void_type{0};

public:
    Id float_type(Word width);
    Id function_type(Id return_type, const vull::Vector<Id> &parameter_types);
    Id matrix_type(Id column_type, Word column_count);
    Id pointer_type(StorageClass storage_class, Id pointee_type);
    Id vector_type(Id component_type, Word component_count);
    Id void_type();

    Instruction &scalar_constant(Id type, Word value);
    Instruction &composite_constant(Id type, vull::Vector<Id> &&elements);

    void append_entry_point(Function &function, ExecutionModel model);
    Function &append_function(vull::StringView name, Id return_type, Id function_type);
    Instruction &append_variable(Id type, StorageClass storage_class, Id initialiser = 0);
    template <typename... Literals>
    void decorate(Id id, Decoration decoration, Literals... literals);

    Id make_id() { return m_next_id++; }
    void write(vull::Function<void(Word)> write_word) const;
};

template <typename... Literals>
void Builder::decorate(Id id, Decoration decoration, Literals... literals) {
    auto &inst = m_decorations.emplace(Op::Decorate);
    inst.append_operand(id);
    inst.append_operand(decoration);
    (inst.append_operand(literals), ...);
}

} // namespace spv
