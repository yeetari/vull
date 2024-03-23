#include <vull/shaderc/spv_builder.hh>

#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/hash.hh>
#include <vull/support/optional.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/spirv.hh>

namespace vull::shaderc::spv {

void Instruction::append_operand(Word operand) {
    m_operands.push(operand);
}

void Instruction::append_operand(StringView operand) {
    Word shift_amount = 0;
    Word word = 0;
    for (auto ch : operand) {
        word |= static_cast<Word>(ch) << shift_amount;
        if ((shift_amount += 8) == 32) {
            append_operand(vull::exchange(word, 0u));
            shift_amount = 0;
        }
    }
    append_operand(word);
}

void Instruction::extend_operands(const Vector<Word> &operands) {
    m_operands.extend(operands);
}

bool Instruction::constant_equals(const Instruction &other) const {
    return type_equals(other) && m_type == other.m_type;
}

bool Instruction::type_equals(const Instruction &other) const {
    if (m_op != other.m_op) {
        return false;
    }
    if (m_operands.size() != other.m_operands.size()) {
        return false;
    }
    for (uint32_t i = 0; i < m_operands.size(); i++) {
        if (m_operands[i] != other.m_operands[i]) {
            return false;
        }
    }
    return true;
}

hash_t Instruction::constant_hash() const {
    return vull::hash_combine(type_hash(), m_type);
}

hash_t Instruction::type_hash() const {
    auto hash = vull::hash_of(m_op);
    for (Word operand : m_operands) {
        hash = vull::hash_combine(hash, operand);
    }
    return hash;
}

Block::Block(Builder &builder) : m_builder(builder), m_label(Op::Label, builder.make_id()) {}

static bool op_has_id(Op op) {
    switch (op) {
    case Op::Store:
    case Op::Return:
    case Op::ReturnValue:
        return false;
    default:
        return true;
    }
}

Instruction &Block::append(Op op, Id type) {
    return *m_instructions.emplace(new Instruction(op, op_has_id(op) ? m_builder.make_id() : 0, type));
}

bool Block::is_terminated() const {
    return !m_instructions.empty() && spv::is_terminator(m_instructions.last()->op());
}

Id Builder::import_extension(StringView name) {
    auto &inst = m_extension_imports.emplace(Op::ExtInstImport, m_next_id++);
    inst.append_operand(name);
    return inst.id();
}

Id Builder::ensure_constant(Instruction &&inst) {
    if (auto existing = m_constants.add(vull::move(inst))) {
        return existing->id();
    }
    return m_next_id++;
}

Id Builder::scalar_constant(Id type, Word value) {
    Instruction inst(Op::Constant, m_next_id, type);
    inst.append_operand(value);
    return ensure_constant(vull::move(inst));
}

Id Builder::composite_constant(Id type, Vector<Id> &&elements) {
    Instruction inst(Op::ConstantComposite, m_next_id, type);
    inst.extend_operands(elements);
    return ensure_constant(vull::move(inst));
}

Id Builder::ensure_type(Instruction &&inst) {
    if (auto existing = m_types.add(vull::move(inst))) {
        return existing->id();
    }
    return m_next_id++;
}

Id Builder::float_type(Word width) {
    Instruction inst(Op::TypeFloat, m_next_id);
    inst.append_operand(width);
    return ensure_type(vull::move(inst));
}

Id Builder::function_type(Id return_type, const Vector<Id> &parameter_types) {
    Instruction inst(Op::TypeFunction, m_next_id);
    inst.append_operand(return_type);
    inst.extend_operands(parameter_types);
    return ensure_type(vull::move(inst));
}

Id Builder::int_type(Word width, bool is_signed) {
    Instruction inst(Op::TypeInt, m_next_id);
    inst.append_operand(width);
    inst.append_operand(is_signed ? 1 : 0);
    return ensure_type(vull::move(inst));
}

Id Builder::matrix_type(Id column_type, Word column_count) {
    Instruction inst(Op::TypeMatrix, m_next_id);
    inst.append_operand(column_type);
    inst.append_operand(column_count);
    return ensure_type(vull::move(inst));
}

Id Builder::pointer_type(StorageClass storage_class, Id pointee_type) {
    Instruction inst(Op::TypePointer, m_next_id);
    inst.append_operand(static_cast<Word>(storage_class));
    inst.append_operand(pointee_type);
    return ensure_type(vull::move(inst));
}

Id Builder::struct_type(const Vector<Id> &member_types, bool is_block) {
    VULL_ENSURE(!is_block);
    Instruction inst(Op::TypeStruct, m_next_id);
    inst.extend_operands(member_types);
    return ensure_type(vull::move(inst));
}

Id Builder::vector_type(Id component_type, Word component_count) {
    Instruction inst(Op::TypeVector, m_next_id);
    inst.append_operand(component_type);
    inst.append_operand(component_count);
    return ensure_type(vull::move(inst));
}

Id Builder::void_type() {
    Instruction inst(Op::TypeVoid, m_next_id);
    return ensure_type(vull::move(inst));
}

} // namespace vull::shaderc::spv
