#include <vull/shaderc/spv_builder.hh>

#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/hash.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
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

void Instruction::build(Vector<Word> &output) const {
    auto word_count = m_operands.size() + 1u;
    if (m_type != 0) {
        word_count++;
    }
    if (m_id != 0) {
        word_count++;
    }

    output.push((word_count << 16u) | static_cast<uint32_t>(m_op));
    if (m_type != 0) {
        output.push(m_type);
    }
    if (m_id != 0) {
        output.push(m_id);
    }
    for (Word operand : m_operands) {
        output.push(operand);
    }
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

Function::Function(Builder &builder, Id return_type, Id function_type)
    : m_builder(builder), m_def_inst(Op::Function, builder.make_id(), return_type) {
    m_def_inst.append_operand(FunctionControl::None);
    m_def_inst.append_operand(function_type);
}

Block &Function::append_block() {
    return *m_blocks.emplace(new Block(m_builder));
}

Instruction &Function::append_variable(Id type) {
    const Id pointer_type = m_builder.pointer_type(StorageClass::Function, type);
    auto &variable = *m_variables.emplace(new Instruction(Op::Variable, m_builder.make_id(), pointer_type));
    variable.append_operand(StorageClass::Function);
    return variable;
}

Instruction &EntryPoint::append_variable(Id type, StorageClass storage_class) {
    auto &builder = m_function.builder();
    const auto pointer_type = builder.pointer_type(storage_class, type);
    auto &variable = *m_interface_variables.emplace(new Instruction(Op::Variable, builder.make_id(), pointer_type));
    variable.append_operand(storage_class);
    return variable;
}

void Builder::ensure_capability(Capability capability) {
    m_capabilities.add(capability);
}

Id Builder::import_extension(StringView name) {
    auto &inst = m_extension_imports.emplace(Op::ExtInstImport, m_next_id++);
    inst.append_operand(name);
    return inst.id();
}

EntryPoint &Builder::append_entry_point(String name, Function &function, ExecutionModel execution_model) {
    return *m_entry_points.emplace(new EntryPoint(vull::move(name), function, execution_model));
}

Function &Builder::append_function(Id return_type, Id function_type) {
    return *m_functions.emplace(new Function(*this, return_type, function_type));
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

// TODO: Hack needed until vull has an ordered hash set.
static Vector<const Instruction &> sorted_set(const auto &set) {
    Vector<const Instruction &> vector;
    for (const auto &instruction : set) {
        vector.push(instruction);
    }
    vull::sort(vector, [](const Instruction &lhs, const Instruction &rhs) {
        return lhs.id() > rhs.id();
    });
    return vector;
}

void Builder::build(Vector<Word> &output) const {
    // The output will always be at least this big.
    output.ensure_capacity(16);

    // Emit header.
    output.push(k_magic_number);
    output.push(0x00010600); // SPIR-V 1.6
    output.push(0xaaaa);     // Generator magic
    output.push(m_next_id);  // ID bound
    output.push(0);          // Schema (reserved)

    // Emit desired capabilities.
    for (const auto capability : m_capabilities) {
        Instruction inst(Op::Capability);
        inst.append_operand(capability);
        inst.build(output);
    }

    // Emit single memory model instruction.
    Instruction memory_model(Op::MemoryModel);
    memory_model.append_operand(AddressingModel::Logical);
    memory_model.append_operand(MemoryModel::Glsl450);
    memory_model.build(output);

    for (const auto &entry_point : m_entry_points) {
        Instruction inst(Op::EntryPoint);
        inst.append_operand(entry_point->execution_model());
        inst.append_operand(entry_point->function().def_inst().id());
        inst.append_operand(entry_point->name());
        for (const auto &variable : entry_point->interface_variables()) {
            inst.append_operand(variable->id());
        }
        inst.build(output);
    }
    for (const auto &decoration : m_decorations) {
        decoration.build(output);
    }
    for (const Instruction &type : sorted_set(m_types)) {
        type.build(output);
    }
    for (const Instruction &constant : sorted_set(m_constants)) {
        constant.build(output);
    }
    for (HashSet<Id> seen_variables; const auto &entry_point : m_entry_points) {
        for (const auto &variable : entry_point->interface_variables()) {
            if (!seen_variables.add(variable->id())) {
                variable->build(output);
            }
        }
    }
    for (const auto &function : m_functions) {
        function->def_inst().build(output);

        // Prepend variables to entry block.
        const auto &blocks = function->blocks();
        if (!blocks.empty()) {
            blocks[0]->label().build(output);
            for (const auto &variable : function->variables()) {
                variable->build(output);
            }
            for (const auto &instruction : blocks[0]->instructions()) {
                instruction->build(output);
            }
        }

        // Emit remaining blocks.
        for (uint32_t i = 1; i < blocks.size(); i++) {
            blocks[i]->label().build(output);
            for (const auto &instruction : blocks[i]->instructions()) {
                instruction->build(output);
            }
        }

        Instruction function_end(Op::FunctionEnd);
        function_end.build(output);
    }
}

} // namespace vull::shaderc::spv
