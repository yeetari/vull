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

void Backend::translate_construct_expr(const ast::Type &vsl_type) {
    // Ensure that we either have exactly enough arguments, or only one in which case we can extend it.
    const auto vector_size = vsl_type.vector_size();
    VULL_ENSURE(m_expr_stack.size() == vector_size || m_expr_stack.size() == 1);

    // Extend, for example, vec4(1.0f) to vec4(1.0f, 1.0f, 1.0f, 1.0f).
    for (uint32_t i = m_expr_stack.size(); i < vector_size; i++) {
        m_expr_stack.push(Id(m_expr_stack.first()));
    }

    // Already only one value, no need to create a composite.
    if (m_expr_stack.size() == 1) {
        return;
    }

    // Otherwise, create a vector composite.
    const auto scalar_type = convert_type(vsl_type.scalar_type());
    const auto composite_type = m_builder.vector_type(scalar_type, vector_size);
    m_expr_stack.push(m_builder.composite_constant(composite_type, vull::move(m_expr_stack)));
}

void Backend::visit(const ast::Aggregate &aggregate) {
    switch (aggregate.kind()) {
    case ast::AggregateKind::Block:
        m_block = &m_function->append_block(m_builder.make_id());
        for (const auto *stmt : aggregate.nodes()) {
            stmt->accept(*this);
        }
        break;
    case ast::AggregateKind::ConstructExpr:
        for (const auto *node : aggregate.nodes()) {
            node->accept(*this);
        }
        translate_construct_expr(aggregate.type());
        break;
    }
}

void Backend::visit(const ast::Constant &constant) {
    const auto type = convert_type(constant.scalar_type());
    m_expr_stack.push(m_builder.scalar_constant(type, static_cast<Word>(constant.integer())));
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
