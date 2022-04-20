#include "Backend.hh"

namespace spv {
namespace {

Op binary_op(ast::BinaryOp op) {
    switch (op) {
    case ast::BinaryOp::Add:
        return Op::FAdd;
    case ast::BinaryOp::Sub:
        return Op::FSub;
    case ast::BinaryOp::Mul:
        return Op::FMul;
    case ast::BinaryOp::Div:
        return Op::FDiv;
    case ast::BinaryOp::Mod:
        VULL_ENSURE_NOT_REACHED("% only defined for integer types");
    }
}

Op unary_op(ast::UnaryOp op) {
    switch (op) {
    case ast::UnaryOp::Negate:
        return Op::FNegate;
    }
}

} // namespace

Backend::Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Backend::Scope::~Scope() {
    m_current = m_parent;
}

const Backend::Scope::Symbol &Backend::Scope::lookup_symbol(vull::StringView name) const {
    for (const auto &symbol : m_symbol_map) {
        if (symbol.name == name) {
            return symbol;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

void Backend::Scope::put_symbol(vull::StringView name, Id id, const ast::Type &vsl_type) {
    m_symbol_map.push(Symbol{name, id, vsl_type});
}

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
    const auto vector_type = m_builder.vector_type(scalar_type, vsl_type.vector_size());
    if (vsl_type.matrix_cols() == 1) {
        return vector_type;
    }
    return m_builder.matrix_type(vector_type, vsl_type.matrix_cols());
}

Instruction &Backend::translate_construct_expr(const ast::Type &vsl_type) {
    // TODO(small-vector)
    vull::Vector<Id> arguments;
    bool is_constant = true;
    for (const auto &value : m_value_stack) {
        // Break down any composites.
        switch (value.creator_op()) {
        case Op::Constant:
            arguments.push(value.id());
            break;
        case Op::ConstantComposite:
        case Op::CompositeConstruct:
            is_constant &= value.creator_op() == Op::ConstantComposite;
            for (Id constant : value.operands()) {
                arguments.push(constant);
            }
            break;
        case Op::Load:
        case Op::FNegate:
            is_constant = false;
            if (value.vector_size() == 1) {
                arguments.push(value.id());
                break;
            }
            for (uint32_t i = 0; i < value.vector_size(); i++) {
                const auto scalar_type = convert_type(value.scalar_type());
                auto &extract_inst = m_block->append(Op::CompositeExtract, m_builder.make_id(), scalar_type);
                extract_inst.append_operand(value.id());
                extract_inst.append_operand(i);
                arguments.push(extract_inst.id());
            }
            break;
        default:
            VULL_ENSURE_NOT_REACHED();
        }
    }

    // Ensure that we either have exactly enough arguments, or only one in which case we can extend it.
    const auto vector_size = vsl_type.vector_size();
    VULL_ENSURE(arguments.size() == vector_size || arguments.size() == 1);

    // Extend, for example, vec4(1.0f) to vec4(1.0f, 1.0f, 1.0f, 1.0f).
    for (uint32_t i = arguments.size(); i < vector_size; i++) {
        arguments.push(Id(arguments.first()));
    }

    // Create a vector composite.
    const auto scalar_type = convert_type(vsl_type.scalar_type());
    const auto composite_type = m_builder.vector_type(scalar_type, vector_size);
    if (is_constant) {
        return m_builder.composite_constant(composite_type, vull::move(arguments));
    }
    auto &inst = m_block->append(Op::CompositeConstruct, m_builder.make_id(), composite_type);
    inst.extend_operands(arguments);
    return inst;
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
        auto saved_stack = vull::move(m_value_stack);
        for (const auto *node : aggregate.nodes()) {
            node->accept(*this);
        }
        auto &inst = translate_construct_expr(aggregate.type());
        m_value_stack = vull::move(saved_stack);
        m_value_stack.emplace(inst, aggregate.type());
        break;
    }
}

void Backend::visit(const ast::BinaryExpr &binary_expr) {
    binary_expr.lhs().accept(*this);
    binary_expr.rhs().accept(*this);
    auto rhs = m_value_stack.take_last();
    auto lhs = m_value_stack.take_last();
    VULL_ENSURE(lhs.scalar_type() == ast::ScalarType::Float);
    VULL_ENSURE(rhs.scalar_type() == ast::ScalarType::Float);

    Op op = binary_op(binary_expr.op());
    Id type = convert_type(lhs);
    if (lhs.is_matrix() && !rhs.is_matrix()) {
        if (rhs.is_vector()) {
            VULL_ENSURE(lhs.vector_size() == rhs.vector_size());
            op = Op::MatrixTimesVector;
            type = m_builder.vector_type(convert_type(lhs.scalar_type()), lhs.vector_size());
        }
    }

    auto &inst = m_block->append(op, m_builder.make_id(), type);
    inst.append_operand(lhs.id());
    inst.append_operand(rhs.id());
    m_value_stack.emplace(inst, lhs);
}

void Backend::visit(const ast::Constant &constant) {
    const auto type = convert_type(constant.scalar_type());
    auto &inst = m_builder.scalar_constant(type, static_cast<Word>(constant.integer()));
    m_value_stack.emplace(inst, ast::Type(constant.scalar_type(), 1, 1));
}

void Backend::visit(const ast::Function &vsl_function) {
    Scope scope(m_scope);

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

            const auto &parameter = vsl_function.parameters()[i];
            m_scope->put_symbol(parameter.name(), variable.id(), parameter.type());
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
    auto expr_value = m_value_stack.take_last();
    if (m_is_vertex_entry) {
        // Intercept returns from the vertex shader entry point as stores to gl_Position.
        auto &store_inst = m_block->append(Op::Store);
        store_inst.append_operand(m_position_output);
        store_inst.append_operand(expr_value.id());
        m_block->append(Op::Return);
        return;
    }
    auto &return_inst = m_block->append(Op::ReturnValue);
    return_inst.append_operand(expr_value.id());
}

void Backend::visit(const ast::Symbol &vsl_symbol) {
    const auto &symbol = m_scope->lookup_symbol(vsl_symbol.name());
    auto &load_inst = m_block->append(Op::Load, m_builder.make_id(), convert_type(symbol.vsl_type));
    load_inst.append_operand(symbol.id);
    m_value_stack.emplace(load_inst, symbol.vsl_type);
}

void Backend::visit(const ast::UnaryExpr &unary_expr) {
    unary_expr.expr().accept(*this);
    auto expr_value = m_value_stack.take_last();
    VULL_ENSURE(expr_value.scalar_type() == ast::ScalarType::Float);

    auto &inst = m_block->append(unary_op(unary_expr.op()), m_builder.make_id(), convert_type(expr_value));
    inst.append_operand(expr_value.id());
    m_value_stack.emplace(inst, expr_value);
}

} // namespace spv
