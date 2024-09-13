#pragma once

#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/support/hash.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/spirv.hh>

#include <stdint.h>

namespace vull::shaderc::spv {

using namespace vull::vk::spv;

class Builder;

class Instruction {
    const Op m_op;
    const Id m_id;
    const Id m_type;

    // TODO(small-vector)
    Vector<Word> m_operands;

public:
    static bool constant_equals(const Instruction &lhs, const Instruction &rhs) { return lhs.constant_equals(rhs); }
    static bool type_equals(const Instruction &lhs, const Instruction &rhs) { return lhs.type_equals(rhs); }
    static hash_t constant_hash(const Instruction &inst) { return inst.constant_hash(); }
    static hash_t type_hash(const Instruction &inst) { return inst.type_hash(); }

    Instruction() : Instruction(Op::Nop) {}
    Instruction(Op op, Id id = 0, Id type = 0) : m_op(op), m_id(id), m_type(type) {}

    template <typename T>
    void append_operand(T operand) requires(sizeof(T) <= sizeof(Word)) {
        append_operand(static_cast<Word>(operand));
    }

    void append_operand(Word operand);
    void append_operand(StringView operand);
    void extend_operands(const Vector<Word> &operands);
    Word operand(uint32_t index) const { return m_operands[index]; }
    uint32_t operand_count() const { return m_operands.size(); }

    bool constant_equals(const Instruction &other) const;
    bool type_equals(const Instruction &other) const;
    hash_t constant_hash() const;
    hash_t type_hash() const;

    void build(Vector<Word> &output) const;

    Op op() const { return m_op; }
    Id id() const { return m_id; }
    Id type() const { return m_type; }
    const Vector<Word> &operands() const { return m_operands; }
};

class Block {
    Builder &m_builder;
    Instruction m_label;
    Vector<UniquePtr<Instruction>> m_instructions;

public:
    explicit Block(Builder &builder);

    Instruction &append(Op op, Id type = 0);
    bool is_terminated() const;

    const Instruction &label() const { return m_label; }
    const Vector<UniquePtr<Instruction>> &instructions() const { return m_instructions; }
};

class Function {
    Builder &m_builder;
    Instruction m_def_inst;
    Vector<UniquePtr<Block>> m_blocks;
    Vector<UniquePtr<Instruction>> m_variables;

public:
    Function(Builder &builder, Id return_type, Id function_type);

    Block &append_block();
    Instruction &append_variable(Id type);

    Builder &builder() { return m_builder; }
    const Instruction &def_inst() const { return m_def_inst; }
    const Vector<UniquePtr<Block>> &blocks() const { return m_blocks; }
    const Vector<UniquePtr<Instruction>> &variables() const { return m_variables; }
};

class EntryPoint {
    String m_name;
    Function &m_function;
    ExecutionModel m_execution_model;
    Vector<UniquePtr<Instruction>> m_interface_variables;

public:
    EntryPoint(String &&name, Function &function, ExecutionModel execution_model)
        : m_name(vull::move(name)), m_function(function), m_execution_model(execution_model) {}

    Instruction &append_variable(Id type, StorageClass storage_class);

    const String &name() const { return m_name; }
    Function &function() const { return m_function; }
    ExecutionModel execution_model() const { return m_execution_model; }
    const Vector<UniquePtr<Instruction>> &interface_variables() const { return m_interface_variables; }
};

class Builder {
    // Roughly follows the logical layout of a module.
    HashSet<Capability> m_capabilities;
    Vector<Instruction> m_extension_imports;
    Vector<UniquePtr<EntryPoint>> m_entry_points;
    Vector<Instruction> m_decorations;
    HashSet<Instruction, &Instruction::type_hash, &Instruction::type_equals> m_types;
    HashSet<Instruction, &Instruction::constant_hash, &Instruction::constant_equals> m_constants;
    Vector<UniquePtr<Function>> m_functions;

    // Counter for instruction result IDs.
    Id m_next_id{1};

    Id ensure_constant(Instruction &&inst);
    Id ensure_type(Instruction &&inst);

public:
    template <typename... Literals>
    void decorate(Id id, Decoration decoration, Literals... literals);
    template <typename... Literals>
    void decorate_member(Id struct_id, Word member, Decoration decoration, Literals... literals);

    void ensure_capability(Capability capability);
    Id import_extension(StringView name);

    EntryPoint &append_entry_point(String name, Function &function, ExecutionModel execution_model);
    Function &append_function(Id return_type, Id function_type);

    Id scalar_constant(Id type, Word value);
    Id composite_constant(Id type, Vector<Id> &&elements);

    Id float_type(Word width);
    Id function_type(Id return_type, const Vector<Id> &parameter_types);
    Id int_type(Word width, bool is_signed);
    Id matrix_type(Id column_type, Word column_count);
    Id pointer_type(StorageClass storage_class, Id pointee_type);
    Id struct_type(const Vector<Id> &member_types, bool is_block);
    Id vector_type(Id component_type, Word component_count);
    Id void_type();

    void build(Vector<Word> &output) const;
    Id make_id() { return m_next_id++; }
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

} // namespace vull::shaderc::spv
