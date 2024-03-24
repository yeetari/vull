#include "backend.hh"

#include "../ast.hh"
#include "../type.hh"

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace spv {
namespace {

Op binary_op(ast::BinaryOp op) {
    switch (op) {
    case ast::BinaryOp::Add:
    case ast::BinaryOp::AddAssign:
        return Op::FAdd;
    case ast::BinaryOp::Sub:
    case ast::BinaryOp::SubAssign:
        return Op::FSub;
    case ast::BinaryOp::Mul:
    case ast::BinaryOp::MulAssign:
        return Op::FMul;
    case ast::BinaryOp::Div:
    case ast::BinaryOp::DivAssign:
        return Op::FDiv;
    case ast::BinaryOp::Mod:
        VULL_ENSURE_NOT_REACHED("% only defined for integer types");
    case ast::BinaryOp::Assign:
        return Op::Store;
    case ast::BinaryOp::VectorTimesScalar:
        return Op::VectorTimesScalar;
    case ast::BinaryOp::MatrixTimesScalar:
        return Op::MatrixTimesScalar;
    case ast::BinaryOp::VectorTimesMatrix:
        return Op::VectorTimesMatrix;
    case ast::BinaryOp::MatrixTimesVector:
        return Op::MatrixTimesVector;
    case ast::BinaryOp::MatrixTimesMatrix:
        return Op::MatrixTimesMatrix;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

Op unary_op(ast::UnaryOp op) {
    switch (op) {
    case ast::UnaryOp::Negate:
        return Op::FNegate;
    }
    VULL_ENSURE_NOT_REACHED();
}

} // namespace

Instruction &Backend::translate_construct_expr(const Type &vsl_type) {
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
        default:
            is_constant = false;
            if (value.vector_size() == 1) {
                arguments.push(value.id());
                break;
            }
            for (uint32_t i = 0; i < value.vector_size(); i++) {
                const auto scalar_type = convert_type(value.scalar_type());
                auto &extract_inst = m_block->append(Op::CompositeExtract, scalar_type);
                extract_inst.append_operand(value.id());
                extract_inst.append_operand(i);
                arguments.push(extract_inst.id());
            }
            break;
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
    auto &inst = m_block->append(Op::CompositeConstruct, composite_type);
    inst.extend_operands(arguments);
    return inst;
}

void Backend::visit(ast::Aggregate &aggregate) {
    switch (aggregate.kind()) {
    case ast::AggregateKind::Block:
        m_block = &m_function->append_block();
        for (auto *stmt : aggregate.nodes()) {
            stmt->traverse(*this);
        }
        break;
    case ast::AggregateKind::ConstructExpr: {
        auto saved_stack = vull::move(m_value_stack);
        for (auto *node : aggregate.nodes()) {
            node->traverse(*this);
        }
        auto &inst = translate_construct_expr(aggregate.type());
        m_value_stack = vull::move(saved_stack);
        m_value_stack.emplace(inst, aggregate.type());
        break;
    }
    case ast::AggregateKind::UniformBlock: {
        vull::Vector<Id> member_types;
        member_types.ensure_capacity(aggregate.nodes().size());
        for (auto *node : aggregate.nodes()) {
            member_types.push(convert_type(node->type()));
        }

        const auto struct_type = m_builder.struct_type(member_types, true);
        auto &variable = m_builder.append_variable(struct_type, StorageClass::PushConstant);
        for (uint8_t i = 0; auto *node : aggregate.nodes()) {
            auto &symbol = m_scope->put_symbol(static_cast<ast::Symbol *>(node)->name(), variable.id());
            symbol.uniform_index = i;
        }
        break;
    }
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

void Backend::visit(ast::DeclStmt &decl_stmt) {
    decl_stmt.value().traverse(*this);

    const auto value = m_value_stack.take_last();
    auto &variable = m_function->append_variable(convert_type(value));
    if (value.creator_op() == Op::Constant || value.creator_op() == Op::ConstantComposite) {
        variable.append_operand(value.id());
    } else {
        auto &store_inst = m_block->append(Op::Store);
        store_inst.append_operand(variable.id());
        store_inst.append_operand(value.id());
    }
    m_scope->put_symbol(decl_stmt.name(), variable.id());
}

void Backend::visit(ast::Function &vsl_function) {
    Scope scope(m_scope);

    vull::Vector<Id> parameter_types;
    parameter_types.ensure_capacity(vsl_function.parameters().size());
    for (const auto &parameter : vsl_function.parameters()) {
        parameter_types.push(convert_type(parameter.type()));
    }

    bool is_vertex_entry = vsl_function.name() == "vertex_main";
    m_is_fragment_entry = vsl_function.name() == "fragment_main";

    spv::Id return_type;
    spv::Id function_type;
    if (is_vertex_entry || m_is_fragment_entry) {
        return_type = m_builder.void_type();
        function_type = m_builder.function_type(return_type, {});
    } else {
        return_type = convert_type(vsl_function.return_type());
        function_type = m_builder.function_type(return_type, parameter_types);
    }

    m_function = &m_builder.append_function(vsl_function.name(), return_type, function_type);
    if (is_vertex_entry) {
        auto &entry_point = m_builder.append_entry_point(*m_function, ExecutionModel::Vertex);

        // Create vertex inputs.
        for (uint32_t i = 0; i < parameter_types.size(); i++) {
            const auto input_type = parameter_types[i];
            auto &variable = entry_point.append_variable(input_type, StorageClass::Input);
            m_builder.decorate(variable.id(), Decoration::Location, i);

            const auto &parameter = vsl_function.parameters()[i];
            m_scope->put_symbol(parameter.name(), variable.id());
        }

        // Create gl_Position builtin.
        const auto position_type = m_builder.vector_type(m_builder.float_type(32), 4);
        auto &position_variable = entry_point.append_variable(position_type, StorageClass::Output);
        m_builder.decorate(position_variable.id(), Decoration::BuiltIn, BuiltIn::Position);
        m_scope->put_symbol("gl_Position", position_variable.id());

        // Create pipeline outputs.
        for (uint32_t i = 0; i < m_pipeline_decls.size(); i++) {
            const ast::PipelineDecl &pipeline_decl = m_pipeline_decls[i];
            const auto type = convert_type(pipeline_decl.type());
            auto &variable = entry_point.append_variable(type, StorageClass::Output);
            m_builder.decorate(variable.id(), Decoration::Location, i);
            m_scope->put_symbol(pipeline_decl.name(), variable.id());
        }
    } else if (m_is_fragment_entry) {
        auto &entry_point = m_builder.append_entry_point(*m_function, ExecutionModel::Fragment);

        // Create pipeline inputs.
        for (uint32_t i = 0; i < m_pipeline_decls.size(); i++) {
            const ast::PipelineDecl &pipeline_decl = m_pipeline_decls[i];
            const auto type = convert_type(pipeline_decl.type());
            auto &variable = entry_point.append_variable(type, StorageClass::Input);
            m_builder.decorate(variable.id(), Decoration::Location, i);
            m_scope->put_symbol(pipeline_decl.name(), variable.id());
        }

        // Create output.
        auto &out_variable =
            entry_point.append_variable(convert_type(vsl_function.return_type()), StorageClass::Output);
        m_builder.decorate(out_variable.id(), Decoration::Location, 0);
        m_fragment_output_id = out_variable.id();
    }

    vsl_function.block().traverse(*this);
    if (!m_block->is_terminated()) {
        m_block->append(Op::Return);
    }
}

void Backend::visit(ast::PipelineDecl &pipeline_decl) {
    m_pipeline_decls.push(pipeline_decl);
}

void Backend::visit(ast::Symbol &ast_symbol) {
    const auto type = convert_type(ast_symbol.type());
    const auto &symbol = m_scope->lookup_symbol(ast_symbol.name());
    Id var_id = symbol.id;
    if (symbol.uniform_index) {
        auto &index_constant = m_builder.scalar_constant(m_builder.int_type(32, false), *symbol.uniform_index);
        auto &access_chain = m_block->append(Op::AccessChain, m_builder.pointer_type(StorageClass::PushConstant, type));
        access_chain.append_operand(var_id);
        access_chain.append_operand(index_constant.id());
        var_id = access_chain.id();
    }

    if (!m_load_symbol) {
        m_load_symbol = true;
        m_value_stack.emplace(var_id, ast_symbol.type());
        return;
    }

    auto &load_inst = m_block->append(Op::Load, type);
    load_inst.append_operand(var_id);
    m_value_stack.emplace(load_inst, ast_symbol.type());
}

} // namespace spv
