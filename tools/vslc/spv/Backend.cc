#include "Backend.hh"

namespace spv {

Id Backend::convert_type(ast::ScalarType scalar_type) {
    switch (scalar_type) {
    case ast::ScalarType::Float:
        return m_builder.float_type(32);
    case ast::ScalarType::Uint:
        // TODO
        VULL_ENSURE_NOT_REACHED();
    }
}

void Backend::visit(const ast::Block &vsl_block) {
    m_block = &m_function->append_block(m_builder.make_id());
    vsl_block.traverse(*this);
}

void Backend::visit(const ast::ConstantList &constant_list) {
    // TODO(small-vector): Likely no more than a few constants in a composite.
    vull::Vector<Id> constants;
    for (const auto &vsl_constant : constant_list) {
        const auto type = convert_type(vsl_constant.scalar_type);
        const auto value = static_cast<Word>(vsl_constant.literal.integer);
        constants.push(m_builder.scalar_constant(type, value));
    }

    // Ensure that we either have exactly enough constants, or only one in which case we can extend it.
    const auto vector_size = constant_list.type().vector_size();
    VULL_ENSURE(constant_list.size() == vector_size || constant_list.size() == 1);

    // Extend, for example, vec4(1.0f) to vec4(1.0f, 1.0f, 1.0f, 1.0f).
    for (uint32_t i = constant_list.size(); i < vector_size; i++) {
        constants.push(Id(constants[0]));
    }

    // Only one constant, no need to create a composite.
    if (constants.size() == 1) {
        m_expr_stack.push(constants[0]);
        return;
    }

    // Otherwise, create a vector composite.
    const auto scalar_type = convert_type(constant_list.type().scalar_type());
    const auto composite_type = m_builder.vector_type(scalar_type, vector_size);
    m_expr_stack.push(m_builder.composite_constant(composite_type, vull::move(constants)));
}

void Backend::visit(const ast::Function &vsl_function) {
    m_function = &m_builder.append_function(vsl_function.name(), m_builder.void_type(),
                                            m_builder.function_type(m_builder.void_type()));

    // Handle special vertex shader entry point.
    m_is_vertex_entry = vsl_function.name() == "vertex_main";
    if (m_is_vertex_entry) {
        m_builder.append_entry_point(*m_function, ExecutionModel::Vertex);

        // Declare gl_Position builtin.
        Id position_type = m_builder.vector_type(m_builder.float_type(32), 4);
        Id position_ptr_type = m_builder.pointer_type(StorageClass::Output, position_type);
        auto &variable = m_builder.append_variable(position_ptr_type, StorageClass::Output);
        m_builder.decorate(variable.id(), Decoration::BuiltIn, BuiltIn::Position);
        m_position_output = variable.id();
    }
    vsl_function.block().accept(*this);
}

void Backend::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.expr().accept(*this);

    // Intercept returns from the vertex shader entry point as stores to gl_Position.
    if (m_is_vertex_entry) {
        auto &store_inst = m_block->append(Op::Store);
        store_inst.append_operand(m_position_output);
        store_inst.append_operand(m_expr_stack.take_last());
        m_block->append(Op::Return);
        return;
    }
    VULL_ENSURE_NOT_REACHED();
}

} // namespace spv
