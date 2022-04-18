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

Id Backend::convert_type(const ast::Type &vsl_type) {
    const auto scalar_type = convert_type(vsl_type.scalar_type());
    if (vsl_type.vector_size() == 1) {
        return scalar_type;
    }
    return m_builder.vector_type(scalar_type, vsl_type.vector_size());
}

Instruction &Backend::translate_construct_expr(const ast::Type &vsl_type) {
    // TODO(small-vector)
    vull::Vector<Id> constants;
    for (const Instruction &inst : m_expr_stack) {
        switch (inst.op()) {
        case Op::Constant:
            constants.push(inst.id());
            break;
        case Op::ConstantComposite:
            // Break down composites.
            for (Id constant : inst.operands()) {
                constants.push(constant);
            }
            break;
        default:
            VULL_ENSURE_NOT_REACHED();
        }
    }
    m_expr_stack.clear();

    // Ensure that we either have exactly enough arguments, or only one in which case we can extend it.
    const auto vector_size = vsl_type.vector_size();
    VULL_ENSURE(constants.size() == vector_size || constants.size() == 1);

    // Extend, for example, vec4(1.0f) to vec4(1.0f, 1.0f, 1.0f, 1.0f).
    for (uint32_t i = constants.size(); i < vector_size; i++) {
        constants.push(Id(constants.first()));
    }

    // Create a vector composite.
    const auto scalar_type = convert_type(vsl_type.scalar_type());
    const auto composite_type = m_builder.vector_type(scalar_type, vector_size);
    return m_builder.composite_constant(composite_type, vull::move(constants));
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
        auto saved_stack = vull::move(m_expr_stack);
        for (const auto *node : aggregate.nodes()) {
            node->accept(*this);
        }
        auto &inst = translate_construct_expr(aggregate.type());
        m_expr_stack = vull::move(saved_stack);
        m_expr_stack.push(inst);
        break;
    }
}

void Backend::visit(const ast::Constant &constant) {
    const auto type = convert_type(constant.scalar_type());
    m_expr_stack.push(m_builder.scalar_constant(type, static_cast<Word>(constant.integer())));
}

void Backend::visit(const ast::Function &vsl_function) {
    vull::Vector<Id> parameter_types;
    parameter_types.ensure_capacity(vsl_function.parameters().size());
    for (const auto &parameter : vsl_function.parameters()) {
        parameter_types.push(convert_type(parameter.type()));
    }

    auto return_type = convert_type(vsl_function.return_type());
    auto function_type = m_builder.function_type(return_type, parameter_types);
    if ((m_is_vertex_entry = vsl_function.name() == "vertex_main")) {
        return_type = m_builder.void_type();
        function_type = m_builder.function_type(return_type, {});
    }
    m_function = &m_builder.append_function(vsl_function.name(), return_type, function_type);
    if (m_is_vertex_entry) {
        VULL_ENSURE(vsl_function.return_type().scalar_type() == ast::ScalarType::Float);
        VULL_ENSURE(vsl_function.return_type().vector_size() == 4);
        m_builder.append_entry_point(*m_function, ExecutionModel::Vertex);

        // Create inputs.
        for (uint32_t i = 0; i < parameter_types.size(); i++) {
            const auto input_type = parameter_types[i];
            const auto input_ptr_type = m_builder.pointer_type(StorageClass::Input, input_type);
            auto &variable = m_builder.append_variable(input_ptr_type, StorageClass::Input);
            m_builder.decorate(variable.id(), Decoration::Location, i);
        }

        // Create gl_Position builtin.
        const auto position_type = m_builder.vector_type(m_builder.float_type(32), 4);
        const auto position_ptr_type = m_builder.pointer_type(StorageClass::Output, position_type);
        auto &variable = m_builder.append_variable(position_ptr_type, StorageClass::Output);
        m_builder.decorate(variable.id(), Decoration::BuiltIn, BuiltIn::Position);
        m_position_output = variable.id();
    }
    vsl_function.block().accept(*this);
}

void Backend::visit(const ast::ReturnStmt &return_stmt) {
    return_stmt.expr().accept(*this);
    const Instruction &expr_inst = m_expr_stack.take_last();
    if (m_is_vertex_entry) {
        // Intercept returns from the vertex shader entry point as stores to gl_Position.
        auto &store_inst = m_block->append(Op::Store);
        store_inst.append_operand(m_position_output);
        store_inst.append_operand(expr_inst.id());
        m_block->append(Op::Return);
        return;
    }
    auto &return_inst = m_block->append(Op::ReturnValue);
    return_inst.append_operand(expr_inst.id());
}

} // namespace spv
