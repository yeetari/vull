#include "legaliser.hh"

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/string_view.hh>

using namespace ast;

Legaliser::Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Legaliser::Scope::~Scope() {
    m_current = m_parent;
}

Type Legaliser::Scope::lookup_symbol(vull::StringView name) const {
    if (auto type = m_symbol_map.get(name)) {
        return *type;
    }
    if (m_parent == nullptr) {
        return ScalarType::Invalid;
    }
    return m_parent->lookup_symbol(name);
}

void Legaliser::Scope::put_symbol(vull::StringView name, Type type) {
    m_symbol_map.set(name, type);
}

void Legaliser::visit(Aggregate &aggregate) {
    if (aggregate.kind() == AggregateKind::UniformBlock) {
        for (auto *node : aggregate.nodes()) {
            auto *symbol = static_cast<Symbol *>(node);
            m_scope->put_symbol(symbol->name(), symbol->type());
        }
        return;
    }
    for (auto *node : aggregate.nodes()) {
        node->traverse(*this);
    }
}

void Legaliser::visit(BinaryExpr &binary_expr) {
    const auto lhs = binary_expr.lhs().type();
    const auto rhs = binary_expr.rhs().type();

    if (ast::is_assign_op(binary_expr.op())) {
        binary_expr.set_type(lhs);
        return;
    }

    const auto scalar_type = lhs.scalar_type();
    if ((lhs.is_vector() && rhs.is_scalar()) || (lhs.is_scalar() && rhs.is_vector())) {
        binary_expr.set_op(BinaryOp::VectorTimesScalar);
        binary_expr.set_type(lhs.is_vector() ? lhs : rhs);
    } else if ((lhs.is_matrix() && rhs.is_scalar()) || (lhs.is_scalar() && rhs.is_matrix())) {
        binary_expr.set_op(BinaryOp::MatrixTimesScalar);
        binary_expr.set_type(lhs.is_matrix() ? lhs : rhs);
    } else if (lhs.is_vector() && rhs.is_matrix()) {
        binary_expr.set_op(BinaryOp::VectorTimesMatrix);
        binary_expr.set_type(Type(scalar_type, rhs.matrix_cols()));
    } else if (lhs.is_matrix() && rhs.is_vector()) {
        binary_expr.set_op(BinaryOp::MatrixTimesVector);
        binary_expr.set_type(Type(scalar_type, lhs.matrix_rows()));
    } else if (lhs.is_matrix() && rhs.is_matrix()) {
        binary_expr.set_op(BinaryOp::MatrixTimesMatrix);
        binary_expr.set_type(Type(scalar_type, rhs.matrix_cols(), lhs.matrix_rows()));
    } else {
        VULL_ASSERT((lhs.is_scalar() && rhs.is_scalar()) || (lhs.is_vector() && rhs.is_vector()));
        VULL_ASSERT(lhs.vector_size() == rhs.vector_size());
        binary_expr.set_type(Type(scalar_type, lhs.vector_size()));
    }
}

void Legaliser::visit(CallExpr &call_expr) {
    for (auto *argument : call_expr.arguments()) {
        argument->traverse(*this);
    }
    if (call_expr.name() == "dot") {
        call_expr.set_type(ScalarType::Float);
        return;
    }
    if (call_expr.name() == "max") {
        call_expr.set_type(call_expr.arguments().first()->type());
        return;
    }
}

void Legaliser::visit(DeclStmt &decl_stmt) {
    m_scope->put_symbol(decl_stmt.name(), decl_stmt.value().type());
}

void Legaliser::visit(Function &function) {
    Scope scope(m_scope);
    if (function.name() == "vertex_main") {
        m_scope->put_symbol("gl_Position", Type(ScalarType::Float, 4));
    }
    for (const auto &parameter : function.parameters()) {
        m_scope->put_symbol(parameter.name(), parameter.type());
    }
    function.block().traverse(*this);
}

void Legaliser::visit(PipelineDecl &pipeline_decl) {
    m_scope->put_symbol(pipeline_decl.name(), pipeline_decl.type());
}

void Legaliser::visit(Symbol &symbol) {
    symbol.set_type(m_scope->lookup_symbol(symbol.name()));
}

void Legaliser::visit(UnaryExpr &unary_expr) {
    unary_expr.set_type(unary_expr.expr().type());
}
