#pragma once

#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/support/hash.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
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
};

class Builder {
    Vector<Instruction> m_extension_imports;
    Vector<Instruction> m_decorations;
    HashSet<Instruction, &Instruction::type_hash, &Instruction::type_equals> m_types;
    HashSet<Instruction, &Instruction::constant_hash, &Instruction::constant_equals> m_constants;
    Id m_next_id{1};

    Id ensure_constant(Instruction &&inst);
    Id ensure_type(Instruction &&inst);

public:
    template <typename... Literals>
    void decorate(Id id, Decoration decoration, Literals... literals);
    template <typename... Literals>
    void decorate_member(Id struct_id, Word member, Decoration decoration, Literals... literals);

    Id import_extension(StringView name);

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
