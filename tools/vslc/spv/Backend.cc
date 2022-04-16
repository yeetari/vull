#include "Backend.hh"

namespace spv {

void Backend::visit(const ast::Block &vsl_block) {
    m_block = &m_function->append_block(m_builder.make_id());
    vsl_block.traverse(*this);
}

void Backend::visit(const ast::ConstantList &constant_list) {
    const auto &vsl_type = constant_list.type();
    VULL_ENSURE(vsl_type.scalar_type() == ast::ScalarType::Float);
    VULL_ENSURE(constant_list.size() == vsl_type.vector_size() || constant_list.size() == 1);

    // TODO(small-vector): Likely no more than a few constants in a composite.
    vull::Vector<Id> constants;
    for (const auto &vsl_constant : constant_list) {
        VULL_ENSURE(vsl_constant.scalar_type == ast::ScalarType::Float);
        auto value = static_cast<Word>(vsl_constant.literal.integer);
        constants.push(m_builder.scalar_constant(m_builder.float_type(32), value));
    }

    // Extend, for example, vec4(1.0f) to vec4(1.0f, 1.0f, 1.0f, 1.0f).
    for (uint32_t i = constant_list.size(); i < vsl_type.vector_size(); i++) {
        Id copy_constant = constants[0];
        constants.push(copy_constant);
    }

    if (constants.size() == 1) {
        m_expr_stack.push(constants[0]);
        return;
    }
    auto composite_type = m_builder.vector_type(m_builder.float_type(32), 4);
    m_expr_stack.push(m_builder.composite_constant(composite_type, vull::move(constants)));
}

void Backend::visit(const ast::Function &vsl_function) {
    m_function = &m_builder.append_function(vsl_function.name(), m_builder.void_type(),
                                            m_builder.function_type(m_builder.void_type()));
    if ((m_is_vertex_entry = (vsl_function.name() == "vertex_main"))) {
        m_builder.append_entry_point(*m_function, ExecutionModel::Vertex);

        Id position_type = m_builder.vector_type(m_builder.float_type(32), 4);
        Id position_ptr_type = m_builder.pointer_type(StorageClass::Output, position_type);
        auto &variable = m_builder.append_variable(position_ptr_type, StorageClass::Output);
        m_builder.decorate(m_position_output = variable.id(), Decoration::BuiltIn, BuiltIn::Position);
    }
    vsl_function.block().accept(*this);
}

void Backend::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.expr().accept(*this);

    // Intercept returns from the vertex shader entry point as stores to gl_Position.
    if (m_is_vertex_entry) {
        auto &store_inst = m_block->append(Op::Store);
        store_inst.append_operand(m_position_output);
        store_inst.append_operand(m_expr_stack.last());
        m_expr_stack.pop();
        m_block->append(Op::Return);
        return;
    }
    VULL_ENSURE_NOT_REACHED();
}

} // namespace spv
