#pragma once

#include "Spirv.hh"

#include <vull/container/Vector.hh>
#include <vull/support/Function.hh> // IWYU pragma: keep
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace spv {

class Builder;

class Instruction {
    const Op m_op;
    const Id m_id;
    const Id m_type;
    // TODO(small-vector): Use a small vector - an instruction likely only has a few operands.
    vull::Vector<Word> m_operands;

    // TODO: Hack for struct type.
    bool m_is_block_decorated;

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

    void set_is_block_decorated(bool is_block_decorated) { m_is_block_decorated = is_block_decorated; }

    Op op() const { return m_op; }
    Id id() const { return m_id; }
    Id type() const { return m_type; }
    Word operand(uint32_t index) const { return m_operands[index]; }
    uint32_t operand_count() const { return m_operands.size(); }
    const vull::Vector<Word> &operands() const { return m_operands; }
    bool is_block_decorated() const { return m_is_block_decorated; }
};

class Block {
    Builder &m_builder;
    Instruction m_label;
    vull::Vector<vull::UniquePtr<Instruction>> m_instructions;

public:
    explicit Block(Builder &builder);

    bool is_terminated() const;

    Instruction &append(Op op, Id type = 0);
    void write_label(const vull::Function<void(Word)> &write_word) const;
    void write_insts(const vull::Function<void(Word)> &write_word) const;
    void write(const vull::Function<void(Word)> &write_word) const;
};

class Function {
    Builder &m_builder;
    vull::String m_name;
    Instruction m_def_inst;
    vull::Vector<Instruction> m_variables;
    vull::Vector<Block> m_blocks;

public:
    Function(Builder &builder, vull::String name, Id return_type, Id function_type);

    Block &append_block();
    Instruction &append_variable(Id type);
    void write(const vull::Function<void(Word)> &write_word) const;

    Builder &builder() const { return m_builder; }
    const vull::String &name() const { return m_name; }
    const Instruction &def_inst() const { return m_def_inst; }
};

class EntryPoint {
    Function &m_function;
    ExecutionModel m_execution_model;
    vull::Vector<Instruction> m_global_variables;

public:
    EntryPoint(Function &function, ExecutionModel execution_model)
        : m_function(function), m_execution_model(execution_model) {}

    Instruction &append_variable(Id type, StorageClass storage_class);

    Function &function() const { return m_function; }
    ExecutionModel execution_model() const { return m_execution_model; }
    const vull::Vector<Instruction> &global_variables() const { return m_global_variables; }
};

class Builder {
    vull::Vector<Instruction> m_ext_inst_imports;
    vull::Vector<vull::UniquePtr<EntryPoint>> m_entry_points;
    vull::Vector<Instruction> m_decorations;
    vull::Vector<Instruction> m_types;
    vull::Vector<Instruction> m_constants;
    vull::Vector<Instruction> m_global_variables;
    vull::Vector<vull::UniquePtr<Function>> m_functions;
    Id m_next_id{1};
    Id m_void_type{0};

public:
    Id float_type(Word width);
    Id function_type(Id return_type, const vull::Vector<Id> &parameter_types);
    Id int_type(Word width, bool is_signed);
    Id matrix_type(Id column_type, Word column_count);
    Id pointer_type(StorageClass storage_class, Id pointee_type);
    Id struct_type(const vull::Vector<Id> &member_types, bool block);
    Id vector_type(Id component_type, Word component_count);
    Id void_type();

    Instruction &scalar_constant(Id type, Word value);
    Instruction &composite_constant(Id type, vull::Vector<Id> &&elements);

    Id import_extension(vull::StringView name);
    EntryPoint &append_entry_point(Function &function, ExecutionModel model);
    Function &append_function(vull::StringView name, Id return_type, Id function_type);
    Instruction &append_variable(Id type, StorageClass storage_class);

    template <typename... Literals>
    void decorate(Id id, Decoration decoration, Literals... literals);
    template <typename... Literals>
    void decorate_member(Id struct_id, Word member, Decoration decoration, Literals... literals);

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

template <typename... Literals>
void Builder::decorate_member(Id struct_id, Word member, Decoration decoration, Literals... literals) {
    auto &inst = m_decorations.emplace(Op::MemberDecorate);
    inst.append_operand(struct_id);
    inst.append_operand(member);
    inst.append_operand(decoration);
    (inst.append_operand(literals), ...);
}

} // namespace spv
